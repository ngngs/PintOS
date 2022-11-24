#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/synch.h"
#include "userprog/process.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void
sys_halt_handler(){
	power_off();
}

void
sys_exit_handler(int arg1){
	thread_current()->process_status = arg1;
	thread_exit();
}

bool sys_create_handler(char *filename, unsigned intial_size){
	struct thread *curr = thread_current();
	if (!(filename 
			&& is_user_vaddr(filename)
		  	&& pml4_get_page(curr->pml4, filename)))
	{
		curr->process_status = -1;
		thread_exit();
	}
	return  filesys_create(filename, intial_size);
}

bool sys_remove_handler(char *filename){
	return filesys_remove(filename);
} 

int sys_open_handler(char *filename){
	struct thread *curr = thread_current();
	if (!(filename
			&& is_user_vaddr(filename)
		  	&& pml4_get_page(curr->pml4, filename)))
	{
		curr->process_status = -1;
		thread_exit();
	}
	struct file **f_table = curr->fd_table;
	int i = 3;
	for (i; i < 10; i++)
	{
		if (f_table[i] == NULL)
			break;
	}
	struct file *file = filesys_open(filename);
	if (!file)
		return -1;
	file->inode->open_cnt += 1;
	f_table[i] = file;

	struct ELF ehdr;
	if (file_read(file, &ehdr, sizeof ehdr) == sizeof ehdr && memcmp(ehdr.e_ident, "\177ELF\2\1\1", 7) == 0 && ehdr.e_type == 2 && ehdr.e_machine == 0x3E && ehdr.e_version == 1 && ehdr.e_phentsize == sizeof(struct Phdr) && ehdr.e_phnum <= 1024)
	{
		file_deny_write(file);
	}
	file->pos -= (sizeof(struct ELF));

	return i;
}

int sys_close_handler(int fd){
	struct file **f_table = thread_current()->fd_table;
	if (fd < 3 || fd >= 10){
		thread_current()->process_status = -1;
		thread_exit();
	}
	else if (f_table[fd]){
		f_table[fd] == NULL;
	}
	else{
		thread_current()->process_status = -1;
		thread_exit();
	}
}

int sys_filesize_handler(int fd){
	struct thread *curr = thread_current();
	struct thread **f_table = curr->fd_table;
	struct file *f = f_table[fd];
	return file_length(f);
}

int sys_read_handler(int fd, void* buffer, unsigned size){
	struct thread *curr = thread_current();
	if (fd < 3 || fd >= 10 || curr->fd_table[fd] == NULL || buffer == NULL || is_kernel_vaddr(buffer) || !pml4_get_page(curr->pml4, buffer)) 
	{
		thread_current()->process_status = -1;
		thread_exit();
	}
	struct file *f = curr->fd_table[fd];
	return file_read(f, buffer, size);
}

int sys_write_handler(int fd, void *buffer, unsigned size){
	struct thread *curr = thread_current();

	if (fd == 1){
		putbuf(buffer, size);
		return size;
	}
	if (fd < 3 || fd >= 10 || curr->fd_table[fd] == NULL || buffer == NULL || is_kernel_vaddr(buffer) || !pml4_get_page(curr->pml4, buffer)) 
	{
		curr->process_status = -1;
		thread_exit();
	}
	struct file *f = curr->fd_table[fd];
	return file_write(f, buffer, size);
}

int sys_fork_handler(char *thread_name, struct intr_frame *f){
	return process_fork(thread_name, f);
}

int sys_wait_handler(int pid){
	return process_wait(pid);
}

int sys_exec_handler(char * filename){
	struct thread *curr = thread_current();
	if (!(filename
			&& is_user_vaddr(filename)
		  	&& pml4_get_page(curr->pml4, filename)))
	{
		curr->process_status = -1;
		thread_exit();
	}
	return process_exec(filename);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) { 
	// TODO: Your implementation goes here.
	int syscall_n = f->R.rax;
	switch (syscall_n)
	{
	case SYS_HALT:
		sys_halt_handler();
		break;
	case SYS_EXIT:
		sys_exit_handler(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = sys_create_handler(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = sys_remove_handler(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = sys_open_handler(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = sys_filesize_handler(f->R.rdi);
		break;
	case SYS_CLOSE:
		f->R.rax = sys_close_handler(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = sys_read_handler(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = sys_write_handler(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_FORK:
		f->R.rax = sys_fork_handler(f->R.rdi, f);
		break;
	case SYS_WAIT:
		f->R.rax = sys_wait_handler(f->R.rdi);
		break;
	case SYS_EXEC:
		f->R.rax = sys_exec_handler(f->R.rdi);
		break;
	default:
		break;
	}
}
