#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <kernel/console.h>
#include "threads/interrupt.h"
#include "lib/stdio.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
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
static int fd_count = 3; // 0,1,2 are used for STDIN/OUT/ERR

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
static void check_pointer(void *ptr);
static void check_pointer_range(void *ptr, unsigned size);

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
  
  check_pointer(f->esp);     
  int32_t syscall_number = *(int32_t *) f->esp;
  //printf("new syscall with code %d\n", syscall_number);
  //kill thread if the system call number is bad.
  if (syscall_number < 0 || syscall_number > IMPLEMENTED_SYSCALLS) {
    thread_exit();
  }
  system_calls[syscall_number](f);
}

void* get_arg(struct intr_frame* f, int arg_num)
{
  return  (void *) *((int32_t *) f->esp + arg_num);
}

static void sys_halt (struct intr_frame * f UNUSED) {
  shutdown_power_off();
  NOT_REACHED();
}

void exit(int status) {
  thread_current()->process_info->return_status = status;
  printf("%s: exit(%d)\n", thread_current()->name, status);  
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
  const char* cmd_line = (const char*) get_arg(f, 1);
  check_pointer(cmd_line);

  tid_t child_tid = process_execute(cmd_line);

  if (child_tid < 0) {
     f->eax = -1;
     return;
  }

  struct process * pr = get_process_by_tid(child_tid, &thread_current()->child_processes);

  f->eax = (pr->load_success) ? child_tid : -1;
}

static void sys_wait (struct intr_frame * f)
{
  tid_t tid = (tid_t) get_arg(f, 1);
  //printf("The current thread name is %s\n", thread_current()->name);
  //printf("PROCESS WAITING\n");
  f->eax = process_wait(tid);
}

static void sys_create (struct intr_frame * f)
{
  const char* file = (const char*) get_arg(f, 1);
  unsigned initial_size = (unsigned) get_arg(f, 2);

  check_pointer(file);

  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  f->eax = success;
}

static void sys_remove(struct intr_frame * f)
{
  const char* file = (const char*) get_arg(f, 1);

  check_pointer(file);

  lock_acquire(&filesys_lock);
  bool success = filesys_remove(file);
  lock_release(&filesys_lock);

  f->eax = success;
}

static void sys_open(struct intr_frame * f)
{

  //printf("The address is %p\n", get_arg(f, 1));

  int fd = -1; // File descriptor/ -1 if the file could not be opened.

  const char* name = (const char*) get_arg(f, 1);
  check_pointer(name); 

  lock_acquire(&filesys_lock);

  struct file* file = filesys_open(name);
  if (file != NULL) {
    // If the file is an executable, we don't want to write to it.
    if (!strcmp(name, thread_current()->name)) {
      file_deny_write(file);
    }

    struct descriptor *desc = malloc(sizeof(struct descriptor));
    // TODO Check Malloc
    desc->id = fd_count++;
    desc->file = file;
    fd = desc->id;
    list_push_back(&thread_current()->descriptors, &desc->elem);  
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
  struct file *file = find_file(fd);
  if (file != NULL)
    file_byte_size = file_length(file);

  lock_release(&filesys_lock);
  f->eax = file_byte_size;
}

static void sys_read(struct intr_frame * f)
{
    //printf("NOW IT'S READ TIME!\n");
  int fd = (int) get_arg(f, 1);
  void* buffer = get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);
  int bytes_read = -1; // -1 in case of errors

  check_pointer_range(buffer, size);

  /* if we're reading from the stdin */
  if (fd == STDIN_FILENO) {
    /* then fill up the buffer with characters we're reading */
    int i;
    char *buf = (char *) buffer;
    for (i = 0; i < size; i++)
      buf[i] = input_getc();

    bytes_read = size;

  } else { /* otherwise we're reading from a file instead */
    lock_acquire(&filesys_lock);
  
    struct file *file = find_file(fd);
    if (file != NULL) {
      /* so use the predefined function to read the file */
      bytes_read = file_read(file, buffer, size);
    }
  
    lock_release(&filesys_lock);
  }  
  f->eax = bytes_read;
}

static void sys_write(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  const void* buffer = (const void*) get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);

  int bytes_written = 0;

  /* Need to check for invalid pointers (user memory access) and also
   * keep track of the number of bytes written to console. */
 
  check_pointer_range(buffer, size);

  /* NOTE: file_write() may be useful for keeping track of bytes written. */
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    bytes_written = size;
  } else {
    lock_acquire(&filesys_lock);

    struct file *file = find_file(fd);
    if (file != NULL) {
      bytes_written = file_write(file, buffer, size);
    }

    lock_release(&filesys_lock);
  }
  f->eax = bytes_written;
}

static void sys_seek(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  unsigned position = (unsigned) get_arg(f, 2);
  lock_acquire(&filesys_lock);

  // If the file descriptor exists, seek to the right position
  struct file *file = find_file(fd);
  if (file != NULL)
    file_seek(file, position);

  lock_release(&filesys_lock);
}

static void sys_tell(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  unsigned position = 0;

  lock_acquire(&filesys_lock);

  struct file *file = find_file(fd);
  if (file != NULL)
    position = file_tell(file);

  lock_release(&filesys_lock);
  f->eax = position;
}

static void sys_close(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  
  lock_acquire(&filesys_lock);

  struct list_elem *e;
  for (e = list_begin (&thread_current()->descriptors); 
       e != list_end (&thread_current()->descriptors); 
       e = list_next (e)) {

    struct descriptor *desc = list_entry(e, struct descriptor, elem);
    
    /* If the file descriptors match, let's close the file and
     * remove it from the list of file descriptors */
    if (desc->id == fd) {
      file_close(desc->file);
      list_remove(e);
      break;
    }

  }

  lock_release(&filesys_lock);

}

/******************************
 *****  HELPER FUNCTIONS  *****
 ******************************/

struct file *find_file (int fd) {
  struct list_elem *e;
  for (e = list_begin (&thread_current()->descriptors);
       e != list_end (&thread_current()->descriptors);
       e = list_next (e)) {
    // If the file descriptors match, return a pointer to that file
    if (list_entry(e, struct descriptor, elem)->id == fd)
      return list_entry(e, struct descriptor, elem)->file;
  }
  return NULL;
}

static void check_pointer(void *ptr)
{
  if (!check_safe_access(ptr, 1)) {

      //printf("AHJSHAIKJSHBCYTKEVWUYT\n");
    exit(-1);
  }
}


static void check_pointer_range(void *ptr, unsigned size)
{
  if (!check_safe_access(ptr, size)) {
    exit(-1);
  }
}

/* TODO Add process kill */
static bool check_safe_access(void *ptr, unsigned size)
{
  /* Checks that the pointer is not null, not pointing to kernel memory
   * and is mapped */

    //if (ptr == NULL) {
    //    printf("BUBU\n");
    //}
  for (unsigned i = 0; i < size; i++) {
      //printf("%s\n", (char *) ptr);
    if ((char *) ptr + i == NULL || !is_user_vaddr((char *) ptr + i)) {
      return false;
    }
    if (pagedir_get_page(thread_current()->pagedir,
                (char *) ptr + i) == NULL) {
        return false;
    } else {
        //printf("Address mapped: %x\n", *(int32_t*) pagedir_get_page(thread_current()->pagedir,
        //            (char *) ptr + 1));
    }
  }
  return true;
}
