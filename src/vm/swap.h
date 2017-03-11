#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>

typedef size_t swap_index_t;

void swap_init(void);
void swap_destroy(void);
void swap_table_init(struct hash *table);
void swap_table_destroy(struct hash *table);
bool swap_into_memory(struct hash *table, void *vaddr, void *kaddr);

struct swap_table_entry {
  void *vaddr;            /* The start of the page that this swap block holds */
  swap_index_t index;     /* The index of the swap block */
  struct hash_elem elem;  /* Bookkeeping */
};

#endif
