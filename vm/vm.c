/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/uninit.h"
#include "kernel/hash.h"
#include "include/threads/mmu.h"
#include "userprog/process.h"
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page *)malloc(sizeof(struct page));
		switch(VM_TYPE(type)){
			case VM_ANON :
				uninit_new (page, upage, init, type, aux, anon_initializer);
				break;
			case VM_FILE :
				uninit_new (page, upage, init, type, aux, file_backed_initializer);
				break;
		}
		/* TODO: Insert the page into the spt. */
		page->writable = writable;
		spt_insert_page(spt, page);
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	struct page *page = (struct page*)malloc(sizeof(struct page));
	struct hash_elem *hash_find_elem;
	page->va = pg_round_down(va);
	hash_find_elem = hash_find(&spt->hash, &page->hash_elem);
	free(page);
	if (hash_find_elem == NULL){
		return NULL;
	} else {
		return hash_entry(hash_find_elem, struct page, hash_elem);
	}
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	bool succ = false;
	/* TODO: Fill this function. */
	if (!hash_insert(&spt->hash, &page->hash_elem)){
		succ = true;
		return succ;
	}
	else {
		return succ;
	}
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	frame->page = NULL;
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	/* user pool memory가 꽉 찼을 때, frame을 하나 swap으로 보내주고 disk에서 그만큼 메모리를 가져와
	frame에 가져온다. 우선 PANIC으로 구현*/
	if (frame == NULL || frame->kva == NULL){
		PANIC("todo");
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1)){
		vm_claim_page(addr);
		thread_current()->stack_bottom -= PGSIZE;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	struct hash_elem hash_elem_fault;

	/* check  User memory*/
	if (is_kernel_vaddr(addr)){
		return false;
	}
	void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->stack_rsp : f->rsp;

	if (not_present){
		
		if (spt_find_page(spt, addr) == NULL){
			if(rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK){
				vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
				return true;
			}
			return false;
		}
		else{
			page = spt_find_page(spt, addr);
			return vm_do_claim_page(page);
		}
	}
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL){
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame();
	struct thread *curr = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(pml4_set_page(curr->pml4,page->va,frame->kva,page->writable)){
		return swap_in (page, frame->kva);
	}
	else{
		PANIC("Fail pml4 set page !!!!!!");
		return false;
	}
	
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash,page_hash,page_less,NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash *src_hash = &src->hash;
	struct hash *dst_hash = &dst->hash;  
	struct hash_iterator i;
	hash_first(&i,src_hash);
	while(hash_next(&i)){
		struct page *src_page = hash_entry(hash_cur(&i),struct page, hash_elem);
		enum vm_type src_type = page_get_type(src_page);		// src_page의 type을 불러온다. 즉, 부모의 페이지 타입을 불러온다
		void *src_page_va = src_page->va;						// src_page의 가상주소. 즉, 부모 페이지의 가상주소
		bool src_writable = src_page->writable;
		vm_initializer *src_init = src_page->uninit.init;		// src_page의 uninit의 init 은 lazy_load_segment 이다.
		
		void *aux = src_page->uninit.aux;
			
		if(src_page->uninit.type & VM_MARKER_0){
			setup_stack(&thread_current()->tf);
		}

		else if(src_page->operations->type == VM_UNINIT){
			if(!vm_alloc_page_with_initializer(src_type,src_page_va,src_writable,src_init,aux)){
				return false;
			}			
		}
		else{
			if(!vm_alloc_page(src_type,src_page_va,src_writable)){
				return false;
			}
			if(!vm_claim_page(src_page_va)){
				return false;
			}
		}


		if(src_page->operations->type != VM_UNINIT){
			struct page *dst_page = spt_find_page(dst,src_page_va);
			memcpy(dst_page->frame->kva,src_page->frame->kva,PGSIZE);
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&spt->hash, spt_free_destroy);
}
