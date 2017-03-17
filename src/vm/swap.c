#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Functions required to use hash table */
static unsigned swap_table_hash_func(const struct hash_elem *elem,
    void *aux UNUSED);
static bool swap_table_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED);
static void delete_swap_table_entry(struct hash_elem *elem, void *aux UNUSED);
static swap_index_t allocate_slot(void);

static struct block *swap_dev;     /* The swap device */
/* Note: assumes that page size is a multiple of sector size (and larger) */
static const int sectors_per_page = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t num_slots;           /* Total number of swap slots */
static struct bitmap *slot_usage;  /* Bitmap to represent usage of slots */
struct lock swap_lock;

/* Initialise the swap system */
void swap_init(void)
{
  swap_dev = block_get_role(BLOCK_SWAP);
  num_slots = block_size(swap_dev) / sectors_per_page;
  slot_usage = bitmap_create(num_slots);
  lock_init(&swap_lock);
}

/* Deinitialise the swap system */
void swap_destroy(void)
{
  bitmap_destroy(slot_usage);
}

/* Initialise a swap table */
void swap_table_init(struct hash *table)
{
  hash_init(table, swap_table_hash_func, swap_table_less_func, NULL);
}

/* Destroy a swap table */
void swap_table_destroy(struct hash *table)
{
  hash_destroy(table, &delete_swap_table_entry);
}

/* Use vaddr as the key, and just call hash_bytes on it */
static unsigned swap_table_hash_func(const struct hash_elem *elem,
    void *aux UNUSED)
{
  struct swap_table_entry *entry
      = hash_entry(elem, struct swap_table_entry, elem);
  return hash_bytes(&entry->vaddr, sizeof(entry->vaddr));
}

/* Use vaddr as the key and just compare those */
static bool swap_table_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED)
{
    struct swap_table_entry *a_entry
        = hash_entry(a, struct swap_table_entry, elem);
    struct swap_table_entry *b_entry
        = hash_entry(b, struct swap_table_entry, elem);
    return a_entry->vaddr < b_entry->vaddr;
}

/* Deallocate memory for a swap table entry */
static void delete_swap_table_entry(struct hash_elem *elem, void *aux UNUSED)
{
  struct swap_table_entry *e = hash_entry(elem, struct swap_table_entry, elem);
  bitmap_reset(slot_usage, e->index);
  free(e);
}

/* Allocate a swap slot and mark it as in use.
   Returns BITMAP_ERROR if there is no available swap */
static swap_index_t allocate_slot(void)
{
  /* Scan the entire bitmap for an unmarked slot and mark it. */
  return bitmap_scan_and_flip(slot_usage, 0, 1, false);
}

/* Moves the swap block for vaddr in the swap table into a frame given by kaddr.
   Returns false if the swap table entry for vaddr could not be found, or true
   if the operation succeeded. */
bool swap_into_memory(struct hash *table, void *vaddr, void *kaddr)
{
  /* check that kaddr and vaddr are valid. */
  ASSERT(is_kernel_vaddr(kaddr));
  ASSERT(is_user_vaddr(vaddr));

  lock_acquire(&swap_lock);

  /* Find the entry for vaddr in the swap table. */
  struct swap_table_entry search_entry;
  search_entry.vaddr = vaddr;
  struct hash_elem *found = hash_find(table, &search_entry.elem);

  if (found == NULL) {
    return false;
  }

  swap_index_t index = hash_entry(found, struct swap_table_entry, elem)->index;
  /* check that there's actually a page at swap index found in table */
  ASSERT(bitmap_test(slot_usage, index));

  /* Read the slot into the frame */
  for (int i = 0; i < sectors_per_page; i++) {
    block_read(swap_dev, (index * sectors_per_page + i),
        kaddr + BLOCK_SECTOR_SIZE * i);
  }

  /* Clean up */
  struct hash_elem *elem = hash_delete(table, found);
  if (elem != NULL) {
    delete_swap_table_entry(found, NULL);
  }
  bitmap_reset(slot_usage, index);

  lock_release(&swap_lock);

  return true;
}

/* Swap to disk the frame at kaddr, (representing user address vaddr), and
   record this in the swap table passed.
   Panics the kernel if we're out of swap space */
void swap_to_disk(struct hash *table, void *vaddr, void *kaddr)
{
  /* check that kaddr and vaddr are valid */
  ASSERT(is_kernel_vaddr(kaddr));
  ASSERT(is_user_vaddr(vaddr));

  lock_acquire(&swap_lock);

  swap_index_t slot_index = allocate_slot();
  if (slot_index == BITMAP_ERROR) {
    /* Out of swap space */
    PANIC("Oh dear, we've run out of swap space.");
  }

  /* Write out to disk at the allocated index */
  for (int i = 0; i < sectors_per_page; i++) {
    block_write(swap_dev, (slot_index * sectors_per_page + i),
        kaddr + BLOCK_SECTOR_SIZE * i);
  }

  /* Make a swap table entry and add it to the swap table */
  struct swap_table_entry *entry
      = (struct swap_table_entry *) malloc(sizeof(struct swap_table_entry));
  ASSERT(entry != NULL);
  entry->vaddr = vaddr;
  entry->index = slot_index;
  /* If hash_insert returned non-null, there was already an entry in the swap
     table for that page. */
  struct hash_elem* inserted = hash_insert(table, &entry->elem);
  ASSERT(inserted == NULL);

  lock_release(&swap_lock);
}
