#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"

/* Functions required to use hash table */
unsigned swap_table_hash_func(const struct hash_elem *elem, void *aux UNUSED);
bool swap_table_less_func(const struct hash_elem *a, const struct hash_elem *b,
    void *aux UNUSED);
static swap_index_t next_free_slot(void);

static struct block *swap_dev;     /* The swap device */
/* Note: assumes that page size is a multiple of sector size (and larger) */
static const int sectors_per_page = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t num_slots;           /* Total number of swap slots */
static struct bitmap *slot_usage;  /* Bitmap to represent usage of slots */

/* Initialise the swap system */
void swap_init(void)
{
  swap_dev = block_get_role(BLOCK_SWAP);
  num_slots = block_size(swap_dev) / sectors_per_page;
  slot_usage = bitmap_create(num_slots);
}

/* Deinitialise the swap system */
void swap_destroy(void)
{
  bitmap_destroy(slot_usage);
}

/* Initialise a swap table */
void swap_table_init(struct hash *table)
{
  hash_init(table, &swap_table_hash_func, &swap_table_less_func, NULL);
}

/* Destroy a swap table */
void swap_table_destroy(struct hash *table)
{
  //TODO might need to free swap table entries as an action func?
  hash_destroy(table, NULL);
}

/* Use vaddr as the key, and just call hash_bytes on it */
unsigned swap_table_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct swap_table_entry *entry
      = hash_entry(elem, struct swap_table_entry, elem);
  return hash_bytes(&entry->vaddr, sizeof(void *));
}

/* Use vaddr as the key and just compare those */
bool swap_table_less_func(const struct hash_elem *a, const struct hash_elem *b,
    void *aux UNUSED)
{
    struct swap_table_entry *a_entry
        = hash_entry(a, struct swap_table_entry, elem);
    struct swap_table_entry *b_entry
        = hash_entry(b, struct swap_table_entry, elem);
    return a_entry->vaddr < b_entry->vaddr;
}

/* Get the swap slot index of the next free swap slot.
   Returns BITMAP_ERROR if there is no available swap */
static swap_index_t next_free_slot(void)
{
  /* Scan the entire bitmap for an unmarked slot */
  return bitmap_scan(slot_usage, 0, num_slots - 1, false);
}

/* Moves the swap block for vaddr in the swap table into a frame given by kaddr.
   Returns false if the swap table entry for vaddr could not be found, or true
   if the operation succeeded. */
bool swap_into_memory(struct hash *table, void *vaddr, void *kaddr)
{
  /* check that kaddr and vaddr are valid. */
  ASSERT(is_kernel_vaddr(kaddr));
  ASSERT(is_user_vaddr(vaddr));

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

  for (int i = 0; i < sectors_per_page; i++) {
    /* Read a single sector */
    block_read(swap_dev, (index * sectors_per_page + i),
        kaddr + BLOCK_SECTOR_SIZE);
  }

  return true;
}
