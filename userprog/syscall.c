#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include <kernel/list.h>
#include "threads/interrupt.h"
#include "threads/thread.h" //->file_sema
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/off_t.h" /* new */
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

static void syscall_handler(struct intr_frame *);
void userp_exit(int status);
struct sup_page_table_entry *check_valid_spte(const void *vaddr, void *esp);
void check_valid_uvaddr(const void *str, unsigned size, void *esp, bool is_buffer, bool write);
void check_valid_pointer(const void *vaddr);

struct file
{
  struct inode *inode; /* File's inode. */
  off_t pos;           /* Current position. */
  bool deny_write;     /* Has file_deny_write() been called? */
};
// ctrl c+v from filesys/file.c

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
/* Check null, unmapped */
static int
get_user(const uint8_t *uaddr)
{
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a"(result)
      : "m"(*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user(uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "r"(byte));
  return error_code != -1;
}

void check_valid_pointer(const void *vaddr)
{
  if (!is_user_vaddr(vaddr)) // || vaddr >= PHYS_BASE || vaddr == NULL)
  {
    // printf("%s: exit(%d)\n", thread_name(), -1);
    // thread_exit();
    userp_exit(-1);
  }
}

struct sup_page_table_entry *
check_valid_spte(const void *vaddr, void *esp)
{
  struct sup_page_table_entry *spte = find_spte(&thread_current()->page_table, (void *)vaddr);
  if (spte)
  {
    load_page_file(spte);
    if (!spte->is_loaded)
    {
      userp_exit(-1);
    }
  }
  else if (vaddr - esp <= 32)
  {
    if (!stack_growth((void *)vaddr))
    {
      userp_exit(-1);
    }
  }
  return spte;
}

void check_valid_uvaddr(const void *str, unsigned size, void *esp, bool is_buffer, bool write)
{
  if (is_buffer)
  {
    char *buffer = (char *)str;
    unsigned i = 0;
    while (i < size)
    {
      struct sup_page_table_entry *spte = check_valid_spte(str, esp);
      if (spte && write && !spte->writable)
      {
        userp_exit(-1);
      }
      i++;
      buffer++;
    }
  }
  else
  {
    check_valid_spte(str, esp);
    while (*(char *)str != 0)
    {
      str = (char *)str + 1;
      check_valid_spte(str, esp);
    }
  }
}

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
  // ASSERT(f!= NULL);
  // ASSERT(f->esp != NULL);
  // ASSERT(pagedir_get_page(thread_current()->pagedir, f->esp) != NULL);

  //sc-bad-sp
  check_valid_pointer(f->esp);
  check_valid_spte(f->esp, f->esp);
  if (get_user((uint8_t *)f->esp) == -1)
  {
    userp_exit(-1);
  }
  if (get_user((uint8_t *)f->esp + 4) == -1)
  {
    userp_exit(-1);
  }
  if (get_user((uint8_t *)f->esp + 8) == -1)
  {
    userp_exit(-1);
  }

  //pt-grow-stk-sc
  thread_current()->stack = f->esp;

  int sys_num = *(uint32_t *)(f->esp);

  int first = *((int *)((f->esp) + 4)); //fd or file or pid
  void *second = *((void **)((f->esp) + 8));
  unsigned third = *((unsigned *)((f->esp) + 12));

  int i;

  switch (sys_num)
  {
  //syscall0 (SYS_HALT);
  case SYS_HALT: //0
  {
    //halt();
    power_off();
    break;
  }

  //syscall1 (SYS_EXIT, status);
  case SYS_EXIT: //1
  {
    check_valid_pointer((f->esp) + 4); //status
    int status = (int)*(uint32_t *)((f->esp) + 4);

    userp_exit(status);
    break;
  }

  //syscall1 (SYS_EXEC, file);
  case SYS_EXEC: //2
  {
    check_valid_pointer((f->esp) + 4); //file = first
    check_valid_uvaddr((f->esp) + 4, NULL, f->esp, false, false);
    f->eax = process_execute(*(const char **)(f->esp + 4));
    //process_execute(*(char **)((f->esp) + 4));
    break;
  }

  //syscall1 (SYS_WAIT, pid);
  case SYS_WAIT: //3
  {
    check_valid_pointer((f->esp) + 4); //pid = tid = first
    f->eax = process_wait((tid_t)first);
    // process_wait(thread_tid());
    break;
  }

  //syscall2 (SYS_CREATE, file, initial_size);
  case SYS_CREATE: //4
  {
    if (first == NULL)
    {
      userp_exit(-1);
    }
    check_valid_pointer((f->esp) + 4); //file = first
    check_valid_pointer((f->esp) + 8); //initial_size = second
    check_valid_pointer(second);       //also a pointer
    check_valid_uvaddr((f->esp) + 4, NULL, f->esp, false, false);

    sema_down(&file_sema);
    f->eax = filesys_create((const char *)first, (int32_t)(second));
    sema_up(&file_sema);
    break;
  }

  //syscall1 (SYS_REMOVE, file);
  case SYS_REMOVE: //5
  {
    if (first == NULL)
    {
      userp_exit(-1);
    }
    check_valid_pointer((f->esp) + 4); //file = first
    check_valid_uvaddr((f->esp) + 4, NULL, f->esp, false, false);
    sema_down(&file_sema);
    f->eax = filesys_remove((const char *)first);
    sema_up(&file_sema);
    break;
  }

  //syscall1 (SYS_OPEN, file);
  case SYS_OPEN: //6
  {
    if (first == NULL)
    {
      userp_exit(-1);
    }

    check_valid_pointer((f->esp) + 4);           //file = first
    check_valid_pointer(*(char **)(f->esp + 4)); //also a pointer
    check_valid_uvaddr((f->esp) + 4, NULL, f->esp, false, false);
    // if(get_user((uint8_t *)(f->esp + 4)) == -1) //check if null or unmapped
    // {
    //   exit(-1);
    // }

    struct file *file = *(char **)(f->esp + 4);
    sema_down(&file_sema);
    struct file *fp = filesys_open(*(char **)(f->esp + 4));
    sema_up(&file_sema);

    if (fp == NULL) //file could not opened
    {
      f->eax = -1;
    }
    else
    {
      f->eax = -1;
      sema_down(&file_sema);
      if (strcmp(thread_current()->name, file) == 0)
      {
        file_deny_write(fp);
      }
      sema_up(&file_sema);

      for (i = 3; i < 128; ++i)
      {
        if (thread_current()->f_d[i] == NULL)
        {

          thread_current()->f_d[i] = fp;
          f->eax = i;
          break; //end for loop
        }
      }
    }

    break; //end open
  }

  //syscall1 (SYS_FILESIZE, fd);
  case SYS_FILESIZE: //7
  {
    if (thread_current()->f_d[first] == NULL)
    {
      userp_exit(-1);
    }
    // if(fd == NULL)
    // {
    //   exit(-1);
    // }
    check_valid_pointer((f->esp) + 4); //fd = first
    sema_down(&file_sema);
    f->eax = file_length(thread_current()->f_d[first]);
    sema_up(&file_sema);
    break;
  }

  //syscall3 (SYS_READ, fd, buffer, size);
  case SYS_READ: //8
  {
    check_valid_pointer((f->esp) + 4);  //fd = first
    check_valid_pointer((f->esp) + 8);  //buffer = second
    check_valid_pointer((f->esp) + 12); //size = third
    check_valid_pointer(second);        //also a pointer
    check_valid_uvaddr((f->esp) + 8, third, f->esp, true, true);

    if (get_user((uint8_t *)(f->esp + 4)) == -1) //check if null or unmapped
    {
      userp_exit(-1);
    }

    int i;
    if (first == 0) //stdin: keyboard input from input_getc()
    {
      for (i = 0; i < third; ++i)
      {
        if (put_user(second++, input_getc()) == -1)
        {
          break;
        }
        // if(((char *)second)[i] == NULL)
        // {
        //   break; //remember i
        // }
      }
    }
    else if (first > 2) //not stdin
    {
      if (thread_current()->f_d[first] == NULL)
      {
        userp_exit(-1);
      }
      if (get_user(second) == -1) /* Check validity of buffer. */
      {
        userp_exit(-1);
      }
      sema_down(&file_sema);
      f->eax = file_read(thread_current()->f_d[first], second, third);
      sema_up(&file_sema);
      break; //end read
    }
    f->eax = i;
    break;
  }

  //syscall3 (SYS_WRITE, fd, buffer, size);
  case SYS_WRITE: //9
  {
    check_valid_pointer((f->esp) + 4);  //fd = first
    check_valid_pointer((f->esp) + 8);  //buffer = second
    check_valid_pointer((f->esp) + 12); //size = third
    check_valid_pointer(second);        //also a pointer
    check_valid_uvaddr((f->esp) + 8, third, f->esp, true, false);

    //check buffer validity
    for (i = 0; i < third; ++i)
    {
      if (get_user(second + i) == -1) //!is_user_vaddr(second + i)
      {
        userp_exit(-1);
      }
    }

    int fd = first;
    if (fd == 1) //stdout: console io
    {
      putbuf(second, third);
      f->eax = third;
      break; //end write
    }
    else if (fd > 2) //not stdout
    {
      if (thread_current()->f_d[fd] == NULL)
      {
        userp_exit(-1);
      }
      if (thread_current()->f_d[fd]->deny_write)
      {
        sema_down(&file_sema);
        file_deny_write(thread_current()->f_d[fd]);
        sema_up(&file_sema);
      }

      sema_down(&file_sema);
      f->eax = file_write(thread_current()->f_d[fd], second, third);
      sema_up(&file_sema);
      break; //end write
    }
    f->eax = -1;
    break;
  }

  //syscall2 (SYS_SEEK, fd, position);
  case SYS_SEEK: //10
  {
    int fd = first;
    if (thread_current()->f_d[fd] == NULL)
    {
      userp_exit(-1);
    }
    check_valid_pointer((f->esp) + 4); //fd = first
    check_valid_pointer((f->esp) + 8); //buffer = second
    check_valid_pointer(second);       //also a pointer

    sema_down(&file_sema);
    file_seek(thread_current()->f_d[fd], (unsigned)second);
    sema_up(&file_sema);
    break;
  }

  //return syscall1 (SYS_TELL, fd);
  case SYS_TELL: //11
  {
    int fd = first;
    if (thread_current()->f_d[fd] == NULL)
    {
      userp_exit(-1);
    }
    check_valid_pointer((f->esp) + 4); //fd = first

    sema_down(&file_sema);
    file_tell(thread_current()->f_d[fd]);
    sema_up(&file_sema);
    break;
  }

  //syscall1 (SYS_CLOSE, fd);
  case SYS_CLOSE: //12
  {
    int fd = first;
    if (thread_current()->f_d[fd] == NULL)
    {
      userp_exit(-1);
    }
    check_valid_pointer((f->esp) + 4); //fd = first

    sema_down(&file_sema);
    file_allow_write(thread_current()->f_d[fd]);
    file_close(thread_current()->f_d[fd]);
    sema_up(&file_sema);

    thread_current()->f_d[fd] = NULL; //file closed -> make it NULL
    break;
  }

  /* VM: PROJECT3 */
  //syscall2 (SYS_MMAP, fd, addr);
  case SYS_MMAP: //13
  {
    int fd = first;
    void *addr = second;

    f->eax = (uint32_t)mmap(fd, addr);

    break;
  }

  //syscall1 (SYS_MUNMAP, mapid);
  case SYS_MUNMAP: //14
  {
    mapid_t mapid = (mapid_t)first;
    check_valid_pointer((f->esp) + 4); //fd = first

    munmap(mapid);
    break;
  }

  } // End of switch(sys_num)
} // End of syscall_handler()

/* VM: PROJECT3 */
mapid_t mmap(int fd, void *addr)
{
  /* Check validity of fd, addr. */
  if (thread_current()->f_d[fd] == NULL || !is_user_vaddr(addr) || get_user(addr) == -1 || (int)addr == 0 //virtual page address 0 is not mapped in pintos
      || ((int)addr % PGSIZE) != 0                                                                        //addr is not page-aligned
      || fd <= 1)
  {
    return MAP_FAILED;
  }

  /* Open file fd. */
  sema_down(&file_sema);

  struct file *f = thread_current()->f_d[fd]; //assure not null
  struct file *f_copy = NULL;

  f_copy = file_reopen(f);

  if (!f_copy || !file_length(f_copy))
  {
    sema_up(&file_sema);
    return MAP_FAILED;
  }

  /* Set up mapid. */
  mapid_t mapid;
  if (!list_empty(&thread_current()->mmap_list))
  {
    mapid = list_entry(list_back(&thread_current()->mmap_list), struct mmap_file, elem)->mapid + 1;
  }
  else //start of a empty list
  {
    mapid = 1;
  }

  /* Create and set up 'mmap_file'. */
  struct mmap_file *mfile = (struct mmap_file *)malloc(sizeof(struct mmap_file));
  mfile->mapid = mapid;
  mfile->file = f_copy;
  list_push_back(&thread_current()->mmap_list, &mfile->elem);

  /* Create and set up spte: Map each page of the file to the filesystem. */

  size_t offset;
  for (offset = 0; offset < file_length(f_copy); offset += PGSIZE) 
  {
    void *file_addr = addr + offset;

    size_t read_bytes = (offset + PGSIZE < file_length(f_copy) ? PGSIZE : file_length(f_copy) - offset);
    size_t zero_bytes = PGSIZE - read_bytes;

    struct sup_page_table_entry *spte;
    spte = (struct sup_page_table_entry*) malloc(sizeof(struct sup_page_table_entry));

    spte->user_vaddr = addr;
    spte->dirty_bit = false;
    spte->accessed_bit = false; //FIXME: ???
    spte->writable = true;
    spte->is_loaded = true; //FIXME: ???
    spte->file = f_copy;
    spte->offset = offset;
    spte->read_bytes = read_bytes;
    spte->zero_bytes = zero_bytes;

    struct hash_elem *e;
    e = hash_insert (&thread_current()->page_table, &spte->hash_elem);
    if(e) //mapping overlap
    {
      sema_up(&file_sema);
      return MAP_FAILED;
    }
  }

  sema_up(&file_sema);
  return mapid;
}

void munmap(mapid_t mapping)
{
  struct thread *t = thread_current();
  /* Find mmap_file (whose mapid = mapping) from mmap_list. */
  struct mmap_file *mfile = NULL;
  struct list_elem *e;
  if(list_empty(&t->mmap_list))
  {
    //error-handling
    userp_exit(-1);
  }
  for(e = list_begin(&t->mmap_list); e != list_end(&t->mmap_list); e = list_next(e))
  {
    mfile = list_entry(e, struct mmap_file, elem);
    if(mfile->mapid == mapping)
    {
      break;
    }
  }

  if(mfile == NULL)
  {
    //error handling
    userp_exit(-1); //FIXME: 맞나
  }

  /* Delete sptes. */
  sema_down(&file_sema);
  struct list_elem *le;
  struct sup_page_table_entry *spte = NULL;
  for(le = list_begin(&mfile->mmap_sptes); le != list_end(&mfile->mmap_sptes); le = list_next(le))
  {
    spte = list_entry(le, struct sup_page_table_entry, map_elem);
    hash_delete(&thread_current()->page_table, &spte->hash_elem);
  }
  /* Delete mmap_file. */
  list_remove(&mfile->elem);
  file_close(mfile->file);
  free(mfile);

  sema_up(&file_sema);
}



void userp_exit(int status) //userprog_exit
{
  int i;
  thread_current()->exit_status = status;
  for (i = 3; i < 128; ++i)
  {
    if (thread_current()->f_d[i] != NULL) //close all files before die
    {
      sema_down(&file_sema);
      file_close(thread_current()->f_d[i]);
      sema_up(&file_sema);
    }
  }
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}
