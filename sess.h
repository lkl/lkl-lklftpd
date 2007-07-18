#ifndef LKLFTPD_SESS_H__
#define LKLFTPD_SESS_H__

#include <apr_network_io.h>
#include <apr_pools.h>
#include <apr_errno.h>
#include <apr_thread_proc.h>

struct lfd_sess
{
	apr_socket_t	* comm_sock;//the client command socket
	apr_pool_t	* loop_pool; //use this pool to allocate temporary data and data that is relevant to the current command only.
	apr_pool_t	* sess_pool; //use this pool to allocate objects that have meaning durring the whole lifetime of the session.
};

apr_status_t lfd_sess_create(struct lfd_sess **plfd_sess, apr_thread_t * thd, apr_socket_t * sock);

#endif//LKLFTPD_SESS_H__
