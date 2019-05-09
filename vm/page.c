#include "vm/page.h"

/* Initialize supplementary page table. */
/* Given in skeleton. */
void
page_init (struct hash *spt)
{
  hash_init(spt, page_hash, page_less, NULL);
}

/* Make new supplementary page table entry for addr. */
/* Given in skeleton. */
struct sup_page_table_entry *
allocate_page (void *addr)
{

}

//hash에 대한 함수는 lib/kernel/hash.* 참고
/* Helper functions for hash_init. */
static unsigned page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct sup_page_table_entry *spte = hash_entry(e, struct sup_page_table_entry, hash_elem);
  return hash_int(int(spte->user_vaddr)); // vaddr의  해시값 반환
}

static bool page_less (const struct hash_elem *a, const struct hash_elem *b)
{
  struct sup_page_table_entry *spte_a = hash_entry(a, struct sup_page_table_entry, hash_elem);
  struct sup_page_table_entry *spte_b = hash_entry(b, struct sup_page_table_entry, hash_elem);
  return ( (spte_a->user_vaddr) < (spte_b->user_vaddr) ); //크기 비교 a의 vaddr이 작을 때 true
}
