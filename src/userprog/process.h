#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pid_t;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process {
  int fd;                             /* File descriptor for process */
  int return_status;                  /* Return status of the process. */
};

#endif /* userprog/process.h */
