#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int32_t off_t;
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

void hash_destructor_func (struct hash_elem *e, void *aux UNUSED);

bool install_a_page (void *upage, void *kpage, bool writable, char* string);

bool load_a_segment(struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable);

#endif /* userprog/process.h */
