#include "sess.h"
#include "utils.h"

apr_status_t lfd_sess_create(struct lfd_sess **plfd_sess, apr_thread_t * thd, apr_socket_t * sock)
{
	apr_pool_t	* sess_pool;
	apr_pool_t	* loop_pool;
	apr_status_t	rc;
	struct lfd_sess	*sess;

	sess_pool = apr_thread_pool_get(thd);

	rc = apr_pool_create(&loop_pool, sess_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lfd_sess_create could not create looop pool. Errorcode: %d", rc);
		return rc;
	}

	*plfd_sess = sess = apr_pcalloc(sess_pool, sizeof(struct lfd_sess));
	sess->sess_pool = sess_pool;
	sess->loop_pool = loop_pool;
	sess->comm_sock = sock;
	return APR_SUCCESS;
}

void lfd_sess_destroy(struct lfd_sess *sess)
{
	//sess->sess_pool is the pool of the thread. This is freed by APR when the thread exits.

	apr_pool_destroy(sess->loop_pool);
	apr_socket_close(sess->comm_sock);
}

