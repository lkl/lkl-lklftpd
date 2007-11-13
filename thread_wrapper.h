#ifndef LKLFTPD_THREAD_WRAPPER_H__
#define LKLFTPD_THREAD_WRAPPER_H__


#include <apr_thread_proc.h>
extern volatile int shutting_down;
apr_status_t wrapper_apr_thread_init(apr_pool_t * pool);
apr_status_t wrapper_apr_thread_create (apr_thread_t **new_thread, apr_threadattr_t *attr, apr_thread_start_t func, void *data, apr_pool_t *cont);
void wrapper_apr_thread_join_all(void);
apr_status_t wrapper_apr_thread_exit (apr_thread_t *thd, apr_status_t retval);

#endif//LKLFTPD_THREAD_WRAPPER_H__
