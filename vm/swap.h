#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdint.h> //unint32_t
#include <stdbool.h>
#include <list.h>
#include <hash.h>
#include <debug.h>

#include "vm/page.h"
#include "vm/frame.h"

#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

void swap_init (void);
bool swap_in (void *addr);
bool swap_out (void);
void read_from_disk (uint8_t *frame, int index);
void write_to_disk (uint8_t *frame, int index);

#endif /* vm/swap.h */
