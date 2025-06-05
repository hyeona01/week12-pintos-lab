/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"

static struct list frame_list;
static struct lock frame_lock;
static struct list_elem *clock_hand = NULL;

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
	list_init(&frame_list);
	lock_init(&frame_lock);
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

/*custom helpers*/
static bool comp_less_addr(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	const struct page *p1 = hash_entry(a, struct page, elem);
	const struct page *p2 = hash_entry(b, struct page, elem);

	return p1->va < p2->va;
}


unsigned hashing_page(const struct hash_elem *e, void *aux UNUSED){
	const struct page *p = hash_entry(e, struct page, elem);
	return hash_bytes(&(p->va), sizeof(p->va));
}

bool insert_page(struct hash *ha, struct page *pa){
	struct hash_elem* check = hash_insert(ha, &(pa->elem));
	if(check == NULL){
		return true;
	}
	return false;
}

bool delete_page(struct hash *ha, struct page *pa){
	struct hash_elem* check = hash_delete(ha, &(pa->elem));
	if(check == NULL){
		return true;
	}
	return false;
}

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	// to do
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		// need to modify(control with switch case)
		struct page* new_page = (struct page*)malloc(sizeof(struct page));

		if(type == VM_ANON) uninit_new(new_page, upage, init, type, aux, anon_initializer);
		else uninit_new(new_page, upage, init, type, aux, file_backed_initializer);

		new_page->rw_w = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, new_page);
	}
err:
	return false;
}





/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	page = (struct page*)malloc(sizeof(struct page));
	page->va = pg_round_down(va);

	struct hash_elem* find_e = hash_find(&spt->spt_hash, &(page->elem));
	free(page);

	if(find_e == NULL) return NULL;

	return hash_entry(find_e, struct page, elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	succ = insert_page(&(spt->spt_hash), page);
	return succ;
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
	if (list_empty(&frame_list))
        return NULL;

    /* clock_hand가 아직 초기화되지 않았으면, 리스트의 첫 번째 요소로 설정 */
    if (clock_hand == NULL || clock_hand == list_end(&frame_list))
        clock_hand = list_begin(&frame_list);

    /* 무한 루프 돌며 accessed 비트를 체크 */
    while (true) {
        struct frame *f = list_entry(clock_hand, struct frame, elem);

        /* 현재 쓰레드(page_table 기준)에서 accessed 비트 확인 */
        if (pml4_is_accessed(thread_current()->pml4, f->page->va)) {
            /* accessed 비트가 1이면, 우선권(Second Chance)을 주고 비트를 지운 후 다음으로 */
            pml4_set_accessed(thread_current()->pml4, f->page->va, 0);

            /* 다음으로 이동. 리스트 끝에 도달하면 다시 처음으로 순환 */
            clock_hand = list_next(clock_hand);
            if (clock_hand == list_end(&frame_list))
                clock_hand = list_begin(&frame_list);
        }
        else {
            /* accessed 비트가 0이면 이 프레임을 희생자로 선택 */
            victim = f;

            /* 다음 호출에서도 순환을 이어가기 위해 핸드를 다음 요소로 이동 */
            clock_hand = list_next(clock_hand);
            if (clock_hand == list_end(&frame_list))
                clock_hand = list_begin(&frame_list);

            return victim;
        }
    }
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	swap_out(victim->page);
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	frame = (struct frame*)malloc(sizeof(struct frame));
	if(frame == NULL) PANIC("vm_get_frame: allocate failed");

	frame->page = NULL;

	frame->kva = palloc_get_page(PAL_USER);

	if(frame->kva == NULL){
		frame = vm_evict_frame();
		frame->page = NULL;
		return frame;
	}

	lock_acquire(&frame_lock);
	list_push_back(&frame_list, &(frame->elem));
	lock_release(&frame_lock);

	frame->page = NULL;

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	if(addr == NULL || is_kernel_vaddr(addr)) return false;

	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, addr);
	
	if(not_present){
		if(page != NULL){
			if(write && !page->rw_w) return 0;
			return vm_do_claim_page(page);
		}
		return 0;
	}
	return 0;
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
	if(page == NULL) return 0;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	
	if (!pml4_set_page (thread_current ()->pml4,
        page->va,
        frame->kva,
        page->rw_w)) {
        
        frame->page = NULL;
        page->frame = NULL;
        palloc_free_page (frame->kva);
        free (frame);
        return false;
    }

    if (!swap_in (page, frame->kva)) {
        pml4_clear_page (thread_current ()->pml4, page->va);
        frame->page = NULL;
        page->frame = NULL;
        palloc_free_page (frame->kva);
        free (frame);
        return false;
    }

    return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, hashing_page, comp_less_addr, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;
	hash_first(&i, &src->spt_hash);
	while (hash_next(&i))
	{
		struct page *src_page = hash_entry(hash_cur(&i), struct page, elem);
		enum vm_type src_type = src_page->operations->type;

		if (src_type == VM_UNINIT)
		{
			vm_alloc_page_with_initializer(
				src_page->uninit.type,
				src_page->va,
				src_page->rw_w,
				src_page->uninit.init,
				src_page->uninit.aux);
		}
		else
		{
			if (vm_alloc_page(src_type, src_page->va, src_page->rw_w) && vm_claim_page(src_page->va))
			{
				struct page *dst_page = spt_find_page(dst, src_page->va);
				memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
			}
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void hash_page_destroy(struct hash_elem* e, void* aux){
	struct page *page = hash_entry(e, struct page, elem);
    destroy(page);
    free(page);
}

void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->spt_hash, hash_page_destroy);
}
