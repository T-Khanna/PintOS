#include "vm/frame.h"
#include "threads/palloc.h"

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

uint8_t * frame_get_page()
{
    uint8_t *kpage = palloc_get_page (PAL_USER | PAL_ZERO);
    if (kpage == NULL) {
       // EVICTION CODE
       frame_evict(); 
    }
    // TODO Maybe install_page here!? FIXME Don't know!
    struct frame *new_frame = (struct frame *) malloc(sizeof(frame));
    new_frame->t = thread_current();
    new_frame->page_addr = kpage;
    new_frame->is_userprog = (new_frame->t->process != NULL);
    
    struct hash_elem *success = hash_insert(&hash_table, &new_frame->hash_elem);
    if (success != NULL) {
        //TODO
        printf("Something has gone terribly wrong!\n");
    }

    return kpage;

}

void frame_evict()
{
    PANIC ("ERRORRRR\n");
}

unsigned frame_hash_func(struct hash_elem *e_, void *aux)
{
    struct frame *e = hash_entry(e_, struct frame, hash_elem);
    return hash_int((int32_t) e->page_addr);
}

bool frame_hash_less(struct hash_elem *e1_, struct hash_elem *e2_, void *aux)
{
    struct frame *e1 = hash_entry(e1_, struct frame, hash_elem);
    struct frame *e2 = hash_entry(e2_, struct frame, hash_elem);
    return e1->page_addr < e2->page_addr;
}





