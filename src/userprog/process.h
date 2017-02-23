#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/synch.h"
#include <list.h>
#include "filesys/descriptor.h"

#define RET_ERROR -1
#define MAX_CMD 3072
#define MAX_ARGS 200

typedef int tid_t;

struct process
  {
    tid_t tid;                          /* Thread identifier that this process
                                           is associated with.. */
    int return_status;                  /* Return status of the process. */
    int next_fd;                        /* Used to allocate the id for the next
                                           file descriptor. */
//    bool has_waited;                    /* Checks if a process has already
//                                           called wait on this thread */
    bool load_success;                  /* Checks if loading an executable was
                                           a success. */
    bool thread_dead;                   /* True if the thread is dead */
    bool parent_dead;                   /* True if the parent process is dead */
    struct file* executable;            /* Process executable */
    struct semaphore wait_sema;         /* Sempahore used to wait for a
                                           child process. */
    struct semaphore exec_sema;         /* Semaphore used to synchronise with
                                           a exec child process. */
    struct list_elem child_elem;        /* Used by the process to keep track
                                           of its child processes. */
  };

struct process* get_process_by_tid(tid_t tid, struct list* processes);
bool init_process (struct thread *t);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void process_kill(void);

#endif /* userprog/process.h */
