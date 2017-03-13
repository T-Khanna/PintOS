#ifndef VM_MMAP_H
#define VM_MMAP_H

#include "filesys/off_t.h"
#include <hash.h>

/* Type used to identify mapped regions. */
typedef int mapid_t;

struct file_page {
  void* vaddr;                /**/
  struct file* file;          /**/
  off_t ofs;                  /**/
  uint8_t* upage;             /**/
  uint32_t read_bytes;        /**/
  uint32_t zero_bytes;        /**/
  bool writable;              /* Check if page is writable or not */
  struct hash_elem hash_elem; /* Bookkeeping */
};

struct mmap_file_page {
  mapid_t mapid;              /**/
  void* vaddr;                /**/
  struct file* file;          /**/
  struct hash_elem hash_elem; /* Bookkeeping */
};

bool file_page_table_insert(struct hash* table, void* vaddr, struct file* file,
                            off_t ofs, uint8_t* upage, uint32_t read_bytes,
                            uint32_t zero_bytes, bool writable);
bool file_page_table_init(struct hash* file_table);
/******************************************************************************/
bool mmap_file_page_table_init(struct hash* mmap_file_table);

#endif
