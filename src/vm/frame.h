#ifndef VM_FRAME_H
#define VM_FRAME_H

struct frame {

    struct thread *t;           /* Reference to the process it belongs to */
    void *page_addr;            /* Reference to page address in kernel vm */
    void *phys_addr;            /* Address in physical memory */
    struct hash_elem hash_elem; /* Hash table element */

};

void frame_init (struct frame *);
void frame_get_page(void * args);
void frame_free_page(void *addr);

#endif /* vm/frame.h */
