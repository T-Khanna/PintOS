#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"

/* Functions required to use hash table */
unsigned swap_table_hash_func(const struct hash_elem *elem, void *aux UNUSED);
bool swap_table_less_func(const struct hash_elem *a, const struct hash_elem *b,
    void *aux UNUSED);

static struct block *swap_dev;     /* The swap device */
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
