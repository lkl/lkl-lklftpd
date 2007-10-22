#ifndef SYSCALL_HELPERS_H__
#define SYSCALL_HELPERS_H__

void syscall_helpers_init(void);
void syscall_helpers_fini(void);
void syscall_enter(void);
void syscall_exit(void);
void syscall_done(void *arg);

#endif
