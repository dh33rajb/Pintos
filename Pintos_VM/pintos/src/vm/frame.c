#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "vm/frame.h"

/* Initialize the frame manager. */
void frame_init(void) {
	list_init(&frame_list); // initalizing frames list
	lock_init(&ftable_lock); // initializing frame table lock
}

/* Tries to allocate and lock a frame for PAGE.
 Returns the frame if successful, false on failure. */
struct frame *
frame_alloc_and_lock(struct page *page) {
	struct frame *new_frame;
	uint8_t *kpage;
	while (true) {
		kpage = palloc_get_page(PAL_USER);
		if (kpage != NULL) {
			lock_acquire(&ftable_lock);
			new_frame = (struct frame *) malloc(sizeof(struct frame));
			if (page->swap_flag != 1)
				page->swap_flag = 0;
			page->frame_holder_thread = thread_current();
			new_frame->page = page;
			new_frame->base = kpage;
			list_insert(list_end(&frame_list), &new_frame->frame_list_elem);
			// new_frame->page->swap_flag = 0;
			lock_release(&ftable_lock);
			break;
		} else {
			// lock_release(&ftable_lock);
			// PANIC("No free page available");
			// page eviction comes here
			// printf("\nEntered EVICT PAGE section");
			evict_page();
			// page->swap_flag = 1;
			// printf("\nExited EVICT PAGE section");
		}
	}
	return new_frame;
}

/* The implementation of LRU 'clock' algorithm follows.. we make use of the PTE's accessed bit and dirty bit:
 on any read or write to a page  ==> accessed bit = 1;
 on any write ==> dirty bit = 1;

 Step 1: We navigate through the pages in a circular order.. i.e., if we hit the end of the frame list,
 we circle back to the start and continue our processing..

 Step 2: If the accessed bit is 'accessed', turn it to 'not accessed', skip the page and proceed to look-up the next one in the list

 Step 3: If the accessed bit is 'not accessed', we can proceed with our page replacement */
void evict_page() {
	struct list_elem *e;
	int count = 0;
	lock_acquire(&ftable_lock);
	for (e = list_begin(&frame_list); e != list_end(&frame_list);
			e = list_next(e)) {
		struct frame *frame = list_entry(e, struct frame, frame_list_elem);
		// printf ("\n%d , %d", count, list_size(&frame_list) - 1);
		if (count != list_size(&frame_list) - 1) {
			// get the accessed flag for the current page
			bool accessed_flag = pagedir_is_accessed(
					frame->page->frame_holder_thread->pagedir, frame->page->addr);
			if (accessed_flag)
				pagedir_set_accessed(frame->page->frame_holder_thread->pagedir,
						frame->page->addr, !accessed_flag);
			else {
				// we need to page replace.. i.e.,
				// step 1: remove the existing page from the frame (swap_out call)
				// step 2: remove the existing page from the page directory
				// step 3: set the accessed flag of the page to 'accessed'
				// step 4: free the page and free the frame, for subsequent use
				list_remove(e);
				swap_out(frame->page);
				frame->page->swap_flag = 1;
				pagedir_clear_page(frame->page->frame_holder_thread->pagedir,
						frame->page->addr);
				palloc_free_page(frame->base);
				free(frame);
				// frame->page->swap_flag = 1;
				break;
			}
			count++;
		} else {
			count = 0;
			e = list_begin(&frame_list);
		}
	}
	lock_release(&ftable_lock);
}

void frame_free(struct frame *f) {
	// here we free the frame held by the process
	lock_acquire(&ftable_lock);
	struct list_elem *elem = list_begin(&frame_list);
	while (elem != list_end(&frame_list)) {
		struct frame *fe = list_entry(elem, struct frame, frame_list_elem);
		if (fe != f) {
			elem = list_next(elem);
			continue;
		}
		// if current process frame that is to be freed is in the list, then remove it from list and free memory
		list_remove(elem);
		pagedir_clear_page(fe->page->frame_holder_thread->pagedir, fe->page->addr);
		palloc_free_page(fe->base); // free the page currently held by the frame
		free(fe); // then free the frame
		break;
	}
	lock_release(&ftable_lock);
}
