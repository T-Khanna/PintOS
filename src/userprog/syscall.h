#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Type used to identify mapped regions. */
typedef int mapid_t;

void syscall_init (void);

void exit(int status);
struct file *find_file (int fd);

#endif /* userprog/syscall.h */
