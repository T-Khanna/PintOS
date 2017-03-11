#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "userprog/process.h"

struct frame {

    struct process *process;    /* Reference to the process it belongs to */
    void *page_addr;            /* Reference to page address in kernel vm */
    bool is_user_program;       /* Kernel or user program */
    struct hash_elem hash_elem; /* Hash table element */

};

void frame_init (struct frame *);

#endif /* vm/frame.h */
