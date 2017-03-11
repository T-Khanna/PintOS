#include "vm/frame.h"

static unsigned frame_hash_func(struct hash_elem *e, void *);
static bool frame_hash_less(struct hash_elem *e1, 
     struct hash_elem *e2, void *);
struct hash hash_table;
struct lock frame_lock;

void frame_init (struct frame *)
{
    lock_init(&frame_lock);
    hash_init(&hash_table, frame_hash_func, frame_hash_less, NULL);
}

void frame_get_page()
{
}


unsigned frame_hash_func(struct hash_elem *e_, void *aux)
{
}

bool frame_hash_less(struct hash_elem *e1_, struct hash_elem *e2_, void *aux)
{
    struct frame *e1 = hash_entry(e1_, struct frame, hash_elem);
    struct frame *e2 = hash_entry(e2_, struct frame, hash_elem);
    return e1->page_addr < e2->page_addr;
}





