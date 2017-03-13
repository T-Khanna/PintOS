#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "vm/mmap.h"

void syscall_init (void);

void exit(int status);
void munmap(mapid_t mapping);

struct file *find_file (int fd);

#endif /* userprog/syscall.h */
