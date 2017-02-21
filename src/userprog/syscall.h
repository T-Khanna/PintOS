#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void exit(int status);
struct file *find_file (int fd);

#endif /* userprog/syscall.h */
