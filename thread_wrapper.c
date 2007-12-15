#include "thread_wrapper.h"

#include <apr_atomic.h>
#include <stdlib.h>
struct apr_thread_wrapper_t
{
    struct apr_thread_wrapper_t*prev,*next;
    apr_thread_t * thd;
};

struct apr_thread_wrapper_t thread_list;
apr_thread_mutex_t * thread_list_mutex;
volatile int thread_list_counter = 0;
volatile int system_preparing_to_go_down = 0;

static void thread_list_add(struct apr_thread_wrapper_t * list)
{
    apr_thread_mutex_lock(thread_list_mutex);
    {
        list->next = thread_list.next;
        thread_list.next->prev = list;
        list->prev = &thread_list;
        thread_list.next = list;
        thread_list_counter++;
    }
    apr_thread_mutex_unlock(thread_list_mutex);
}

static void thread_list_rem(struct apr_thread_wrapper_t * list)
{
    if(thread_list_counter && system_preparing_to_go_down==0)
    {
        apr_thread_mutex_lock(thread_list_mutex);
        {
            thread_list_counter--;
            list->next->prev = list->prev;
            list->prev->next = list->next;
            free(list);
        }
        apr_thread_mutex_unlock(thread_list_mutex);
    }
}

apr_status_t wrapper_apr_thread_init(apr_pool_t * pool)
{
	apr_status_t rc;
	rc = apr_thread_mutex_create(&thread_list_mutex, APR_THREAD_MUTEX_UNNESTED, pool);
	if(APR_SUCCESS != rc)
		return rc;
        thread_list.prev = thread_list.next = &thread_list;
	return APR_SUCCESS;
}


apr_status_t wrapper_apr_thread_create (apr_thread_wrapper_t **new_thread, apr_threadattr_t *attr, 
                                        apr_thread_start_t func, void *data, apr_pool_t *cont)
{
	apr_status_t rc;	
        apr_thread_wrapper_t * l = (apr_thread_wrapper_t *)malloc(sizeof(apr_thread_wrapper_t));
        *new_thread = l;
        thread_list_add(l);
	rc = apr_thread_create (&l->thd, attr, func, data, cont);
        if(APR_SUCCESS != rc)
            thread_list_rem(l);
	return rc;
}


apr_status_t wrapper_apr_thread_exit (apr_thread_wrapper_t *thw, apr_status_t retval)
{
        apr_thread_t * thd = thw->thd;
        //thread_list_rem(thw);
        return apr_thread_exit(thd, retval);
}

void wrapper_apr_thread_join_all()
{
    struct apr_thread_wrapper_t * list;
    struct apr_thread_wrapper_t * old_list;
    apr_status_t rc;
    system_preparing_to_go_down = 1;
    list = thread_list.next;
    while(list != &thread_list)
    {
        if(NULL != list->thd)
        {
            apr_thread_join(&rc, list->thd);
        }
        old_list = list;
        list = list->next;
        free(old_list);
    }
}
