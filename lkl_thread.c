#ifdef LKL_FILE_APIS

#include "lkl_thread.h"

apr_thread_mutex_t *kth_mutex;
apr_pool_t * lkl_thread_creator_pool;

int private_thread_info_size(void)
{
        return sizeof(struct _thread_info);
}

void private_thread_info_init(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

	apr_thread_mutex_create(&pti->sched_mutex, APR_THREAD_MUTEX_DEFAULT, lkl_thread_creator_pool);
        apr_thread_mutex_lock(pti->sched_mutex);
}

void _switch_to(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;

        apr_thread_mutex_unlock(_next->sched_mutex);
        apr_thread_mutex_lock(_prev->sched_mutex);
}

void* kernel_thread_helper(apr_thread_t * aprth, void *arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        apr_thread_mutex_unlock(kth_mutex);
        apr_thread_mutex_lock(pti->sched_mutex);
        return (void*)fn(farg);
}



void destroy_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;
	apr_status_t ret;

	apr_thread_mutex_destroy(pti->sched_mutex);
	apr_thread_exit(pti->th,ret);
}

int _copy_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };
        int ret;

	ret=apr_thread_create(&ktha.pti->th, NULL, kernel_thread_helper, &ktha, lkl_thread_creator_pool);
        apr_thread_mutex_lock(kth_mutex);
        return ret;
}

void cpu_wait_events(void)
{
}

#endif //LKL_FILE_APIS
