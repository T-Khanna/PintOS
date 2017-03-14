#include "vm/frame.h"
#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

static unsigned frame_hash_func(const struct hash_elem *e, void *aux);
static bool frame_hash_less(const struct hash_elem *e1,
     const struct hash_elem *e2, void *aux);
static void frame_access_lock(void);
static void frame_access_unlock(void);
static void frame_evict(void);
struct hash hash_table;
struct lock frame_lock;

void frame_init (void)
{
    lock_init(&frame_lock);
    hash_init(&hash_table, frame_hash_func, frame_hash_less, NULL);
}

void frame_access_lock()
{
    lock_acquire(&frame_lock);
}

void frame_access_unlock()
{
    lock_release(&frame_lock);
}

void* frame_get_page(void *upage)
{
    void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);

    if (kpage == NULL) {
       frame_evict();
       // TODO some more things probably
    }
    struct frame *new_frame = (struct frame *) malloc(sizeof(struct frame));
    new_frame->t = thread_current();
    new_frame->kaddr = kpage;
    new_frame->uaddr = upage;

    frame_access_lock();

    struct hash_elem *success = hash_insert(&hash_table, &new_frame->hash_elem);
    supp_page_table_insert(&new_frame->t->supp_page_table, upage, LOADED);

    frame_access_unlock();

    if (success != NULL) {
        //TODO
        //PANIC("Something has gone terribly wrong!\n");
    }

    return kpage;

}

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

static void frame_evict(void)
{
    PANIC ("ERRORRRR\n");
    // YOUR EVICTION CODE HERE FROM ONLY £50/WEEK CALL NOW
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
