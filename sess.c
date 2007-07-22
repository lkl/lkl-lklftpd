#include "sess.h"
#include "utils.h"

apr_status_t lfd_sess_create(struct lfd_sess **plfd_sess, apr_thread_t * thd, apr_socket_t * sock)
{
	apr_pool_t	* sess_pool;
	apr_pool_t	* tmp_pool;
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

	rc = apr_pool_create(&tmp_pool, sess_pool);

	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lfd_sess_create could not create temporary pool. Errorcode: %d", rc);
		return rc;
	}

	*plfd_sess = sess = apr_pcalloc(sess_pool, sizeof(struct lfd_sess));

	sess->sess_pool = sess_pool;
	sess->loop_pool = loop_pool;
	sess->temp_pool = tmp_pool;
	sess->comm_sock = sock;
	sess->dbg_strerror_buffer = apr_pcalloc(sess_pool, STR_ERROR_MAX_LEN);

	// read user/pass from config file, or let them by default
	sess->user="gringo";
	sess->passwd="pass";
	return APR_SUCCESS;
}

void lfd_sess_destroy(struct lfd_sess *sess)
{
	//sess->sess_pool is the pool of the thread. This is freed by APR when the thread exits.

	apr_pool_destroy(sess->loop_pool);
	apr_socket_close(sess->comm_sock);
}

char * lfd_sess_strerror(struct lfd_sess * sess, apr_status_t rc)
{
	return apr_strerror(rc, sess->dbg_strerror_buffer, STR_ERROR_MAX_LEN);
}

