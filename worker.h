#ifndef LKLFTPD_WORKER_H__
#define LKLFTPD_WORKER_H__

#include <apr_errno.h>
#include <apr_thread_proc.h>

struct lfd_sess
{
	apr_socket_t	* sock;
	apr_pool_t	* loop_pool;
	apr_pool_t	* sess_pool;
};

void * lfd_worker_protocol_main( apr_thread_t * thd, void* param);
#endif//LKLFTPD_WORKER_H__
