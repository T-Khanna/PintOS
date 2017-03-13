#include "threads/malloc.h"
#include "vm/mmap.h"

static unsigned fpt_hash_func(const struct hash_elem *elem, void *aux UNUSED);
static bool fpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                   void *aux UNUSED);


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

bool file_page_table_init(struct hash* file_table) {
  return hash_init(file_table, &fpt_hash_func, &fpt_less_func, NULL);
}

bool mmap_file_page_table_init(struct hash* mmap_file_table) {
  return hash_init(mmap_file_table, &fpt_hash_func, &fpt_less_func, NULL);
}

/* function for hashing the page pointer in a supplementary page table entry */
unsigned fpt_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct file_page *entry = hash_entry(elem, struct file_page, hash_elem);
  return hash_bytes(&entry->vaddr, sizeof(entry->vaddr));
}

/* function for comparing the addresses of two supplemtary page table entries */
bool fpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                   void *aux UNUSED)
{
  struct file_page *a_entry = hash_entry(a, struct file_page, hash_elem);
  struct file_page *b_entry = hash_entry(b, struct file_page, hash_elem);
  return a_entry->vaddr < b_entry->vaddr;
}
