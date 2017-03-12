#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>

struct frame {

    struct thread *t;           /* Reference to the process it belongs to */
    void *user_addr;            /* The address in user memory. */
    void *page_addr;            /* Reference to page address in kernel vm.
                                   Also the key used for the hash table. */
    bool is_userprog;           /* TODO Check if needed. Used to check if a
                                   frame can be evicted or not (in case
                                   it's kernel memory */
    struct hash_elem hash_elem; /* Hash table element */

};

void frame_init (void);
void* frame_get_page(void *);
//void frame_free_page(void *addr);

#endif /* vm/frame.h */
