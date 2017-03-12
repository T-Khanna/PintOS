#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

static unsigned frame_hash_func(const struct hash_elem *e, void *aux);
static bool frame_hash_less(const struct hash_elem *e1,
     const struct hash_elem *e2, void *aux);
static void frame_evict(void);
struct hash hash_table;
struct lock frame_lock;

void frame_init (void)
{
    lock_init(&frame_lock);
    hash_init(&hash_table, frame_hash_func, frame_hash_less, NULL);
}

void* frame_get_page(void)
{
    uint8_t *kpage = palloc_get_page (PAL_USER | PAL_ZERO);
    if (kpage == NULL) {
       frame_evict();
       // TODO some more things probably
    }
    // TODO Maybe install_page here!? FIXME Don't know!
    struct frame *new_frame = (struct frame *) malloc(sizeof(struct frame));
    new_frame->t = thread_current();
    new_frame->page_addr = kpage;
    new_frame->is_userprog = (new_frame->t->process != NULL);

    struct hash_elem *success = hash_insert(&hash_table, &new_frame->hash_elem);
    if (success != NULL) {
        //TODO
        PANIC("Something has gone terribly wrong!\n");
    }

    return kpage;

}

static void frame_evict(void)
{
    PANIC ("ERRORRRR\n");
    // YOUR EVICTION CODE HERE FROM ONLY £50/WEEK CALL NOW
}

unsigned frame_hash_func(const struct hash_elem *e_, void *aux UNUSED)
{
    struct frame *e = hash_entry(e_, struct frame, hash_elem);
    return hash_int((int32_t) e->page_addr);
}

bool frame_hash_less(const struct hash_elem *e1_,
    const struct hash_elem *e2_, void *aux UNUSED)
{
    struct frame *e1 = hash_entry(e1_, struct frame, hash_elem);
    struct frame *e2 = hash_entry(e2_, struct frame, hash_elem);
    return e1->page_addr < e2->page_addr;
}
