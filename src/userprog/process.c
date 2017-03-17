#include "userprog/process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#ifdef VM
  #include "vm/page.h"
  #include "vm/frame.h"
  #include "vm/mmap.h"
  #include "vm/swap.h"
#endif

#define MAX_FILE_NAME 16
#define WORDSIZE 4

static thread_func start_process NO_RETURN;
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static int count_args(char *command);
static void read_args(char **argv, char **file_name, char *command_);
static void push_args(struct intr_frame *if_, int argc, char **argv);
static char* strcpy_stack(char *dst, char *src);
static void push_word(uint32_t *word, struct intr_frame *if_);
static void unmap_elem(struct hash_elem *elem, void *aux UNUSED);

/* Starts a new thread running a command, with the program name as the first
   word and any arguments following it.
   The entire command must be less than 3kB, and have less than 200 arguments.
   This function assumes that all of command is in valid memory (checked by the
   syscall handler).
   The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *command)
{
  char *cmd_copy;
  char name[MAX_FILE_NAME + 1];
  size_t cmd_len = strlen(command);

  if (cmd_len > MAX_CMD) {
    return TID_ERROR;
  }

  /* Copy the command into cmd_copy.
     Otherwise there's a race between the caller and load(). */
  cmd_copy = palloc_get_page (0);
  if (cmd_copy == NULL) {
    return TID_ERROR;
  }
  strlcpy(cmd_copy, command, MAX_CMD);
  strlcpy(name, command, sizeof name);
  char *save_ptr;
  char *file_name = strtok_r(name, " ", &save_ptr);

  /* Create a new thread to execute FILE_NAME. */
  tid_t tid = thread_create(file_name, PRI_DEFAULT, start_process, cmd_copy);
  if (tid == TID_ERROR) {
    palloc_free_page (cmd_copy);
    return tid;
  }

  struct thread *curr = thread_current();
  struct process *child = get_process_by_tid(tid, &curr->child_processes);
  sema_down(&child->exec_sema);

  return child->load_success ? tid : RET_ERROR;
}

/* Initialize the struct process for the passed thread, as a child of the
   current thread. Returns false if we failed to allocate space. */
bool
init_process (struct thread *t) {
  /* Allocate memory for the struct process. */
  struct process *proc = (struct process *) malloc(sizeof(struct process));

  if (proc == NULL) {
    return false;
  }

  proc->tid = t->tid;
  proc->return_status = RET_ERROR;
  proc->next_mapid = 1;
  proc->next_fd = 3;
  proc->load_success = false;
  proc->thread_dead = false;
  proc->parent_dead = false;
  proc->executable = NULL;

  sema_init(&proc->exec_sema, 0);
  sema_init(&proc->wait_sema, 0);

  list_push_back(&thread_current()->child_processes, &proc->child_elem);

  /* Process is now initialized fully. */
  t->process = proc;
  return true;
}

/* A thread function that loads a user command and starts it running. */
static void
start_process (void *command_)
{
  struct thread* t = thread_current();
  char *cmd = command_;

  /* Ensure <= 200 arguments. */
  int argc = count_args(cmd);
  if (argc > MAX_ARGS) {
    palloc_free_page (cmd);
    t->process->load_success = false;
    sema_up(&t->process->exec_sema);
    process_kill();
  }

  /* Tokenize command. */
  char *argv[argc];
  char *file_name;
  read_args(argv, &file_name, cmd);

  /* Initialize interrupt frame and load executable. */
  struct intr_frame if_;
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  bool success = load(file_name, &if_.eip, &if_.esp);

  t->process->load_success = success;
  sema_up(&t->process->exec_sema);

  /* Put arguments onto stack if we loaded the file sucessfully. */
  if (success) {
    push_args(&if_, argc, argv);
  }

  palloc_free_page (cmd);

  /* If load failed, quit. */
  if (!success) {
    process_kill();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Returns the number of arguments in a command (including program name). */
static int count_args(char* command_) {
  /* Make a local copy of the command to avoid modification. */
  int cmd_size = strlen(command_) + 1;
  char cmd[cmd_size];
  strlcpy(cmd, command_, cmd_size);

  int argc = 0;
  char *token, *save_ptr;
  for (token = strtok_r(cmd, " ", &save_ptr);
       token != NULL;
       token = strtok_r(NULL, " ", &save_ptr))
       {
         argc++;
       }

  return argc;
}

/* Splits the command into space separated tokens, and put them in the array
   argv. Also puts the first token into file_name. */
static void read_args(char **argv, char **file_name, char* cmd) {
  char *token, *save_ptr;
  /* Read the first argument (the file name). */
  *file_name = argv[0] = strtok_r(cmd, " ", &save_ptr);
  /* Tokenise the arguments and load into argv. */
  int i = 1;
  for (
    token = strtok_r(NULL, " ", &save_ptr);
    token != NULL;
    token = strtok_r(NULL, " ", &save_ptr)) {
      argv[i] = token;
      i++;
    }
}

/* Pushes the arguments held in argv onto the stack referenced by if_.*/
static void push_args(struct intr_frame *if_, int argc, char **argv) {
  uint32_t *arg_starts[argc];
  int cur_arg;

  /* Copy the argument strings onto the stack in reverse order. */
  for (cur_arg = argc - 1; cur_arg >= 0; cur_arg--) {
    if_->esp = strcpy_stack(argv[cur_arg], if_->esp);
    arg_starts[cur_arg] = if_->esp;
  }

  /* Word align the stack pointer. */
  if_->esp -= (uint32_t)if_->esp % WORDSIZE;

  /* Push a null pointer (sentinel) so that argv[argc] == 0. */
  push_word(NULL, if_);

  /* Push pointers to arguments in reverse order. */
  for (cur_arg = argc - 1; cur_arg >= 0; cur_arg--) {
    push_word(arg_starts[cur_arg], if_);
  }

  /* Push a pointer to the first pointer (argv). */
  push_word(if_->esp, if_);

  /* Push the number of arguments (argc). Cast argc to a uint32_t*
     because push_word works with those. */
  push_word((uint32_t *)argc, if_);

  /* Push a fake return address (0). */
  push_word((uint32_t *)0, if_);
}

/* Push a word to the stack referenced by if_ */
static void push_word(uint32_t *word, struct intr_frame *if_) {
  if_->esp -= WORDSIZE;
  *((uint32_t**)if_->esp) = word;
}

/* Copy a string backwards from src to dst. Returns the next free
   location on the stack after the end of the string. */
static char* strcpy_stack(char *src, char *dst) {
  int i;
  /* Make i point to the end of the string (\0 char). */
  for (i = 0; src[i] != '\0'; i++);
  /* Copy backwards into dst. */
  for (; i>=0; i--) {
    *(--dst) = src[i];
  }
  return dst;
}

/* Retrieve the process with the given tid from a list of processes. */
struct process*
get_process_by_tid(tid_t tid, struct list* processes)
{
  struct process* result = NULL;
  struct list_elem* e;
  for (e = list_begin(processes);
       e != list_end(processes);
       e = list_next(e)) {
    struct process* pr = list_entry(e, struct process, child_elem);
    if (pr->tid == tid) {
      result = pr;
      break;
    }
  }
  return result;
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{

  struct thread* curr = thread_current();
  struct process* child = get_process_by_tid(child_tid, &curr->child_processes);

  if (child == NULL) {
    return RET_ERROR;
  }

  sema_down(&child->wait_sema);

  /* Free resouces of the child process as they are no longer needed.
     As a side effect, this handles the case of double waiting. */
  list_remove(&child->child_elem);
  int return_status = child->return_status;
  free(child);
  return return_status;
}

/* Kill the current process. */
void
process_kill(void)
{
  exit(-1);
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current();
  uint32_t *pd;

  if (cur->process->executable != NULL) {
    file_close(cur->process->executable);
  }

  /* Notify all children that the parent thread is dead, and free all of the
   * child processes whose thread is no longer running. */
  while(!list_empty(&cur->child_processes)) {
    struct process* child = list_entry(list_pop_front(&cur->child_processes),
                                       struct process, child_elem);
    child->parent_dead = true;
    if (child->thread_dead) {
      free(child);
    }
  }

  /* Close all of the currently open files and free their descriptors. */
  while (!list_empty(&thread_current()->descriptors)) {
    struct descriptor *d = list_entry(
            list_pop_front(&thread_current()->descriptors),
            struct descriptor, elem);
    file_close(d->file);
    free(d);
  }

  /* If this thread is orphaned, free it's process struct. Otherwise, we need
     to notify the parent that this thread is exiting. */
  if (cur->process->parent_dead) {
    free(cur->process);
  } else {
    cur->process->thread_dead = true;
    sema_up(&cur->process->wait_sema);
  }

  #ifdef VM
    /* Unmap all memory mapped files, Also, deletes all entries in
       mmap_file_page_table. */
    hash_apply(&cur->mapid_page_table, unmap_elem);
    mmap_file_page_table_destroy(&cur->mmap_file_page_table);
    supp_page_table_destroy(&cur->supp_page_table);
    swap_table_destroy(&cur->swap_table);
  #endif

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

static void unmap_elem(struct hash_elem *elem, void *aux UNUSED) {
  struct mapid_to_addr *mta = hash_entry(elem, struct mapid_to_addr, hash_elem);
  munmap(mta->mapid);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }


  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  /* Set executable in process to the file pointer and deny any writes
     to the running executable. */
  t->process->executable = file;
  file_deny_write(t->process->executable);

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (!success) {
    file_close(file);
  }

  return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or insertion error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);

  struct thread* t = thread_current();
  off_t file_page_offset = ofs;

  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      if (page_zero_bytes == PGSIZE) {
        supp_page_table_insert(&t->supp_page_table, upage, ZEROED);
      } else {
        supp_page_table_insert(&t->supp_page_table, upage, MMAPPED);
        mmap_file_page_table_insert(&t->mmap_file_page_table, upage, 0, file,
            file_page_offset, page_read_bytes, writable);
      }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      file_page_offset += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
  uint8_t *kpage;
  bool success = false;

  enum intr_level old_level = intr_disable();

  struct thread *t = thread_current();

  uint8_t *page_addr = ((uint8_t *) PHYS_BASE) - PGSIZE;
  kpage = (uint8_t *) frame_get_page(page_addr);
  if (kpage != NULL) {
      success = install_page(page_addr, kpage, true);
      supp_page_table_insert(&t->supp_page_table, page_addr, LOADED);
      if (success)
        *esp = PHYS_BASE;
      else
        frame_free_page (kpage);
  }

  intr_set_level(old_level);

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
