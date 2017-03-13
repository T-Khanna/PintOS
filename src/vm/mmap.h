#ifndef VM_MMAP_H
#define VM_MMAP_H

#include "filesys/off_t.h"
#include <hash.h>

/* Type used to identify mapped regions. */
typedef int mapid_t;

struct mmap_file_page {
  void* vaddr;                /**/
  mapid_t mapid;              /**/
  struct file* file;          /**/
  off_t ofs;                  /**/
  uint32_t read_bytes;        /**/
  uint32_t zero_bytes;        /**/
  bool writable;              /* Check if page is writable or not */
  struct hash_elem hash_elem; /* Bookkeeping */
};

bool mmap_file_page_table_init(struct hash* file_table);
struct mmap_file_page * mmap_file_page_table_get(struct hash* table, void* vaddr);
bool mmap_file_page_table_insert(struct hash* table, void* vaddr,
    struct file* file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes,
    bool writable);
void mmap_file_page_table_destroy(struct hash *table);

#endif
