#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

/* Maximum size of process stack, in bytes. */
#define STACK_MAX (1024 * 1024)

/* Faults in the page containing FAULT_ADDR.
 Returns true if successful, false on failure. */
bool page_in(void *fault_addr) {
	struct page pge;
	struct page *p = &pge; // temp fix
	pge.addr = (void *) pg_round_down(fault_addr);
	// check if the page is present in the suppl page table?
	// if yes, return false... i.e. the page already has a frame assigned to it, no need to reassign it.
	// if no, allocate a new frame, read the page, install the page (make an entry into pageDir) and make an entry into the suppl page table
	struct hash_elem *elem = hash_find(&thread_current()->suppl_page_table,
			&pge.spt_hash_elem);
	if (elem != NULL) {
		p = hash_entry(elem, struct page, spt_hash_elem);
		struct frame *frme = frame_alloc_and_lock(p);
		if (frme != NULL) {
			p->frame = frme;
			// two scenarios handling starts
			if (p->swap_flag == 1) {
				// printf ("\n!!!ENTERED DANGER ZONE");
				// Case 2: Swap scenario
				// Load from swap
				swap_in(p);
			} else {
				// Case 1: normal dynamic paging scenario
				// Load this page.
				if (file_read_at(p->file, frme->base, p->read_bytes,
						p->file_ofs) != (int) p->read_bytes) {
					palloc_free_page(frme->base);
					return false;
				}
				memset(frme->base + p->read_bytes, 0, PGSIZE - p->read_bytes);
				// Add the page to the process's address space.
				// two scenarios handling end
			}
			if (!install_a_page(p->addr, frme->base, p->writable, "page_in")) {
				palloc_free_page(frme->base);
				frame_free(p->frame);
				return false;
			}
		} else
			return false;
	} else
		return false;
	return true;
}

/* Adds a mapping for user virtual address VADDR to the page hash
 table.  Fails if VADDR is already mapped or if memory
 allocation fails. */
struct page *
page_allocate(struct file *file, off_t ofs, uint32_t read_bytes, void *vaddr, bool read_only) {
	struct page *pge = (struct page *) malloc(sizeof(struct page));
	// check if in spt.. if yes, return NULL
	if (hash_find(&thread_current()->suppl_page_table,
			&pge->spt_hash_elem)==NULL) {
		// setting page values for use during page fault
		pge->file = file;
		pge->addr = vaddr;
		pge->file_ofs = ofs;
		pge->read_bytes = read_bytes;
		pge->writable = read_only;
		pge->frame = NULL;
		hash_insert(&thread_current()->suppl_page_table, &pge->spt_hash_elem);
	} else
		return NULL;
	return pge;
}

bool stack_grow(void *fault_addr) {
	// as we are growing the stack here, we create a new page for allocation
	// then we allocate the page to a frame and add it to the suppl page table
	struct page *pge = (struct page *) malloc(sizeof(struct page)); // allocate a new page
	pge->addr = (void *) pg_round_down(fault_addr);
	pge->writable = true;
	struct frame *frme = frame_alloc_and_lock(pge);
	if (frme != NULL) {
		pge->frame = frme;
		/* if we are unable to install the page into the pageDir (i.e map the user virtual address to kernel virtual address),
		 we free all resources - page & frame */
		if (!install_a_page(pge->addr, frme->base, pge->writable,
				"stack_grow")) {
			/*frame_free(pge->frame); // frame_free(frme);
			 free(pge); // free the page*/
			palloc_free_page(frme->base);
			frame_free(pge->frame);
			return false;
		}
	} else
		return false;
	return true;
}

/* Evicts the page containing address VADDR
 and removes it from the page table. */
void page_deallocate(void *vaddr) {
	struct page pge;
	struct page *p = &pge; // temp fix
	pge.addr = (void *) pg_round_down(vaddr);
	struct hash_elem *elem = hash_find(&thread_current()->suppl_page_table,
			&pge.spt_hash_elem);
	if (elem != NULL) {
		p = hash_entry(elem, struct page, spt_hash_elem);
		struct hash_elem *e = hash_delete(&thread_current()->suppl_page_table,
				&p->spt_hash_elem);
		if (e != NULL) {
			if (p->frame != NULL) {
				palloc_free_page(p->frame->base);
				// frame_free(p->frame);
			}
			if (pagedir_get_page(thread_current()->pagedir, p->addr) != NULL)
				pagedir_clear_page(thread_current()->pagedir, p->addr);
		} else {
			palloc_free_page(p->frame->base);
		}
	}
}

/* Returns a hash value for the page that E refers to. */
unsigned page_hash(const struct hash_elem *e, void *aux UNUSED) {
	const struct page *page = hash_entry(e, struct page, spt_hash_elem);
	return hash_bytes(&page->addr, sizeof page->addr);
}

/* Returns true if page A precedes page B. */
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_,
		void *aux UNUSED) {
	struct page *p1 = hash_entry(a_, struct page, spt_hash_elem);
	struct page *p2 = hash_entry(b_, struct page, spt_hash_elem);
	return p1->addr < p2->addr;
}
