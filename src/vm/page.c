#include "vm/page.h"
#include "threads/malloc.h"

unsigned supp_pte_hash_func(const struct hash_elem *elem,
    void *aux UNUSED);
bool supp_pte_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED);
void delete_supp_pte(struct hash_elem *elem, void *aux UNUSED);

/* Initialises a supplementary page table, and returns whether it was successful
   in doing so */
bool supp_page_table_init(struct hash *table)
{
  return hash_init(table, &supp_pte_hash_func, &supp_pte_less_func, NULL);
}

/* Destroy all of the elements of a supplementary page table. */
void supp_page_table_destroy(struct hash *table)
{
  hash_destroy(table, &delete_supp_pte);
}

/* Returns the supplementary page table entry corresponding to the page vaddr
   in hash if it exists, or null otherwise */
struct supp_page_table_entry * supp_page_table_get(struct hash *hash,
    void *vaddr)
{
  return hash_entry(hash_find(hash, vaddr), struct supp_page_table_entry,
      hash_elem);
}

/* Inserts the page vaddr into the supplemtary page table unless an entry for it
   already exists, in which case it is returned without modifying hash.
   vaddr should point to the start of the page, as returned by pg_round_down */
struct supp_page_table_entry * supp_page_table_insert(struct hash *hash,
    void *vaddr, enum page_status_t status)
{
  struct supp_page_table_entry *entry
      = malloc(sizeof(struct supp_page_table_entry));
  ASSERT(entry != NULL);
  entry->vaddr = vaddr;
  entry->status = status;
  struct hash_elem *prev = hash_insert(hash, &entry->hash_elem);

  return (prev == NULL) ? NULL :
      hash_entry(prev, struct supp_page_table_entry, hash_elem);
}

/* function for hashing the page pointer in a supplementary page table entry */
unsigned supp_pte_hash_func(const struct hash_elem *elem,
    void *aux UNUSED)
{
  struct supp_page_table_entry *entry
      = hash_entry(elem, struct supp_page_table_entry, hash_elem);
  return hash_bytes(entry->vaddr, sizeof(void *));
}

/* function for comparing the addresses of two supplemtary page table entries */
bool supp_pte_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED)
{
  struct supp_page_table_entry *a_entry
      = hash_entry(a, struct supp_page_table_entry, hash_elem);
  struct supp_page_table_entry *b_entry
      = hash_entry(b, struct supp_page_table_entry, hash_elem);
  return a_entry->vaddr < b_entry->vaddr;
}

/* Free the memory used by an entry in the supplementary page table */
void delete_supp_pte(struct hash_elem *elem, void *aux UNUSED)
{
  struct supp_page_table_entry *entry
      = hash_entry(elem, struct supp_page_table_entry, hash_elem);
  free(entry);
}
