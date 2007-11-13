#include "thread_wrapper.h"

#include <apr_atomic.h>
static volatile apr_uint32_t number_of_threads = -1;
static apr_thread_mutex_t * all_threads_are_gone_mutex;

apr_status_t wrapper_apr_thread_init(apr_pool_t * pool)
{
	apr_status_t rc;
	rc = apr_thread_mutex_create(&all_threads_are_gone_mutex, APR_THREAD_MUTEX_UNNESTED, pool);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_thread_mutex_lock(all_threads_are_gone_mutex);
	if(APR_SUCCESS != rc)
	{
		apr_thread_mutex_destroy(all_threads_are_gone_mutex);
		return rc;
	}
	return APR_SUCCESS;
}


apr_status_t wrapper_apr_thread_create (apr_thread_t **new_thread, apr_threadattr_t *attr, apr_thread_start_t func, void *data, apr_pool_t *cont)
{
	apr_status_t rc;
	apr_atomic_inc32(&number_of_threads);
	rc = apr_thread_create (new_thread, attr, func, data, cont);
	if(APR_SUCCESS != rc)
		apr_atomic_dec32(&number_of_threads);
	return rc;
}


apr_status_t wrapper_apr_thread_exit (apr_thread_t *thd, apr_status_t retval)
{
	apr_status_t rc;
	apr_uint32_t threads_still_exist;

	threads_still_exist = apr_atomic_dec32(&number_of_threads);
	if(!threads_still_exist && shutting_down)
		apr_thread_mutex_unlock(all_threads_are_gone_mutex);
	rc = apr_thread_exit (thd, retval);
	return rc;

}

void wrapper_apr_thread_join_all(void)
{
	apr_uint32_t nr_threads;

	nr_threads = apr_atomic_read32(&number_of_threads);
	if(-1 != nr_threads)
	{
		apr_thread_mutex_lock(all_threads_are_gone_mutex);
		apr_thread_mutex_unlock(all_threads_are_gone_mutex);
		apr_thread_mutex_destroy(all_threads_are_gone_mutex);
	}
	apr_thread_mutex_lock(all_threads_are_gone_mutex);
}

