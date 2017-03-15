#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "vm/mmap.h"

void syscall_init (void);

void exit(int status);
void munmap(mapid_t mapping);

struct file *find_file (int fd);

/* Function to lock and unlock file system access. */
void lock_filesys_access(void);
void unlock_filesys_access(void);

#endif /* userprog/syscall.h */
