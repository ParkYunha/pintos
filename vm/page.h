#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h> //unint32_t
#include <stdbool.h>
#include <list.h>
#include <hash.h>
#include <debug.h>

#include "vm/page.h"
#include "threads/palloc.h"

struct sup_page_table_entry
{
	uint32_t* user_vaddr;
	uint64_t access_time;

	bool dirty;
	bool accessed;

	struct hash_elem hash_elem; //
};

void page_init (struct hash *spt);
struct sup_page_table_entry *allocate_page (void *addr);
// static unsigned page_hash (const struct hash_elem *e, void *aux);
// static bool page_less (const struct hash_elem *a, const struct hash_elem *b);

#endif /* vm/page.h */
