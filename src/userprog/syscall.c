#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
void* get_arg(struct intr_frame *, int arg_num);
void exit(int status);

/* Ensures that only one syscall can touch the file system
 * at a time */
struct lock filesys_lock;

static void sys_halt (struct intr_frame *);
static void sys_exit (struct intr_frame *);
static void sys_exec (struct intr_frame *);
static void sys_wait (struct intr_frame *);
static void sys_create (struct intr_frame *);
static void sys_remove (struct intr_frame *);
static void sys_open (struct intr_frame *);
static void sys_filesize (struct intr_frame *);
static void sys_read (struct intr_frame *);
static void sys_write (struct intr_frame *);
static void sys_seek (struct intr_frame *);
static void sys_tell (struct intr_frame *);
static void sys_close (struct intr_frame *);

static void (*system_calls[]) (struct intr_frame *) = {
  &sys_halt, &sys_exit, &sys_exec, &sys_wait, &sys_create, &sys_remove,
  &sys_open, &sys_filesize, &sys_read, &sys_write, &sys_seek, &sys_tell,
  &sys_close
};

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init(&filesys_lock);
  // thread_exit ();
}

static void
syscall_handler (struct intr_frame *f)
{
  int32_t syscall_number = (int32_t) f->esp;
  system_calls[syscall_number](f);
  // printf ("system call!\n");
  // thread_exit ();
}

void* get_arg(struct intr_frame* f, int arg_num) {
  return f->esp + arg_num;
}


static void sys_halt (struct intr_frame * f UNUSED) {
  shutdown_power_off();
  NOT_REACHED();
}

void exit(int status) {
  thread_current()->process_info->return_status = status;
  thread_exit();
  NOT_REACHED();
}

static void sys_exit (struct intr_frame * f) {
  // This may not be entirely finished. TODO
  int status = (int) get_arg(f, 1);
  exit(status);
}

static void sys_exec (struct intr_frame * f) {
  /*TODO: Need to make sure this is synchronized properly to force the parent
   *      process to wait until the child process has successfully loaded. */
  const char* cmd_line = (const char*) get_arg(f, 1);
  pid_t pid = process_execute(cmd_line);
  f->eax = pid;
}

static void sys_wait (struct intr_frame * f) {
  pid_t pid = (pid_t) get_arg(f, 1);
  f->eax = process_wait(pid);
}

static void sys_create (struct intr_frame * f) {
  const char* file = (const char*) get_arg(f, 1);
  unsigned initial_size = (unsigned) get_arg(f, 2);

  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  f->eax = success;
}

static void sys_remove (struct intr_frame * f) {
  const char* file = (const char*) get_arg(f, 1);

  lock_acquire(&filesys_lock);
  bool success = filesys_remove(file);
  lock_release(&filesys_lock);

  f->eax = success;
}

static void sys_open (struct intr_frame * f) {
  const char* name = (const char*) get_arg(f, 1);
  int fd = -1; // File descriptor. -1 if the file could not be opened.

  struct file* file = filesys_open(name);

  if (file != NULL) {
    // How am I supposed to know which fd to generate?
  }

  f->eax = fd;
}

static void sys_filesize (struct intr_frame * f) {
  int fd = (int) get_arg(f, 1);
  int file_byte_size = 0;

  // TODO

  f->eax = file_byte_size;
}

static void sys_read (struct intr_frame * f) {
  int fd = (int) get_arg(f, 1);
  void* buffer = get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);
  int bytes_read = 0;
  //TODO
  f->eax = bytes_read;
}

static void sys_write (struct intr_frame * f) {
  int fd = (int) get_arg(f, 1);
  const void* buffer = (const void*) get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);
  int bytes_written = 0;
  /*TODO: Need to check for invalid pointers (user memory access) and also
   *      keep track of the number of bytes written to console.
   *NOTE: file_write() may be useful for keeping track of bytes written. */
  f->eax = bytes_written;
}

static void sys_seek (struct intr_frame * f) {
  int fd = (int) get_arg(f, 1);
  unsigned position = (unsigned) get_arg(f, 2);
  //TODO
}

static void sys_tell (struct intr_frame * f) {
  int fd = (int) get_arg(f, 1);
  unsigned position = 0;
  //TODO
  f->eax = position;
}

static void sys_close (struct intr_frame * f) {
  int fd = (int) get_arg(f, 1);
  //TODO
}
