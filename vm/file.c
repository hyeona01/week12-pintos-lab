/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "lib/round.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static bool file_backed_swap_in(struct page* page, void* kva);
static bool file_backed_swap_out(struct page* page);
static void file_backed_destroy(struct page* page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init(void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer(struct page* page, enum vm_type type, void* aux) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page* file_page = &page->file;

	struct vm_aux* vm_aux = (struct vm_aux*)aux;
	file_page->file = vm_aux->file;
	file_page->offset = vm_aux->ofs;
	file_page->page_cnt = vm_aux->page_cnt;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page* page, void* kva) {
	struct file_page* file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page* page) {
	struct file_page* file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page* page) {
	struct file_page* file_page UNUSED = &page->file;

	if (pml4_is_dirty(&thread_current()->pml4, page->va)) {
		file_write_at(page->file.file, page->frame->kva, page->file.length, page->file.offset);
		pml4_set_dirty(&thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(&thread_current()->pml4, page->va);
}

/* Do the mmap */
void*
do_mmap(void* addr, size_t length, int writable,
	struct file* file, off_t offset) {

	struct file* f = file_reopen(file);
	void* original_addr = addr;
	size_t read_bytes = file_length(f) < length ? file_length(f) : length; // 실제 file size 확인
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	/* munmap을 고려한 총 page 개수 */
	int page_cnt;
	if (read_bytes < PGSIZE) {
		page_cnt = 1;
	}
	else if (read_bytes % PGSIZE != 0) {
		page_cnt = read_bytes / PGSIZE + 1;
	}
	else {
		page_cnt = read_bytes / PGSIZE;
	}

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* lazy_load_segment 인자(aux) 전달 */
		struct vm_aux* vm_aux = malloc(sizeof(struct vm_aux));
		if (vm_aux == NULL) {
			return NULL;
		}
		vm_aux->file = f;
		vm_aux->ofs = offset;
		vm_aux->read_bytes = page_read_bytes;
		vm_aux->zero_bytes = page_zero_bytes;
		vm_aux->page_cnt = page_cnt;

		/* page 등록 */
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, vm_aux)) {
			return NULL;
		}

		// printf("*** addr: %p / read_bytes: %d / offset: %d\n", addr, read_bytes, offset);
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}
	return original_addr;
}

/* Do the munmap */
void
do_munmap(void* addr) {
	/* 'addr'은 mmap()에서 반환된 첫 번째 페이지의 시작 주소 */
	struct page* p = spt_find_page(&thread_current()->spt, addr);
	if (p == NULL) return;

	for (int i = 0; i < p->file.page_cnt; i++) {
		destroy(p);

		addr += PGSIZE; // 연속된 다음 페이지로 이동
		p = spt_find_page(&thread_current()->spt, addr);
		if (p == NULL) break;
	}
}
