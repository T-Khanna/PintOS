#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

/* The status of a page */
enum page_status_t {
  LOADED,     /* Loaded in memory */
  IN_FILESYS, /* In filesystem */
  MMAPPED,    /* Memory mapped and stored in the mmap table */
  SWAPPED,    /* Swapped out to disk */
  ZEROED,     /* Zeroed out page */
  LAZY_EXEC   /* Part of a lazy loaded executable, not yet loaded */
};

struct supp_page_table_entry {
  void *vaddr;                /* The virtual address of this page */
  enum page_status_t status;  /* The status of this page */
  struct hash_elem hash_elem; /* Bookkeeping */
};

bool supp_page_table_init(struct hash *table);
void supp_page_table_destroy(struct hash *table);
struct supp_page_table_entry * supp_page_table_get(struct hash *hash,
    void *vaddr);
struct supp_page_table_entry * supp_page_table_insert(struct hash *hash,
    struct supp_page_table_entry *entry);

#endif /* vm/page.h */
