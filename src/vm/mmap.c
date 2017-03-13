#include "threads/malloc.h"
#include "filesys/file.h"
#include "vm/mmap.h"

static unsigned fpt_hash_func(const struct hash_elem *elem, void *aux UNUSED);
static bool fpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                          void *aux UNUSED);
static unsigned mfpt_hash_func(const struct hash_elem *elem, void *aux UNUSED);
static bool mfpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                           void *aux UNUSED);
static void delete_fpt(struct hash_elem *elem, void *aux UNUSED);
static void free_fpt_related_resources(struct hash_elem *elem,
                                       void *aux UNUSED);

struct file_page * file_page_table_get(struct hash* table, void* vaddr) {
  struct file_page entry;
  entry.vaddr = vaddr;
  return hash_entry(hash_find(table, &entry.hash_elem), struct file_page,
                    hash_elem);
}

bool file_page_table_insert(struct hash* table, void* vaddr, struct file* file,
                            off_t ofs, uint8_t* upage, uint32_t read_bytes,
                            uint32_t zero_bytes, bool writable) {
  struct file_page* fp = (struct file_page *) malloc(sizeof(struct file_page));
  if (fp == NULL) {
    return false;
  }
  fp->vaddr = vaddr;
  fp->file = file;
  fp->ofs = ofs;
  fp->upage = upage;
  fp->read_bytes = read_bytes;
  fp->zero_bytes = zero_bytes;
  fp->writable = writable;
  struct hash_elem *prev = hash_insert(table, &fp->hash_elem);
  return prev == NULL;
}

/* Destroy all of the elements of a supplementary page table.
   Takes appropriate action to deallocate resources for each entry. */
void file_page_table_destroy(struct hash *table)
{
  ASSERT(table != NULL);
  hash_apply(table, &free_fpt_related_resources);
  hash_destroy(table, &delete_fpt);
}

/* Take appropriate action for a supplemntary page table entry when a process
   exits.*/
static void free_fpt_related_resources(struct hash_elem *elem, void *aux UNUSED)
{
  struct file_page *entry = hash_entry(elem, struct file_page, hash_elem);
  file_close(entry->file);
}

/* Free the memory used by an entry in the supplementary page table */
static void delete_fpt(struct hash_elem *elem, void *aux UNUSED)
{
  struct file_page *entry = hash_entry(elem, struct file_page, hash_elem);
  free(entry);
}

bool file_page_table_init(struct hash* file_table) {
  return hash_init(file_table, &fpt_hash_func, &fpt_less_func, NULL);
}

bool mmap_file_page_table_init(struct hash* mmap_file_table) {
  return hash_init(mmap_file_table, &mfpt_hash_func, &mfpt_less_func, NULL);
}

/* function for hashing the page pointer in a supplementary page table entry */
static unsigned fpt_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct file_page *entry = hash_entry(elem, struct file_page, hash_elem);
  return hash_bytes(&entry->vaddr, sizeof(entry->vaddr));
}

/* function for comparing the addresses of two supplemtary page table entries */
static bool fpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                   void *aux UNUSED)
{
  struct file_page *a_entry = hash_entry(a, struct file_page, hash_elem);
  struct file_page *b_entry = hash_entry(b, struct file_page, hash_elem);
  return a_entry->vaddr < b_entry->vaddr;
}


/* function for hashing the page pointer in a supplementary page table entry */
static unsigned mfpt_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct mmap_file_page* e = hash_entry(elem, struct mmap_file_page, hash_elem);
  return hash_bytes(&e->vaddr, sizeof(e->vaddr));
}

/* function for comparing the addresses of two supplemtary page table entries */
static bool mfpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                   void *aux UNUSED)
{
  struct mmap_file_page* _a = hash_entry(a, struct mmap_file_page, hash_elem);
  struct mmap_file_page *_b = hash_entry(b, struct mmap_file_page, hash_elem);
  return _a->vaddr < _b->vaddr;
}
