#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h" // power_off
#include "console.h" // putbuf
#include "filesys/filesys.h" // filesys_create
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "vm/vm.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame*);

/* system calls */
void halt(void);
void exit(int status);
int exec(const char* cmd_line);
int wait(tid_t tid);
tid_t fork(const char* thread_name, struct intr_frame* if_);

/* file */
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
void close(int fd);
int filesize(int fd);
int read(int fd, void* buffer, unsigned size);
int write(int fd, const void* buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);

/* memory mapped */
void* mmap(void* addr, size_t length, int writable, int fd, off_t offset);
void munmap(void* addr);

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
syscall_init(void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
		((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
		FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler(struct intr_frame* f UNUSED) {
	/* %rdi, %rsi, %rdx, %r10, %r8, %r9 */
	int syscall_number = (int)f->R.rax;
	thread_current()->stack_ptr = f->rsp;

	switch (syscall_number)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;

	default:
		exit(-1);
		break;
	}
}

#ifndef VM
void check_address(void* addr) {
	if (addr == NULL || !is_user_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL)
	{
		exit(-1);
	}
}
#else
struct page* check_address(void* addr) {
	struct thread* curr = thread_current();

	if (!is_user_vaddr(addr) || addr == NULL)
		exit(-1);

	return spt_find_page(&curr->spt, addr);
}

/* buffer의 쓰기 권한 확인 */
bool check_buffer(void* buffer, int size) {
	uint8_t* addr = (uint8_t*)buffer;
	int page_cnt = (size + PGSIZE - 1) / PGSIZE;

	for (int i = 0; i < page_cnt; i++) {
		struct page* page = spt_find_page(&thread_current()->spt, addr + i * PGSIZE);
		if (page == NULL) return true; // stack growth의 목적일 수 있기 떄문에 true로 반환
		if (!page->rw_w) return false; // write 권한이 없다면 false 반환
	}
	return true;
}

#endif


/* ---------- system calls ---------- */
void halt(void) {
	power_off();
}

/* Terminates the current user program, returning status to the kernel. */
void exit(int status) {
	struct thread* curr = thread_current();
	curr->exit_status = status; // 종료 코드를 부모 프로세스가 알 수 있도록

	// thread 이름 출력
	printf("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

int exec(const char* cmd_line) {
	check_address(cmd_line);

	char* new_cmd_line = palloc_get_page(PAL_ZERO);
	if (new_cmd_line == NULL) return -1;

	strlcpy(new_cmd_line, cmd_line, PGSIZE);

	int result = process_exec(new_cmd_line);
	if (result < 0)	return -1;

	return result; // 성공 시 프로세스 ID 반환
}

int wait(tid_t tid) {
	return process_wait(tid);
}

tid_t fork(const char* thread_name, struct intr_frame* f) {
	tid_t pid = process_fork(thread_name, f);
	if (pid == TID_ERROR || pid <= 0) {
		return TID_ERROR;
	}
	return pid;
}

/* ---------- file manipulation ---------- */
bool create(const char* file, unsigned initial_size) {
	check_address(file);

	lock_acquire(&filesys_lock);
	bool result = filesys_create(file, initial_size);
	lock_release(&filesys_lock);

	return result;
}

bool remove(const char* file) {
	check_address(file);

	lock_acquire(&filesys_lock);
	bool result = filesys_remove(file);
	lock_release(&filesys_lock);

	return result;
}

int open(const char* file) {
	check_address(file);

	lock_acquire(&filesys_lock);
	struct file* f = filesys_open(file);

	if (f == NULL) {
		lock_release(&filesys_lock);
		return -1;
	}

	struct thread* curr = thread_current();
	struct file** fdt = curr->fd_table;

	for (int fd = 2; fd < FDCOUNT_LIMIT; fd++) {
		if (fdt[fd] == NULL) {
			fdt[fd] = f;
			lock_release(&filesys_lock);
			return fd;
		}
	}

	// FDT가 가득 찼다면 파일 닫고 -1 반환
	file_close(f);
	lock_release(&filesys_lock);
	return -1;
}

void close(int fd) {
	if (fd >= 2 && fd < FDCOUNT_LIMIT) {
		struct file* f = thread_current()->fd_table[fd];
		if (f != NULL) {
			file_close(f);
			thread_current()->fd_table[fd] = NULL;
		}
	}
}

int filesize(int fd) {
	if (fd < 2 || fd >= FDCOUNT_LIMIT) {
		exit(-1);
	}
	struct file* f = thread_current()->fd_table[fd];
	if (f == NULL) {
		exit(-1);
	}
	return file_length(f);
	// return file_length(thread_current()->fd_table[fd]);
}

int read(int fd, void* buffer, unsigned size) {
	// buffer의 쓰기 권한 확인
	if (!check_buffer(buffer, size)) exit(-1);

	// 표준 입력의 경우, 키보드의 입력을 받음
	if (fd == 0) {
		char* buf = (char*)buffer;
		for (unsigned i = 0; i < size; i++) {
			buf[i] = input_getc();
		}
		return size;
	}

	// 옳은 fd인지 확인
	if (fd < 2 || fd >= FDCOUNT_LIMIT) {
		return -1;
	}

	// 접근한 file이 비어있는지 확인
	struct file* read_file = thread_current()->fd_table[fd];
	if (read_file == NULL) {
		return -1;
	}

	// file_read 수행
	lock_acquire(&filesys_lock);
	off_t bytes = file_read(read_file, buffer, size);
	lock_release(&filesys_lock);

	return bytes;
}

int write(int fd, const void* buffer, unsigned size) {
	check_address(buffer);

	// 표준 출력: 콘솔 처리
	if (fd == 1) {
		lock_acquire(&filesys_lock);
		putbuf(buffer, size);
		lock_release(&filesys_lock);
		return size;
	}

	// 옳은 fd인지 확인
	if (fd < 2 || fd >= FDCOUNT_LIMIT) {
		return -1;
	}

	// 접근한 file이 비어있는지 확인
	struct file* write_file = thread_current()->fd_table[fd];
	if (write_file == NULL) {
		return -1;
	}

	// file_write 수행
	lock_acquire(&filesys_lock);
	off_t bytes = file_write(write_file, buffer, size);
	lock_release(&filesys_lock);

	return bytes;
}

void seek(int fd, unsigned position) {
	if (fd < 2 || fd >= FDCOUNT_LIMIT) {
		exit(-1);
	}
	struct file* f = thread_current()->fd_table[fd];
	if (f == NULL) {
		exit(-1);
	}
	file_seek(f, position);
}

unsigned tell(int fd) {
	if (fd < 2 || fd >= FDCOUNT_LIMIT) {
		exit(-1);
	}
	struct file* f = thread_current()->fd_table[fd];
	if (f == NULL) {
		exit(-1);
	}
	return file_tell(f);
}

/* ---------- memory mapped ---------- */
void* mmap(void* addr, size_t length, int writable, int fd, off_t offset) {
	if (addr != pg_round_down(addr) || addr == NULL) return NULL;
	if (is_kernel_vaddr(addr) || is_kernel_vaddr(addr + length - 1)) return NULL;

	if (length == 0) return NULL;
	if (fd == 0 || fd == 1) return NULL;
	if (offset % PGSIZE != 0) return NULL;

	if (spt_find_page(&thread_current()->spt, addr) != NULL) return NULL;

	struct file* file = thread_current()->fd_table[fd];
	if (file == NULL) return NULL;

	lock_acquire(&filesys_lock);
	addr = do_mmap(addr, length, writable, file, offset);
	lock_release(&filesys_lock);
	if (addr == NULL) return NULL;

	return addr;
}

void munmap(void* addr) {
	lock_acquire(&filesys_lock);
	do_munmap(addr);
	lock_release(&filesys_lock);
}