#include "vm/swap.h"
#include "devices/disk.h"
#include "threads/synch.h"
#include <bitmap.h>


/* The swap device */
static struct disk *swap_device;

/* Tracks in-use and free swap slots */
static struct bitmap *swap_table;

/* Protects swap_table */
static struct lock swap_lock;

/*
 * Initialize swap_device, swap_table, and swap_lock.
 */
void
swap_init (void)
{
  // get swap disk
  swap_device = disk_get(1,1);
  // make swap table , size of - (bit하나가 관리하는 disk크기)
  swap_table = bitmap_create(disk_size(swap_device)/(PGSIZE/DISK_SECTOR_SIZE));
  bitmap_set_all(swap_table, 0);
  lock_init(&swap_lock);
}

/*
 * Reclaim a frame from swap device.
 * 1. Check that the page has been already evicted.
 * 2. You will want to evict an already existing frame
 * to make space to read from the disk to cache.
 * 3. Re-link the new frame with the corresponding supplementary
 * page table entry.
 * 4. Do NOT create a new supplementray page table entry. Use the
 * already existing one.
 * 5. Use helper function read_from_disk in order to read the contents
 * of the disk into the frame.
 */
bool
swap_in (void *addr)
{
  struct sup_page_table_entry * spte = find_spte(&thread_current()->page_table, addr);
  uint8_t *frame = allocate_frame(addr, PAL_USER);
  if(!install_page(spte->user_vaddr, frame, spte->writable))
  {
    frame_free(frame);
    return false;
  }
  read_from_disk(frame, spte->swap_index);
  spte->is_loaded = true;
  return true;
}

/*
 * Evict a frame to swap device.
 * 1. Choose the frame you want to evict.
 * (Ex. Least Recently Used policy -> Compare the timestamps when each
 * frame is last accessed)
 * 2. Evict the frame. Unlink the frame from the supplementray page table entry
 * Remove the frame from the frame table after freeing the frame with
 * pagedir_clear_page.
 * 3. Do NOT delete the supplementary page table entry. The process
 * should have the illusion that they still have the page allocated to
 * them.
 * 4. Find a free block to write you data. Use swap table to get track
 * of in-use and free swap slots.
 */
void *
swap_out (enum palloc_flags flags)
{
  lock_acquire(&frame_table_lock);
  struct list_elem *frame_elem;
  void* addr;
  while(true)
  {
    for (frame_elem = list_begin(&frame_table_list); frame_elem != list_end(&frame_table_list); frame_elem = list_next(frame_elem))
    {
      struct frame_table_entry *fte = list_entry(frame_elem, struct frame_table_entry, elem);
      //uint32_t ft_pagedir = fte->owner->pagedir;
      //uint8_t uvaddr = fte->spte->user_vaddr;
      if(! fte->spte->accessed_bit && pagedir_is_accessed(fte->owner->pagedir, fte->spte->user_vaddr))
      {
        pagedir_set_accessed(fte->owner->pagedir, fte->spte->user_vaddr, false);
      }
      else if(! fte->spte->accessed_bit)
      {
        if(pagedir_is_dirty(fte->owner->pagedir, fte->spte->user_vaddr) || fte->spte->type == 1)
        {
          if(fte->spte->type == 0)
          {
            fte->spte->type = 1;
          }
          if(fte->spte->type == 2)
          {
            file_write_at(fte->spte->file,fte->spte->user_vaddr, fte->spte->read_bytes, fte->spte->offset);
          }
        }

        //find first 0 bit  and  flip it
        int free_index = bitmap_scan_and_flip(swap_table, 0, 1, 0);

        fte->spte->swap_index = write_to_disk((uint8_t*)fte->frame, free_index);

        fte->spte->is_loaded = false;
        list_remove(&fte->elem);
        lock_release(&frame_table_lock);
        pagedir_clear_page(fte->owner->pagedir, fte->spte->user_vaddr);
        palloc_free_page(fte->frame);
        free(fte);


        addr = palloc_get_page(flags);
        if(addr) break;
      }
    }
    return addr;
  }



}

/*
 * Read data from swap device to frame.
 * Look at device/disk.c
 */
void read_from_disk (uint8_t *frame, int index)
{
  lock_acquire(&swap_lock);
  int i=0;
  while(i<8)
  {
      disk_read(swap_device, index * 8 + 1, (uint8_t*)frame + i * DISK_SECTOR_SIZE);
      i++;
  }
  bitmap_flip(swap_table, index);
  lock_release(&swap_lock);
}

/* Write data to swap device from frame */
int write_to_disk (uint8_t *frame, int index)
{
  lock_acquire(&swap_lock);
  int i;
  for (i=0; i<(PGSIZE/DISK_SECTOR_SIZE);i++)
  {
    disk_write(swap_device, index * (PGSIZE/DISK_SECTOR_SIZE) + i, (uint8_t*) frame + i * DISK_SECTOR_SIZE);
  }
  lock_release(&swap_lock);
  return index;

}
