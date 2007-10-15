#include <apr_pools.h>
#include <apr_thread_mutex.h>

apr_thread_mutex_t	* barrier_mutex;
apr_pool_t 		* barrier_pool;



void barrier_init(void)
{
	apr_pool_create(&barrier_pool, NULL);
	apr_thread_mutex_create(&barrier_mutex, APR_THREAD_MUTEX_UNNESTED, barrier_pool);
}

void barrier_fini(void)
{
	apr_thread_mutex_destroy(barrier_mutex);
	apr_pool_destroy(barrier_pool);
}

void barrier_enter(void)
{
	apr_thread_mutex_lock(barrier_mutex);
}

void barrier_exit(void)
{
	apr_thread_mutex_unlock(barrier_mutex);
}

