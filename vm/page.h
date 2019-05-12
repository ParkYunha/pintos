#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h> //unint32_t
#include <stdbool.h>
#include <string.h>
#include <list.h>
#include <hash.h>
#include <debug.h>
#include <syscall.h>

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

#define MAX_STACK_SIZE (1 << 23) /* 8MB */

struct sup_page_table_entry
{
	uint32_t* user_vaddr;
	uint64_t access_time;

	int type; // 0:FILE  1:SWAP  2:MMAP //TODO: change to enum for readability

	bool dirty_bit;
	bool accessed_bit;
	bool writable;
	bool is_loaded;

	uint32_t read_bytes; // page에 쓰여져 있는 데이터 크기
	uint32_t zero_bytes; // 남은 페이지의 크기, 0으로 채우려고
	struct file* file; // vaddr랑 맵핑된 파일
	uint32_t offset;

	size_t swap_index;

	struct hash_elem hash_elem;	/* Hash-elem for (supplemantary) page_table. */
	struct list_elem map_elem;  /* List-elem for mmap_sptes. */
};

struct mmap_file
{
	mapid_t mapid;
	struct file* file;			/* Keep tracks of open files. */
	struct list_elem elem;	/* List-elem for mmap_list. */
	struct list mmap_sptes; /* List of sptes corresponding to the mmapped file. */

}

void page_init (struct hash *spt);
struct sup_page_table_entry *allocate_page (void *addr, struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

unsigned page_hash_func (const struct hash_elem *e, void *aux UNUSED);
bool page_less_func (const struct hash_elem *a, const struct hash_elem *b);

struct sup_page_table_entry *find_spte(struct hash *spt, void *addr);
bool load_page_file(struct sup_page_table_entry *spte);

struct sup_page_table_entry * find_spte(struct hash *spt, void *addr);
bool insert_spte(struct hash *spt, struct sup_page_table_entry *spte);
bool remove_spte(struct hash *spt, struct sup_page_table_entry *spte);
void spt_destructor(struct hash_elem *elem);
void destroy_spt(struct hash *spt);

bool stack_growth (void * uv_addr);

#endif /* vm/page.h */
