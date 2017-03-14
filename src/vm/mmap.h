#ifndef VM_MMAP_H
#define VM_MMAP_H

#include "filesys/off_t.h"
#include <hash.h>

/* Type used to identify mapped regions. */
typedef int mapid_t;

struct mmap_file_page {
  mapid_t mapid;              /* The mapid for this mapping belongs to */
  void* vaddr;                /* The page that this entry is for */
  struct file* file;          /* The file this page refers to */
  off_t ofs;                  /* The offset of the start of the page into the
                                 file */
  uint32_t size;              /* The number of bytes into the page that belong
                                 to the file. */
  bool writable;              /* Check if page is writable or not */
  struct hash_elem hash_elem; /* Bookkeeping */
};

struct addr_to_mapid {
  void* vaddr;                /**/
  mapid_t mapid;              /**/
  struct hash_elem hash_elem; /**/
};

bool address_mapid_init(struct hash* table);
bool add_addr_mapid_mapping(struct hash* table, void* vaddr, mapid_t mapid);
mapid_t get_mapid_from_addr(struct hash* table, void* vaddr);
bool delete_addr_mapid_mapping(struct hash* table, void* vaddr);

bool mmap_file_page_table_init(struct hash* file_table);
struct mmap_file_page * mmap_file_page_table_get(struct hash* table,
    void *vaddr);
bool mmap_file_page_table_insert(struct hash* table, void* vaddr, mapid_t mapid,
    struct file* file, off_t ofs, uint32_t size, bool writable);
bool mmap_file_page_table_delete_entry(struct hash* table,
    struct mmap_file_page* page);
void mmap_file_page_table_destroy(struct hash *table);



void print_mmap_table(struct hash *table);

#endif
