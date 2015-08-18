#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

int count;

/* The swap device. */
static struct block *swap_device;

/* Used swap pages. */
static struct bitmap *swap_bitmap;

/* Protects swap_bitmap. */
static struct lock swap_lock;

/* Number of sectors per page. */
#define PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

/*
 * Block size of swap device = 4 MB = 8192 (the swap disk space we allocate)
 * Block sector size: 512 (pre defined, hardcoded)
 *
 * PAGE SIZE = 2 MB = 4096
 * PAGE SECTORS COUNT = PAGE SIZE / BLOCK SECTOR SIZE = 8
 * BIT COUNT = Block size of swap device / PAGE SECTORS count = 1024
 *
 * BLOCK SIZE: 8192 | BLOCK SECTOR SIZE: 512 | PAGE SIZE: 4096 |  BIT COUNT: 1024
 */
/* Sets up swap. */
void swap_init(void) {
	count = 0;
	// we use BLOCK_SWAP device for swap. So, device role = SWAP device.
	// Device size is the alloted disk size for the swap disk space. ex: 4 MB = 8192 sectors
	swap_device = block_get_role(BLOCK_SWAP);
	// check if swap device is present, else panic
	if (swap_device != NULL) {
		// the total number of bits we have in our array, this is the total number of pages we can allocate in the swap space
		size_t bit_count = block_size(swap_device) / PAGE_SECTORS;
		swap_bitmap = bitmap_create(bit_count);
		if (swap_bitmap != NULL)
			// setting the default values into the bit map
			bitmap_set_all(swap_bitmap, 0);
		else
			PANIC("Unable to create swap bitmap");
	} else
		PANIC("No swap device is present");
	// initialize the swap lock
	lock_init(&swap_lock);
}

/* Swaps in page P, which must have a locked frame
 (and be swapped out). */
void swap_in(struct page *p) {
	// printf("\n SWAPPING IN!!");
	if (p != NULL && p->frame != NULL) {
		// Only if the swap sector is occupied we must proceed with swap in
		if (bitmap_test(swap_bitmap, p->swap_sector) == 1) {
			// proceed
			size_t l = 0;
			lock_acquire(&swap_lock);
			while (l < PAGE_SECTORS) {
				block_read(swap_device, p->swap_sector * PAGE_SECTORS + l,
						p->frame->base + l * BLOCK_SECTOR_SIZE);
				bitmap_set(swap_bitmap, p->swap_sector, 0);
				l++;
			}
			lock_release(&swap_lock);
		} else
			PANIC("No need to swap, sector is available");
	}
}

/* Swaps out page P, which must have a locked frame. */
bool swap_out(struct page *p) {
	// printf("\n SWAPPING OUT!!, %d", count++);
	lock_acquire(&swap_lock);
	// find the FIRST ELEMENT in the bitmap with 0 value and then toggle it to 1.. the returned value is the element's position in the bitmap
	block_sector_t available_swap_sector = bitmap_scan(swap_bitmap, 0, 1, 0);
	if (available_swap_sector != BITMAP_ERROR) {
		bitmap_set_multiple(swap_bitmap, available_swap_sector, 1, 1);
		// proceed
		p->swap_sector = available_swap_sector;
		size_t l = 0;
		while (l < PAGE_SECTORS) {
			block_write(swap_device, p->swap_sector * PAGE_SECTORS + l,
					p->frame->base + l * BLOCK_SECTOR_SIZE);
			l++;
		}
	} else {
		PANIC("Swap space is full");
	}
	lock_release(&swap_lock);
	return true;
}
