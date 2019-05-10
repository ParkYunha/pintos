#include "vm/page.h"



/* Initialize supplementary page table. */
/* Given in skeleton. */
void //
page_init (struct hash *spt)
{
  hash_init(spt, page_hash_func, page_less_func, NULL);
}

/* Make new supplementary page table entry for addr. */
/* Given in skeleton. */
struct sup_page_table_entry *
allocate_page (void *addr)
{
  struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));

  if (spte == NULL){
    return NULL;
  }

  /*set sup_page_table_entry */
  spte->user_vaddr = pg_round_down(addr);//addr이 포함되는 page 시작 주소를 반환
  //spte->file = NULL;
  //spte->read_bytes = 0;
  //spte->zero_bytes = 0;
  spte->offset = 0;

  hash_insert(thread_current()->page_table, &spte->hash_elem);//hash 에 elem 넣어주기

}

/* Helper functions for page_init. */
//hash에 대한 함수는 lib/kernel/hash.* 참고

/* Return e's vaddr's hash number. */
static unsigned
page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  const struct sup_page_table_entry *spte = hash_entry(e, struct sup_page_table_entry, hash_elem);
  return hash_int((int)spte->user_vaddr); // vaddr의  해시값 반환
}

/* Compare their vaddr => return true if a('s vaddr) is smaller than b */
static bool
page_less_func (const struct hash_elem *a, const struct hash_elem *b)
{
  struct sup_page_table_entry *spte_a = hash_entry(a, struct sup_page_table_entry, hash_elem);
  struct sup_page_table_entry *spte_b = hash_entry(b, struct sup_page_table_entry, hash_elem);
  return ( (spte_a->user_vaddr) < (spte_b->user_vaddr) ); //크기 비교 a의 vaddr이 작을 때 true
}
