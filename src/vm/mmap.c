#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/mmap.h"

/* Prototypes */

static unsigned mapaddr_hash_func(const struct hash_elem *elem,
    void *aux UNUSED);
static bool mapaddr_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED);

static unsigned mfpt_hash_func(const struct hash_elem *elem, void *aux UNUSED);
static bool mfpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
    void *aux UNUSED);
static void delete_mfpt(struct hash_elem *elem, void *aux UNUSED);

/* Function definitons */

bool mapping_init(struct hash* table) {
  return hash_init(table, &mapaddr_hash_func, &mapaddr_less_func, NULL);
}

bool add_mapping(struct hash* table, mapid_t mapid, void* start_addr,
    void* end_addr) {
  struct mapid_to_addr* map = malloc(sizeof(struct mapid_to_addr));
  if (map == NULL) {
    return false;
  }
  map->mapid = mapid;
  map->start_addr = start_addr;
  map->end_addr = end_addr;
  struct hash_elem* prev = hash_insert(table, &map->hash_elem);
  return prev == NULL;
}


struct mapid_to_addr* get_mapping(struct hash* table, mapid_t mapid) {
  struct mapid_to_addr entry;
  entry.mapid = mapid;
  return hash_entry(hash_find(table, &entry.hash_elem), struct mapid_to_addr,
      hash_elem);
}

bool delete_mapping(struct hash* table, mapid_t mapid) {
  struct mapid_to_addr* entry = get_mapping(table, mapid);
  struct hash_elem* found = hash_delete(table, &entry->hash_elem);
  return found != NULL;
}

static unsigned mapaddr_hash_func(const struct hash_elem *elem,
    void *aux UNUSED) {
  struct mapid_to_addr* e = hash_entry(elem, struct mapid_to_addr, hash_elem);
  return hash_bytes(&e->mapid, sizeof(e->mapid));
}

static bool mapaddr_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED) {
  struct mapid_to_addr* _a = hash_entry(a, struct mapid_to_addr, hash_elem);
  struct mapid_to_addr* _b = hash_entry(b, struct mapid_to_addr, hash_elem);
  return _a->mapid < _b->mapid;
}



bool mmap_file_page_table_init(struct hash* mmap_file_table) {
  return hash_init(mmap_file_table, &mfpt_hash_func, &mfpt_less_func, NULL);
}

struct mmap_file_page * mmap_file_page_table_get(struct hash* table,
    void *vaddr) {
  struct mmap_file_page entry;
  entry.vaddr = vaddr;
  struct hash_elem *found = hash_find(table, &entry.hash_elem);
  return found != NULL ? hash_entry(found, struct mmap_file_page,
      hash_elem) : NULL;
}

bool mmap_file_page_table_insert(struct hash* table, void* vaddr, mapid_t mapid,
    struct file* file, off_t ofs, uint32_t size, bool writable) {
  struct mmap_file_page* mmap_page = malloc(sizeof(struct mmap_file_page));
  if (mmap_page == NULL) {
    return false;
  }
  ASSERT(size <= PGSIZE);
  mmap_page->vaddr = vaddr;
  mmap_page->mapid = mapid;
  mmap_page->file = file;
  mmap_page->ofs = ofs;
  mmap_page->size = size;
  mmap_page->writable = writable;
  struct hash_elem *prev = hash_insert(table, &mmap_page->hash_elem);
  // printf("AFTER INSERTION\n");
  // print_mmap_table(table);
  return prev == NULL;
}

bool mmap_file_page_table_delete_entry(struct hash* table,
    struct mmap_file_page* entry) {
  ASSERT(entry != NULL);
  void* vaddr = entry->vaddr;
  struct thread* t = thread_current();
  struct hash_elem* found = hash_delete(table, &entry->hash_elem);
  if (found != NULL) {
    free(hash_entry(found, struct mmap_file_page, hash_elem));
  }
  return found != NULL;
}

void mmap_file_page_table_destroy(struct hash *table)
{
  ASSERT(table != NULL);
  hash_destroy(table, &delete_mfpt);
}

static void delete_mfpt(struct hash_elem *elem, void *aux UNUSED)
{
  struct mmap_file_page *p = hash_entry(elem, struct mmap_file_page, hash_elem);
  struct thread* t = thread_current();
  if (pagedir_is_dirty(t->pagedir, p->vaddr)) {
    file_write(p->file, p->vaddr, p->size);
  }
  free(p);
}

static unsigned mfpt_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct mmap_file_page* e = hash_entry(elem, struct mmap_file_page, hash_elem);
  return hash_bytes(&e->vaddr, sizeof(e->vaddr));
}

static bool mfpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                   void *aux UNUSED)
{
  struct mmap_file_page* _a = hash_entry(a, struct mmap_file_page, hash_elem);
  struct mmap_file_page* _b = hash_entry(b, struct mmap_file_page, hash_elem);
  return _a->vaddr < _b->vaddr;
}

static void print_mmap_entry(struct hash_elem *elem, void *aux UNUSED);

/* print an spt, one line per entry */
void print_mmap_table(struct hash *table)
{
  hash_apply(table, print_mmap_entry);
}

static void print_mmap_entry(struct hash_elem *elem, void *aux UNUSED)
{
  struct mmap_file_page* p = hash_entry(elem, struct mmap_file_page, hash_elem);
  char* is_writable = p->writable ? "True" : "False";
  printf("MAPID: %d, VADDR: %p, FILE OFFSET: %d, SIZE: %d, WRITEABLE? %s\n",
            p->mapid, p->vaddr, p->ofs, p->size, is_writable);
}

static void print_mapped_entry(struct hash_elem *elem, void *aux UNUSED);

/* print an spt, one line per entry */
void print_mappings(struct hash *table)
{
  hash_apply(table, print_mapped_entry);
}

static void print_mapped_entry(struct hash_elem *elem, void *aux UNUSED)
{
  struct mapid_to_addr* mta = hash_entry(elem, struct mapid_to_addr, hash_elem);
  printf("MAPID: %d, START ADDR: %p, END ADDR: %p\n", mta->mapid,
      mta->start_addr, mta->end_addr);
}
