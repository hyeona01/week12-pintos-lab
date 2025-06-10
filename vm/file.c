/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#define PGSIZE (1<<12)

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;

	struct vm_aux* lazy_load_arg = (struct vm_aux*)page->uninit.aux;
	
	// file_page->file = lazy_load_arg->file;
    // file_page->ofs = lazy_load_arg->ofs;
    // file_page->read_bytes = lazy_load_arg->read_bytes;
    // file_page->zero_bytes = lazy_load_arg->zero_bytes;
	
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	// if (pml4_is_dirty(thread_current()->pml4, page->va))
    // {
    //     file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
    // }
    // pml4_clear_page(thread_current()->pml4, page->va);
}

static bool
lazy_load_segment_for_mmap(struct page* page, void* aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct vm_aux* load_arg = (struct vm_aux*) aux;
	bool succ = true;
	void *kva = page->frame->kva;
	ASSERT(kva!=NULL);

	if(load_arg->read_bytes > 0){
		int bytes_read = file_read_at(
            load_arg->file,
            kva,
            load_arg->read_bytes,
            load_arg->ofs
        );
        if (bytes_read != (int) load_arg->read_bytes) {
            succ = false;
            goto done;
        }
	}

    if (load_arg->zero_bytes > 0) {
        memset(kva + load_arg->read_bytes, 0, load_arg->zero_bytes);
    }
done:
    return succ;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	
	struct file* f = file_reopen(file);
	void* ret_addr = addr;

	int page_count;

	size_t read_bytes, zero_bytes;

	if(length <= PGSIZE){
		page_count = 1;
	}
	else{
		page_count = length / PGSIZE;
		if(length % PGSIZE){
			page_count++;
		}
	}


	read_bytes = file_length(f) > length ? length : file_length(f);

	for (int i = 0; i < page_count; i++) {
    	size_t temp_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    	size_t temp_zero_bytes = PGSIZE - temp_read_bytes;

		struct vm_aux* lazy_load_arg = (struct vm_aux*)malloc(sizeof(struct vm_aux));
		
		lazy_load_arg->file = f;
		lazy_load_arg->ofs = offset;
		lazy_load_arg->read_bytes = temp_read_bytes;
		lazy_load_arg->zero_bytes = temp_zero_bytes;

		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment_for_mmap, lazy_load_arg)){
			free(lazy_load_arg);
			return NULL;
		}    
		spt_find_page(&thread_current()->spt, ret_addr)->page_count = page_count;

		read_bytes -= temp_read_bytes;
    	addr += PGSIZE;
    	offset += temp_read_bytes;
		free(lazy_load_arg);
	}

	return ret_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
    struct page *p = spt_find_page(spt, addr);
    int count = p->page_count;
    for (int i = 0; i < count; i++)
    {
        if (p) destroy(p);
        addr += PGSIZE;
        p = spt_find_page(spt, addr);
    }
}
