#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

/* Virtual page. */
struct page {
	void *addr; /* User virtual address. */
	struct frame *frame; /* Page frame. */
	/* other struct members as necessary */

	// file related data will be saved here
	struct file *file;
	uint32_t read_bytes;
	off_t file_ofs;bool writable;
	struct hash_elem spt_hash_elem;
	// swap data
	block_sector_t swap_sector;
	int swap_flag; // a swap indicator flag
	struct thread *frame_holder_thread;
};

void page_exit(void);
//struct page *page_allocate (void *, bool read_only);
struct page *
page_allocate(struct file *file, off_t ofs, uint32_t read_bytes, void *vaddr,
bool read_only);
void page_deallocate(void *vaddr);
bool page_in(void *fault_addr);
bool page_out(struct page *);
bool page_accessed_recently(struct page *);
bool page_lock(const void *, bool will_write);
void page_unlock(const void *);
hash_hash_func page_hash;
hash_less_func page_less;

#endif /* vm/page.h */
