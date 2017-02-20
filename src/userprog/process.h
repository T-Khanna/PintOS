#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/synch.h"
#include <list.h>
#include "filesys/descriptor.h"

typedef int tid_t;

struct process {
  tid_t tid;                          /* Thread identifier that this process
                                         is associated with.. */
  int return_status;                  /* Return status of the process. */
  bool has_waited;                    /* Checks if a process has already
                                         called wait on this thread */
  struct semaphore wait_sema;         /* Sempahore used to wait for a
                                         child process */
  struct list_elem child_elem;        /* Used by the process to keep track
                                         of its child processes. */
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
