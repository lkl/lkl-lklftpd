#ifndef LKL_THREADS_H__
#define LKL_THREADS_H__

#include <apr_general.h>
#include <apr_thread_proc.h>
#include <apr_thread_mutex.h>
#include <apr_errno.h>


struct _thread_info {
        apr_thread_t *th;
        apr_thread_mutex_t *sched_mutex;
};

struct kernel_thread_helper_arg {
        int (*fn)(void*);
        void *arg;
        struct _thread_info *pti;
};

extern apr_thread_mutex_t *kth_mutex;
extern apr_pool_t * root_pool;

int private_thread_info_size(void);
void private_thread_info_init(void *arg);
void _switch_to(void *prev, void *next);
void* kernel_thread_helper(apr_thread_t * aprth, void *arg);
void destroy_thread(void *arg);
int _copy_thread(int (*fn)(void*), void *arg, void *pti);
void cpu_wait_events(void);

extern void* current_private_thread_info(void);

#endif //LKL_THREADS_H__
