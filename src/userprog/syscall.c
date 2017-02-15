#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

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
}

void exit(int exitcode) {
  thread_current()->return_status = exitcode;
  thread_exit();
}

static void sys_exit (struct intr_frame * f) {
  int exitcode = (int) get_arg(f, 1);
  exit(exitcode);
}

static void sys_exec (struct intr_frame * f) {
  const char* cmd_line = (const char*) get_arg(f, 1);
  pid_t pid = process_execute(cmd_line);
  f->eax = pid;
}

static void sys_wait (struct intr_frame * f) {

}

static void sys_create (struct intr_frame * f) {

}

static void sys_remove (struct intr_frame * f) {

}

static void sys_open (struct intr_frame * f) {

}

static void sys_filesize (struct intr_frame * f) {

}

static void sys_read (struct intr_frame * f) {


}
static void sys_write (struct intr_frame * f) {
  int fd = (int) get_arg(f, 1);
  const void* buffer = (const void*) get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);
  int bytes_written = 0;
  /*TODO: Need to check for invalid pointers (user memory access) and also
   *      keep track of the number of bytes written to console. */
  f->eax = bytes_written;
}

static void sys_seek (struct intr_frame * f) {

}

static void sys_tell (struct intr_frame * f) {

}

static void sys_close (struct intr_frame * f) {

}
