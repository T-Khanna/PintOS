#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>

struct frame {

    struct thread *t;           /* Reference to the process it belongs to */
    void *page_addr;            /* Reference to page address in kernel vm */
    bool is_userprog;           /* TODO Check if needed. Used to check if a
                                   frame can be evicted or not (in case
                                   it's kernel memory */
    struct hash_elem hash_elem; /* Hash table element */

};

void frame_init (struct frame *);
void frame_get_page(void * args);
void frame_free_page(void *addr);

#endif /* vm/frame.h */
