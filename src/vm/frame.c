#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "vm/swap.h"
#include "vm/mmap.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"


static unsigned frame_hash_func(const struct hash_elem *e, void *aux);
static bool frame_hash_less(const struct hash_elem *e1,
     const struct hash_elem *e2, void *aux);
static void frame_access_lock(void);
static void frame_access_unlock(void);
static struct frame * choose_victim(void);
static void frame_evict(struct frame *victim);

struct hash hash_table;
struct lock frame_lock;
static struct hash_iterator clock_pos;

void frame_init (void)
{
    lock_init(&frame_lock);
    hash_init(&hash_table, &frame_hash_func, &frame_hash_less, NULL);
}

void frame_access_lock()
{
    lock_acquire(&frame_lock);
}

void frame_access_unlock()
{
    lock_release(&frame_lock);
}

static void print_frame(struct hash_elem* hash_elem, void* aux UNUSED);

void print_frame_table(void) {
  hash_apply(&hash_table, print_frame);
}

static void print_frame(struct hash_elem* elem, void* aux UNUSED) {
  struct frame* found = hash_entry(elem, struct frame, hash_elem);
  printf("FOR THREAD %s WITH TID %d, KERNEL VIRTUAL ADDRESS IS: %p, USER ADDRESS IS %p\n",
      found->t->name, found->t->tid, found->kaddr, found->uaddr);
}

/* Get a frame of memory for the current thread */
void* frame_get_page(void *upage)
{
    ASSERT(is_user_vaddr(upage));
    void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);

    if (kpage == NULL) {
      /* make space and try again */
      frame_evict(choose_victim());
      kpage = palloc_get_page(PAL_USER | PAL_ZERO);
      ASSERT(kpage != NULL);
    }
    struct frame *new_frame = (struct frame *) malloc(sizeof(struct frame));

    if (new_frame == NULL) {
        PANIC("Cannot malloc");
    }

    new_frame->t = thread_current();
    new_frame->kaddr = kpage;
    new_frame->uaddr = upage;

    frame_access_lock();

    struct hash_elem *success = hash_insert(&hash_table, &new_frame->hash_elem);
    frame_access_unlock();

    if (success != NULL) {
        //TODO
        //PANIC("Something has gone terribly wrong!\n");
    }

    return kpage;

}

/* Remove a frame from the frame table, and give up the frame */
void frame_free_page(void *kaddr)
{
    struct frame f;
    f.kaddr = kaddr;

    frame_access_lock();

    struct hash_elem *del_elem = hash_delete(&hash_table, &f.hash_elem);
    ASSERT(del_elem != NULL);
    struct frame *del_frame = hash_entry(del_elem, struct frame, hash_elem);

    // Frees the page and removes its reference
    pagedir_clear_page(del_frame->t->pagedir, del_frame->uaddr);

    palloc_free_page(del_frame->kaddr);
    free(del_frame);

    frame_access_unlock();
}

/* Choose a frame as a candidate for eviction. */
static struct frame * choose_victim(void)
{
  ASSERT(!hash_empty(&hash_table));

  struct frame *victim = NULL;

  frame_access_lock();

  struct hash_iterator iter;
  hash_first(&iter, &hash_table);
  while (hash_next(&iter)) {
    struct frame *cur = hash_entry(hash_cur(&iter), struct frame, hash_elem);
    victim = cur;
  }

  frame_access_unlock();
  /* make sure we found a victim */
  ASSERT(victim != NULL);
  return victim;
}

/* clear the given frame of memory */
bool clear_frame(void *kaddr) {
  ASSERT(is_kernel_vaddr(kaddr));
  struct frame target;
  target.kaddr = kaddr;
  struct hash_elem *elem = hash_find(&hash_table, &target.hash_elem);
  if (elem == NULL) {
    return false;
  }
  struct frame *found = hash_entry(elem, struct frame, hash_elem);
  frame_evict(found);
  return true;
}

/* Evict a frame from memory, taking appropriate action to write it out to the
   backing store (either swap or a file). Also frees it's frame table entry. */
static void frame_evict(struct frame *victim)
{
  struct supp_page *spte = supp_page_table_get(&victim->t->supp_page_table,
      victim->uaddr);
  switch (spte->status) {
    case LOADED:
      /* normal memory, swap out to disk */
      swap_to_disk(&victim->t->swap_table, victim->uaddr, victim->kaddr);
      spte->status = SWAPPED;
      break;
    case MMAPPED:
      /* write the frame back to disk, if it has been modified */
      if (pagedir_is_dirty(victim->t->pagedir, victim->uaddr)) {
        struct mmap_file_page *mmfp = mmap_file_page_table_get(
            &victim->t->mmap_file_page_table, victim->uaddr);
        lock_filesys_access();
        file_seek(mmfp->file, mmfp->ofs);
        file_write(mmfp->file, victim->kaddr, mmfp->size);
        unlock_filesys_access();
      }
      break;
    case SWAPPED:
    case ZEROED:
    default:
      /* These types of pages shouldn't be in a frame, panic the kernel */
      printf("SPT STATUS: %d\n", spte->status);
      print_spt(&victim->t->supp_page_table);
      PANIC("Bad type of page in memory!");
      NOT_REACHED();
  }

  /* remove and free the frame table entry */
  frame_free_page(victim->kaddr);
}

unsigned frame_hash_func(const struct hash_elem *e_, void *aux UNUSED)
{
    struct frame *e = hash_entry(e_, struct frame, hash_elem);
    return hash_int((int32_t) e->kaddr);
}

bool frame_hash_less(const struct hash_elem *e1_,
    const struct hash_elem *e2_, void *aux UNUSED)
{
    struct frame *e1 = hash_entry(e1_, struct frame, hash_elem);
    struct frame *e2 = hash_entry(e2_, struct frame, hash_elem);
    return e1->kaddr < e2->kaddr;
}
