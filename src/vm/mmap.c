#include "threads/malloc.h"
#include "filesys/file.h"
#include "vm/mmap.h"

/* Prototypes */

static struct addr_to_mapid* get_mapping(struct hash* table, void* vaddr);
static unsigned addrmap_hash_func(const struct hash_elem *elem,
    void *aux UNUSED);
static bool addrmap_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED);

static unsigned mfpt_hash_func(const struct hash_elem *elem, void *aux UNUSED);
static bool mfpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
    void *aux UNUSED);
static void delete_mfpt(struct hash_elem *elem, void *aux UNUSED);
static void free_mfpt_related_resources(struct hash_elem *elem,
    void *aux UNUSED);

/* Function definitons */

bool address_mapid_init(struct hash* table) {
  return hash_init(table, &addrmap_hash_func, &addrmap_less_func, NULL);
}

bool add_addr_mapid_mapping(struct hash* table, void* vaddr, mapid_t mapid) {
  struct addr_to_mapid* map = malloc(sizeof(struct addr_to_mapid));
  if (map == NULL) {
    return false;
  }
  map->vaddr = vaddr;
  map->mapid = mapid;
  struct hash_elem* prev = hash_insert(table, &map->hash_elem);
  return prev == NULL;
}


static struct addr_to_mapid* get_mapping(struct hash* table, void* vaddr) {
  struct addr_to_mapid entry;
  entry.vaddr = vaddr;
  return hash_entry(hash_find(table, &entry.hash_elem), struct addr_to_mapid,
      hash_elem);
}

mapid_t get_mapid_from_addr(struct hash* table, void* vaddr) {
  struct addr_to_mapid* mapping = get_mapping(table, vaddr);
  return mapping->mapid;
}

bool delete_addr_mapid_mapping(struct hash* table, void* vaddr) {
  struct addr_to_mapid* entry = get_mapping(table, vaddr);
  struct hash_elem* found = hash_delete(table, &entry->hash_elem);
  return found != NULL;
}

static unsigned addrmap_hash_func(const struct hash_elem *elem,
    void *aux UNUSED) {
  struct addr_to_mapid* e = hash_entry(elem, struct addr_to_mapid, hash_elem);
  return hash_bytes(&e->vaddr, sizeof(e->vaddr));
}

static bool addrmap_less_func(const struct hash_elem *a,
    const struct hash_elem *b, void *aux UNUSED) {
  struct addr_to_mapid* _a = hash_entry(a, struct addr_to_mapid, hash_elem);
  struct addr_to_mapid* _b = hash_entry(b, struct addr_to_mapid, hash_elem);
  return _a->vaddr < _b->vaddr;
}



bool mmap_file_page_table_init(struct hash* mmap_file_table) {
  return hash_init(mmap_file_table, &mfpt_hash_func, &mfpt_less_func, NULL);
}

struct mmap_file_page * mmap_file_page_table_get(struct hash* table,
    mapid_t mapid) {
  struct mmap_file_page entry;
  entry.mapid = mapid;
  return hash_entry(hash_find(table, &entry.hash_elem), struct mmap_file_page,
      hash_elem);
}

bool mmap_file_page_table_insert(struct hash* table, void* vaddr, mapid_t mapid,
    struct file* file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes,
    bool writable) {
  struct mmap_file_page* mmap_page = malloc(sizeof(struct mmap_file_page));
  if (mmap_page == NULL) {
    return false;
  }
  mmap_page->vaddr = vaddr;
  mmap_page->mapid = mapid;
  mmap_page->file = file;
  mmap_page->ofs = ofs;
  mmap_page->read_bytes = read_bytes;
  mmap_page->zero_bytes = zero_bytes;
  mmap_page->writable = writable;
  struct hash_elem *prev = hash_insert(table, &mmap_page->hash_elem);
  return prev == NULL;
}

bool mmap_file_page_table_delete_entry(struct hash* table,
    struct mmap_file_page* entry) {
  ASSERT(entry != NULL);
  struct hash_elem* found = hash_delete(table, &entry->hash_elem);
  return found != NULL;
}

void mmap_file_page_table_destroy(struct hash *table)
{
  ASSERT(table != NULL);
  hash_apply(table, &free_mfpt_related_resources);
  hash_destroy(table, &delete_mfpt);
}

static void free_mfpt_related_resources(struct hash_elem *elem, void *aux UNUSED)
{
  struct mmap_file_page *p = hash_entry(elem, struct mmap_file_page, hash_elem);
  // file_close(p->file);
}

static void delete_mfpt(struct hash_elem *elem, void *aux UNUSED)
{
  struct mmap_file_page *p = hash_entry(elem, struct mmap_file_page, hash_elem);
  free(p);
}

static unsigned mfpt_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct mmap_file_page* e = hash_entry(elem, struct mmap_file_page, hash_elem);
  return hash_bytes(&e->mapid, sizeof(e->mapid));
}

static bool mfpt_less_func(const struct hash_elem *a, const struct hash_elem *b,
                   void *aux UNUSED)
{
  struct mmap_file_page* _a = hash_entry(a, struct mmap_file_page, hash_elem);
  struct mmap_file_page* _b = hash_entry(b, struct mmap_file_page, hash_elem);
  return _a->mapid < _b->mapid;
}
