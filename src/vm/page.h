#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

/* The status of a page */
enum page_status_t {
  LOADED,     /* Loaded in memory */
  MMAPPED,    /* Memory mapped and stored in the mmap table */
  SWAPPED,    /* Swapped out to disk */
  ZEROED,     /* Zeroed out page */
};

struct supp_page {
  void *vaddr;                /* The virtual address of this page */
  enum page_status_t status;  /* The status of this page */
  struct hash_elem hash_elem; /* Bookkeeping */
};

bool supp_page_table_init(struct hash *table);
void supp_page_table_destroy(struct hash *table);
struct supp_page * supp_page_table_get(struct hash *hash,
    void *vaddr);
void supp_page_table_insert(struct hash *hash, void *vaddr,
                            enum page_status_t);
void supp_page_table_remove(struct hash *hash, void *vaddr);

#endif /* vm/page.h */
