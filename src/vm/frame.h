#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>

struct frame {

    struct thread *t;           /* Reference to the process it belongs to */
    void *uaddr;                /* The address in user memory. */
    void *kaddr;                /* Reference to page address in kernel vm.
                                   Also the key used for the hash table. */
    bool clock_bit;             /* Used in clock page replacement algorithm */
    struct hash_elem hash_elem; /* Hash table element */

};

void frame_init (void);
void* frame_get_page(void *uaddr);
void frame_free_page(void *kaddr);
struct frame * choose_victim(void);

#endif /* vm/frame.h */
