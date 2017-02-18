#ifndef FILESYS_DESCRIPTOR_H
#define FILESYS_DESCRIPTOR_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

/* File descriptor struct */
struct descriptor 
  {
    int id;                     /* The id relating to the descriptor */
    struct file *file;          /* The file we're talking about */
    struct list_elem elem;      /* List elem for placing in descriptor list */
  };

#endif /* filesys/descriptor.h */
