#include "sess.h"
#include "utils.h"

apr_status_t lfd_sess_create(struct lfd_sess **plfd_sess, apr_thread_t * thd, apr_socket_t * sock)
{
	apr_pool_t	* sess_pool;
	apr_pool_t	* loop_pool;
	apr_status_t	rc;
	struct lfd_sess	*sess;
	rc = apr_pool_create(&loop_pool, NULL);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lfd_sess_create could not create looop pool. Errorcode: %d", rc);
		return rc;
	}
	sess_pool = apr_thread_pool_get(thd);
	*plfd_sess = sess = apr_pcalloc(sess_pool, sizeof(struct lfd_sess));
	sess->sess_pool = sess_pool;
	sess->loop_pool = loop_pool;
	sess->comm_sock = sock;
	return APR_SUCCESS;
}

