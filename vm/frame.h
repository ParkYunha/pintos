#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h> //unint32_t
#include <stdbool.h>
#include <list.h>

#include "vm/page.h"
#include "threads/palloc.h"

struct frame_table_entry
{
	uint32_t* frame;
	uint32_t* vaddr;
	struct thread* owner;
	struct sup_page_table_entry* spte;

	struct list_elem elem; //for frame_tables list
};

struct lock frame_table_lock;
struct list frame_table_list;


void frame_init (void);
void frame_free(void *frame);
void* allocate_frame (void *addr);

#endif /* vm/frame.h */
