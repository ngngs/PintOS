#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* Project3 */
struct send_data_via{
    struct file *file;
    off_t position;
    uint32_t page_read_bytes;
    uint32_t page_zero_bytes;
};

/* ELF types.  See [ELF1] 1-2. */

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* Project3 */
bool lazy_load_segment(struct page *page, void *aux);
bool setup_stack(struct intr_frame *if_);

#endif /* userprog/process.h */
