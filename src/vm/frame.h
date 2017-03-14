#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "vm/page.h"

struct frame {

    struct thread *t;           /* Reference to the process it belongs to */
    void *uaddr;                /* The address in user memory. */
    void *kaddr;                /* Reference to page address in kernel vm.
                                   Also the key used for the hash table. */
    struct hash_elem hash_elem; /* Hash table element */

};

void frame_init (void);
void* frame_get_page(void *uaddr);
void frame_free_page(void *kaddr);

#endif /* vm/frame.h */
