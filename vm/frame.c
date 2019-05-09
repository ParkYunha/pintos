#include "vm/frame.h"
#include <stdint.h>
#include <stdbool.h>

#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"

 /* Initialize frame table. */
 /* Given in skeleton. */
void
frame_init (void)
{
  lock_init(&frame_table_lock);
  list_init(&frame_table_list);
}

/*pintos pdf */
/*The most important operation on the frame table is obtaining an unused frame.
  This is easy when a frame is free.
  When none is free, a frame must be made free by evicting some page from its frame.
  If no frame can be evicted without allocating a swap slot,
  but swap is full, panic the kernel.
  */

/* Make a new frame table entry for addr. */
/* Given in skeleton. */
void* //-> 제대로 allocate 됐는지 리턴하라는 건가??
allocate_frame (void *addr)
{

  // return palloc_get_page(PAL_USER); //for debugging

  lock_acquire(&frame_table_lock);
  
  void *frame_page = palloc_get_page(PAL_USER); //from user pool
  if(frame_page == NULL) {
    /* If page allocation failed. */
    //TODO: 뭔가 pdf에서 패닉해라 뭐해라 있었던 듯
    lock_release(&frame_table_lock);
    return NULL;
  }

  struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
  if(fte == NULL) {
    /* If frame allocation failed. */
    lock_release(&frame_table_lock);
    return NULL;
  }
  /* Set frame table entry. */
  fte->vaddr = addr;
  fte->frame = frame_page;
  fte->owner = thread_current();
  // fte->spte //TODO: ??

  list_push_back(&frame_table_list, &fte->elem);
  lock_release(&frame_table_lock);

  return frame_page;
}


//TODO: free frame function
//TODO: frame_evict()
