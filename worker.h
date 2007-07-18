#ifndef LKLFTPD_WORKER_H__
#define LKLFTPD_WORKER_H__

#include <apr_errno.h>
#include <apr_thread_proc.h>

struct lfd_sess
{
	apr_socket_t	* comm_sock;//the client command socket
	apr_pool_t	* loop_pool; //use this pool to allocate temporary data and data that is relevant to the current command only.
	apr_pool_t	* sess_pool; //use this pool to allocate objects that have meaning durring the whole lifetime of the session.
};

void * lfd_worker_protocol_main( apr_thread_t * thd, void* param);
#endif//LKLFTPD_WORKER_H__
