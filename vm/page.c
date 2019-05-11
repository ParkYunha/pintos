#include "vm/page.h"
#include "userprog/process.h"

/* Initialize supplementary page table. */
/* Given in skeleton. */
/* Init when a process starts. */
void page_init(struct hash *spt)
{
  hash_init(spt, page_hash_func, page_less_func, NULL);
}

/* Helper functions for page_init. */
//hash에 대한 함수는 lib/kernel/hash.* 참고

/* Return e's vaddr's hash number. */
static unsigned
page_hash_func(const struct hash_elem *e, void *aux UNUSED)
{
  const struct sup_page_table_entry *spte = hash_entry(e, struct sup_page_table_entry, hash_elem);
  return hash_bytes(&spte->user_vaddr, sizeof(&spte->user_vaddr));
  // return hash_int((int)spte->user_vaddr); // vaddr의  해시값 반환
}

/* Compare their vaddr => return true if a('s vaddr) is smaller than b */
static bool
page_less_func(const struct hash_elem *a, const struct hash_elem *b)
{
  struct sup_page_table_entry *spte_a = hash_entry(a, struct sup_page_table_entry, hash_elem);
  struct sup_page_table_entry *spte_b = hash_entry(b, struct sup_page_table_entry, hash_elem);
  return ((spte_a->user_vaddr) < (spte_b->user_vaddr)); //크기 비교 a의 vaddr이 작을 때 true
}



/* Make new supplementary page table entry for addr. */
/* Given in skeleton. */
struct sup_page_table_entry *
allocate_page(void *addr, struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));

  if (spte == NULL)
  {
    return NULL;
  }
  /*set sup_page_table_entry */
  spte->user_vaddr = pg_round_down(addr); //addr이 포함되는 page 시작 주소를 반환
  spte->file = file; 
  spte->writable = writable;
  spte->is_loaded = false;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->offset = ofs;

  hash_insert(&thread_current()->page_table, &spte->hash_elem); //hash 에 elem 넣어주기
  return spte;
}

/* expend stack by 1 page. */
bool 
stack_growth (void * uv_addr)
{
  // check if it is in valid stack range. //
  if ((size_t) (PHYS_BASE - pg_round_down(uv_addr)) > MAX_STACK_SIZE)
  {
      return false;
  }
  // Create spte. //
  struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
  if (spte == NULL)
  {
    return false;
    }
    // Setup spte. //
    spte->user_vaddr = pg_round_down(uv_addr);
    spte->is_loaded = true;
    spte->writable = true;
  //allocate frame //
  uint8_t *frame = allocate_frame (spte, PAL_USER);
  if (!frame) //if failed alllocation
    {
      free(spte);
      return false;
    }
  // install page //
  if (!install_page(spte->user_vaddr, frame, spte->writable))
    {
      free(spte);
      frame_free(frame);
      return false;
    }

  // if (intr_context())
  //   {
  //     spte->pinned = false;
  //   }

  // Put entry into page table(hash). //
  return (hash_insert(&thread_current()->page_table, &spte->hash_elem) == NULL);
}






/* addr이 포함된 spte를 spt에서 찾는 함수*/
/* Find a spte including addr from spt and return it(spte) */
struct sup_page_table_entry *
find_spte(struct hash *spt, void *addr)
{
  struct sup_page_table_entry spte;
  
  spte.user_vaddr = pg_round_down(addr);
  struct hash_elem *h_elem = NULL;
  h_elem = hash_find(spt, &spte.hash_elem);
  if (h_elem == NULL)
  {
    return NULL;
  }

  struct sup_page_table_entry* spte2 = hash_entry(h_elem, struct sup_page_table_entry, hash_elem);
  return spte2;
}

/* Insert spte to spt. */
bool
insert_spte(struct hash *spt, struct sup_page_table_entry *spte)
{
  struct hash_elem *h_elem;
  h_elem = hash_insert(spt, &spte->hash_elem);

  if (h_elem != NULL) //same spte already there
  {
    return false;
  }
  return true;
}

/* Remove(delete) spte from spt. */
bool
remove_spte(struct hash *spt, struct sup_page_table_entry *spte)
{
  struct hash_elem *h_elem;
  h_elem = hash_delete(spt, &spte->hash_elem);

  if (h_elem != NULL) //same spte already there
  {
    return false;
  }
  return true;
}


/* Helper function for destroy_spte. */
void spt_destructor(struct hash_elem *elem)
{
  struct sup_page_table_entry *spte = hash_entry(elem, struct sup_page_table_entry, hash_elem);
  if (spte->is_loaded)
    {
      frame_free(pagedir_get_page(thread_current()->pagedir, spte->user_vaddr));
      pagedir_clear_page(thread_current()->pagedir, spte->user_vaddr);
    }
  free(spte);
}

/* Remove(destory) the spt. */
void
destroy_spt(struct hash *spt)
{
  hash_destroy(spt, spt_destructor);
}



/* load page <- file */
bool load_page_file(struct sup_page_table_entry *spte)
{
  // file_seek(spte->file, spte->offset);
  enum palloc_flags flags = PAL_USER;
  if(spte->read_bytes == 0){
    flags = (PAL_USER | PAL_ZERO);
  }
  void *frame_page = allocate_frame(spte->user_vaddr, flags); 

  if (frame_page != NULL)
  {
    sema_down(&file_sema);
    int rfile = file_read_at(spte->file, frame_page, spte->read_bytes, spte->offset);
    sema_up(&file_sema);
    if (rfile == (off_t)spte->read_bytes)
    {
      memset(frame_page + spte->read_bytes, 0, spte->zero_bytes);

      if(! install_page(spte->user_vaddr, frame_page, spte->writable)) {
        frame_free(frame_page);
        return false;
      }
    } else { 
      frame_free(frame_page);
      return false;
    }
  }
  spte->is_loaded = true;
  return true;
}

// TODO: load_page_swap, load_page_mmap 도 나중에 만들어줘야할듯
