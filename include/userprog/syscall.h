#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);
#ifndef VM
void check_address(void* addr);
#else
struct page* check_address(void* addr);
#endif

struct lock filesys_lock;

#endif /* userprog/syscall.h */
