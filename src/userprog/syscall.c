#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
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
#ifdef VM
#include "vm/mmap.h"
#include "vm/page.h"
#endif

/* Function to call the corresponding system call with the syscall number. */
static void syscall_handler (struct intr_frame *);

/* Function to get the nth argument of the system call. */
static void* get_arg(struct intr_frame *, int n);

/* Ensures that only one syscall can touch the file system at a time. */
static struct lock filesys_lock;

/* Function to lock and unlock file system access. */
static void lock_filesys_access(void);
static void unlock_filesys_access(void);

/* System call function declarations. */
static void sys_halt(struct intr_frame *);
static void sys_exit(struct intr_frame *);
static void sys_exec(struct intr_frame *);
static void sys_wait(struct intr_frame *);
static void sys_create(struct intr_frame *);
static void sys_remove(struct intr_frame *);
static void sys_open(struct intr_frame *);
static void sys_filesize(struct intr_frame *);
static void sys_read(struct intr_frame *);
static void sys_write(struct intr_frame *);
static void sys_seek(struct intr_frame *);
static void sys_tell(struct intr_frame *);
static void sys_close(struct intr_frame *);
static void sys_mmap(struct intr_frame *);
static void sys_munmap(struct intr_frame *);

/* Functions to ensure safe user memory access. */
static void check_safe_access(const void *ptr, unsigned size);
static void check_pointer(const void *ptr);
static void check_pointer_range(const void *ptr, unsigned size);
static void check_safe_string(const char *str);


/* Contiguous number of system calls implemented, starting from 0 (HALT). */
#define IMPLEMENTED_SYSCALLS 12

/* Function pointer table for system calls. Indexed by the syscall number. */
static void (*system_calls[]) (struct intr_frame *) = {
  &sys_halt, &sys_exit, &sys_exec, &sys_wait, &sys_create, &sys_remove,
  &sys_open, &sys_filesize, &sys_read, &sys_write, &sys_seek, &sys_tell,
  &sys_close, &sys_mmap, &sys_munmap
};

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f)
{
  check_pointer(f->esp);
  int32_t syscall_number = *(int32_t *) f->esp;
  /* Kill process if the system call number is bad. */
  if (syscall_number < 0 || syscall_number > IMPLEMENTED_SYSCALLS) {
    process_kill();
  }
  system_calls[syscall_number](f);
}

void* get_arg(struct intr_frame* f, int n)
{
  int32_t* arg_pos = (int32_t *) f->esp + n;
  if (!is_user_vaddr(arg_pos)) {
    process_kill();
  }
  return (void *) *(arg_pos);
}

static void sys_halt (struct intr_frame * f UNUSED) {
  shutdown_power_off();
  NOT_REACHED();
}

void exit(int status) {
  thread_current()->process->return_status = status;
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
  NOT_REACHED();
}

static void sys_exit (struct intr_frame * f)
{
  int status = (int) get_arg(f, 1);
  exit(status);
}

static void sys_exec (struct intr_frame * f)
{
  const char* cmd_line = (const char*) get_arg(f, 1);
  check_safe_string(cmd_line);
  lock_filesys_access();
  f->eax = process_execute(cmd_line);
  unlock_filesys_access();
}

static void sys_wait (struct intr_frame * f)
{
  tid_t tid = (tid_t) get_arg(f, 1);
  f->eax = process_wait(tid);
}

static void sys_create (struct intr_frame * f)
{
  const char* file = (const char*) get_arg(f, 1);
  check_safe_string(file);
  unsigned initial_size = (unsigned) get_arg(f, 2);

  lock_filesys_access();
  bool success = filesys_create(file, initial_size);
  unlock_filesys_access();

  f->eax = success;
}

static void sys_remove(struct intr_frame * f)
{
  const char* file = (const char*) get_arg(f, 1);
  check_safe_string(file);

  lock_filesys_access();
  bool success = filesys_remove(file);
  unlock_filesys_access();

  f->eax = success;
}

static void sys_open(struct intr_frame * f)
{
  /* File descriptor. This is -1 if the file could not be opened. */
  int fd = -1;

  const char* name = (const char*) get_arg(f, 1);
  check_safe_string(name);

  /* Allocate space for descriptor. */
  struct descriptor* d = (struct descriptor*) malloc(sizeof(struct descriptor));
  if (d == NULL) {
    goto exit;
  }

  lock_filesys_access();
  struct file* file = filesys_open(name);

  /* Store the file in the descriptor and map it to the next_fd value.
     This is then pushed into the list of descriptors of the current thread. */
  if (file != NULL) {
    struct thread* t = thread_current();
    d->id = t->process->next_fd++;
    d->file = file;
    fd = d->id;
    list_push_back(&t->descriptors, &d->elem);
  }

  unlock_filesys_access();

exit:
  f->eax = fd;
}

static void sys_filesize(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  /* File size. This is -1 if the file couldn't be opened. */
  int file_byte_size = -1;
  lock_filesys_access();

  /* Go through the list and see if this file descriptor exists. */
  struct file *file = find_file(fd);
  if (file != NULL) {
    file_byte_size = file_length(file);
  }

  unlock_filesys_access();
  f->eax = file_byte_size;
}

static void sys_read(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  void* buffer = get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);

  check_pointer_range(buffer, size);

  /* This is set to -1 in case of file loading errors. */
  int bytes_read = -1;

  /* If we're reading from the stdin. */
  if (fd == STDIN_FILENO) {
    /* Then fill up the buffer with characters we're reading. */
    char *buf = (char *) buffer;
    for (unsigned i = 0; i < size; i++) {
      buf[i] = input_getc();
    }

    bytes_read = size;

  } else {
    /* Otherwise we're reading from a file instead. */
    lock_filesys_access();

    struct file *file = find_file(fd);
    if (file != NULL) {
      bytes_read = file_read(file, buffer, size);
    }

    unlock_filesys_access();
  }
  f->eax = bytes_read;
}

static void sys_write(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  const void* buffer = (const void*) get_arg(f, 2);
  unsigned size = (unsigned) get_arg(f, 3);

  check_pointer_range(buffer, size);

  /* This is set to 0 if we can't write to the file. */
  int bytes_written = 0;

  /* If we're writing to stdout. */
  if (fd == STDOUT_FILENO) {
    /* Just write all the characters from the buffer to the console. */
    putbuf(buffer, size);
    bytes_written = size;
  } else {
    /* Otherwise we're writing to a file instead. */
    lock_filesys_access();

    struct file *file = find_file(fd);
    if (file != NULL) {
      bytes_written = file_write(file, buffer, size);
    }

    unlock_filesys_access();
  }
  f->eax = bytes_written;
}

static void sys_seek(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  unsigned position = (unsigned) get_arg(f, 2);
  lock_filesys_access();

  /* If the file descriptor exists, seek to the right position. */
  struct file *file = find_file(fd);
  if (file != NULL)
    file_seek(file, position);

  unlock_filesys_access();
}

static void sys_tell(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);
  unsigned position = 0;

  lock_filesys_access();

  struct file *file = find_file(fd);
  if (file != NULL)
    position = file_tell(file);

  unlock_filesys_access();
  f->eax = position;
}

static void sys_close(struct intr_frame * f)
{
  int fd = (int) get_arg(f, 1);

  lock_filesys_access();

  struct list_elem *e;
  for (e = list_begin (&thread_current()->descriptors);
       e != list_end (&thread_current()->descriptors);
       e = list_next (e)) {

    struct descriptor *desc = list_entry(e, struct descriptor, elem);

    /* If the fd and descriptor id match, close the file and remove it from
       the list of descriptors. */
    if (desc->id == fd) {
      file_close(desc->file);
      list_remove(e);
      free(desc);
      break;
    }

  }

  unlock_filesys_access();

}

static void sys_mmap(struct intr_frame * f)
{
  mapid_t mapid = -1;
  int fd = (int) get_arg(f, 1);
  void* addr = get_arg(f, 2);
  if (pg_ofs(addr) != 0) {
    goto ret;
  }
  //TODO: Check for valid addr
  struct file* file = find_file(fd);
  if (file == NULL) {
    goto ret;
  }
  lock_filesys_access();
  uint32_t read_bytes = file_length(file);
  unlock_filesys_access();
  struct thread* t = thread_current();
  //TODO check for overlapping with other pages (probably by inspecting spt)
  mapid = t->process->next_mapid++;
  uint32_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
  /* Make an spt entry for each page */
  for (uint32_t curr_page = 0;
       curr_page <= read_bytes % PGSIZE;
       curr_page += PGSIZE)
  {
    //TODO fix size arg to insert
    mmap_file_page_table_insert(&t->mmap_file_page_table, addr + curr_page,
        mapid, file, curr_page, PGSIZE, true);
    supp_page_table_insert(&t->supp_page_table, addr + curr_page, MMAPPED);
  }
ret:
  f->eax = mapid;
}

static void sys_munmap(struct intr_frame * f)
{
  mapid_t mapping = (mapid_t) get_arg(f, 1);
  munmap(mapping);
}

void munmap(mapid_t mapping) {
  struct thread* t = thread_current();
  struct mmap_file_page* page
    = mmap_file_page_table_get(&t->mmap_file_page_table, 0);
  void* addr = page->vaddr;
  //TODO: Free mmapped pages
  mmap_file_page_table_delete_entry(&t->mmap_file_page_table, page);
  //delete_addr_mapid_mapping(&t->addrs_to_mapids, addr);
}

/******************************
 *****  HELPER FUNCTIONS  *****
 ******************************/

struct file *find_file (int fd)
{
  struct list_elem *e;
  for (e = list_begin (&thread_current()->descriptors);
       e != list_end (&thread_current()->descriptors);
       e = list_next (e)) {
    /* If the file descriptors match, return a pointer to that file. */
    if (list_entry(e, struct descriptor, elem)->id == fd)
      return list_entry(e, struct descriptor, elem)->file;
  }
  return NULL;
}

static void lock_filesys_access(void)
{
  lock_acquire(&filesys_lock);
}

static void unlock_filesys_access(void)
{
  lock_release(&filesys_lock);
}

static void check_pointer(const void *ptr)
{
  check_safe_access(ptr, 1);
}


static void check_pointer_range(const void *ptr, unsigned size)
{
  check_safe_access(ptr, size);
}

/* Checks that the pointer is not pointing to kernel memory and is mapped. */
static void check_safe_access(const void *ptr, unsigned size)
{
  /* Check the initial pointer and the first byte on every new page in the
     block of memory we are checking. */
  struct thread *cur = thread_current();
  for (const void const* base = ptr;
       ptr <= base + size;
       ptr = pg_round_down(ptr + PGSIZE)) {
    if (!is_user_vaddr(ptr)
        || pagedir_get_page(cur->pagedir, ptr) == NULL) {
      process_kill();
    }
  }
}

/* Checks the entirety of a string is in valid user memory.
   Kills the current process if it is not. */
static void check_safe_string(const char *str)
{
  struct thread *cur = thread_current();
  /* outer loop checks that a page is valid every time we enter a new page */
  while (is_user_vaddr(str)
      && pagedir_get_page(cur->pagedir, str) != NULL) {
    /* inner loop checks for a null terminator */
    while((uintptr_t) str % PGSIZE != 0) {
      if (*str == '\0') {
        return;
      }
      str++;
    }
    str++;
  }

  /* Encountered a bad address before string termination. */
  process_kill();
}
