#include <apr_pools.h>
#include <apr_thread_mutex.h>

static apr_thread_mutex_t *syscall_mutex;
static apr_pool_t *syscall_pool;
static apr_thread_mutex_t *wait_syscall_mutex;

void syscall_helpers_init(void)
{
	apr_pool_create(&syscall_pool, NULL);
	apr_thread_mutex_create(&syscall_mutex, APR_THREAD_MUTEX_UNNESTED, syscall_pool);
	apr_thread_mutex_create(&wait_syscall_mutex, APR_THREAD_MUTEX_UNNESTED, syscall_pool);
	apr_thread_mutex_lock(wait_syscall_mutex);
}

void syscall_helpers_fini(void)
{
	apr_thread_mutex_destroy(syscall_mutex);
	apr_pool_destroy(syscall_pool);
}


void syscall_done(void *arg)
{
	apr_thread_mutex_unlock(wait_syscall_mutex);
}

void syscall_enter(void)
{
	apr_thread_mutex_lock(syscall_mutex);
}

void syscall_exit(void)
{
	apr_thread_mutex_lock(wait_syscall_mutex);
	apr_thread_mutex_unlock(syscall_mutex);
}

