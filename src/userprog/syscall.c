#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <kernel/console.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
void* get_arg(struct intr_frame *, int arg_num);
void exit(int status);

/* Ensures that only one syscall can touch the file system
 * at a time */
struct lock filesys_lock;
static int fd_count = 3; /* 0,1,2 are used for STDIN/OUT/ERR */

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

static bool check_safe_access(void *ptr, unsigned size);

/* contiguous number of system calls implemented, starting from 0 (HALT) */
#define IMPLEMENTED_SYSCALLS 12

/* function pointer table for system call handers.
 * indexed by their syscall number. */
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
  //hex_dump(0, f->esp, 0xc0000000 - (int) f->esp, 1);    
  int32_t syscall_number = *(int32_t *) f->esp;
  //kill thread if the system call number is bad.
  if (syscall_number < 0 || syscall_number > IMPLEMENTED_SYSCALLS) {
    thread_exit();
  }
  system_calls[syscall_number](f);
}

void* get_arg(struct intr_frame* f, int arg_num)
{
  return  *((int32_t *) f->esp + arg_num);
}


static void sys_halt (struct intr_frame * f UNUSED)
{
  shutdown_power_off();
  NOT_REACHED();
}

void exit(int status) {
  thread_current()->return_status = status;
  thread_exit();
  NOT_REACHED();
}

static void sys_exit (struct intr_frame * f)
{
  // This may not be entirely finished. TODO
  int status = (int) get_arg(f, 1);
  exit(status);
}

static void sys_exec (struct intr_frame * f)
{
  /*TODO: Need to make sure this is synchronized properly to force the parent
   *      process to wait until the child process has successfully loaded. */
  const char* cmd_line = (const char*) get_arg(f, 1);
  pid_t pid = process_execute(cmd_line);
  f->eax = pid;
}

static void sys_wait (struct intr_frame * f)
{
  pid_t pid = (pid_t) get_arg(f, 1);
  f->eax = process_wait(pid);
}

static void sys_create (struct intr_frame * f)
{
  const char* file = (const char*) get_arg(f, 1);
  unsigned initial_size = (unsigned) get_arg(f, 2);

  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  f->eax = success;
}

static void sys_remove(struct intr_frame * f)
{
  const char* file = (const char*) get_arg(f, 1);

  lock_acquire(&filesys_lock);
  bool success = filesys_remove(file);
  lock_release(&filesys_lock);

  f->eax = success;
}

static void sys_open(struct intr_frame * f)
{
  
  printf("The address is %p\n", get_arg(f, 1));
  
  int fd = -1; // File descriptor/ -1 if the file could not be opened.

  if (!check_safe_access(get_arg(f, 1), 1)) {
    printf("The file cannot be open\n");
    f->eax = fd;
    return;
  }
    
  const char* name = (const char*) get_arg(f, 1);

  lock_acquire(&filesys_lock);
  struct file* file = filesys_open(name);
  if (file != NULL) {
    struct descriptor desc;
    desc.id = fd_count++;
    desc.file = file;
    fd = desc.id;
    list_push_front (&thread_current()->descriptors, &desc.elem);
  }
  lock_release(&filesys_lock);

  f->eax = fd;

}

static void sys_filesize(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  int file_byte_size = -1; // File size. -1 if the file couldn't be opened.

  lock_acquire(&filesys_lock);
  // Go through the list and see if this file descriptor exists
  struct list_elem *e;
  for (e = list_begin (&thread_current()->descriptors); 
       e != list_end (&thread_current()->descriptors); 
       e = list_next (e)) {
    if (list_entry(e, struct descriptor, elem)->id == fd) {
      file_byte_size = file_length(list_entry(e, struct descriptor, elem)->file);
      break;
    }
  }

  lock_release(&filesys_lock);
  f->eax = file_byte_size;
}

static void sys_read(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  void* buffer = get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);
  int bytes_read = 0;
  // TODO
  f->eax = bytes_read;
}

static void sys_write(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  const void* buffer = (const void*) get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);
    //printf("I AM INSIDE WRITE! THE SIZE IS %d\n", size);
  

  int bytes_written = 0;
  /*TODO: Need to check for invalid pointers (user memory access) and also
   *      keep track of the number of bytes written to console.
   *NOTE: file_write() may be useful for keeping track of bytes written. */
  if (fd == 1) {
    //TODO: check these pointers!!! (unsafe)
    putbuf(buffer, size);
  }
  f->eax = bytes_written;
}

static void sys_seek(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  unsigned position = (unsigned) get_arg(f, 2);
  //TODO
}

static void sys_tell(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  unsigned position = 0;
  //TODO
  f->eax = position;
}

static void sys_close(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  //TODO
}



static bool check_safe_access(void *ptr, unsigned size)
{
  /* Checks that the pointer is not null */
  if (ptr == NULL) {
    return false;
  }

  for (int i = 0; i < size; i++) {
    if (pagedir_get_page(thread_current()->pagedir, 
                (char *) ptr + i) == NULL) {
        return false;
    }
  }
  return true;
}





