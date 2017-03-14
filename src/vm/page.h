#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/off_t.h"
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
  struct file* file;
  off_t ofs;
  uint8_t* upage;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;              /* Check if page is writable or not */
  struct hash_elem hash_elem; /* Bookkeeping */
};

bool supp_page_table_init(struct hash *table);
void supp_page_table_destroy(struct hash *table);
struct supp_page * supp_page_table_get(struct hash *hash,
    void *vaddr);
void supp_page_table_insert(struct hash *hash, void *vaddr,
                            enum page_status_t);
void supp_page_table_remove(struct hash *hash, void *vaddr);
void print_spt(struct hash *spt);
void print_spt_entry(struct hash_elem *elem, void *);
bool supp_page_table_insert_entry(struct hash *hash, struct supp_page * entry);

#endif /* vm/page.h */
