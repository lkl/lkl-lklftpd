#include <apr_strings.h>

#include "sess.h"
#include "utils.h"

const apr_size_t cmd_input_buffer_len = 100;

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
	sess->cmd_input_buffer = apr_pcalloc(sess_pool, cmd_input_buffer_len);
	sess->comm_sock = sock;

	sess->data_conn = apr_pcalloc(sess_pool, sizeof(struct lfd_data_sess));
	sess->cwd_path = apr_pstrdup(sess->sess_pool, "/");

	return APR_SUCCESS;
}

void lfd_sess_destroy(struct lfd_sess *sess)
{
	//sess->sess_pool is the pool of the thread. This is freed by APR when the thread exits.

	apr_pool_destroy(sess->loop_pool);
	apr_socket_close(sess->comm_sock);
}

apr_status_t lfd_data_sess_create(struct lfd_data_sess ** plfd_data_sess, apr_thread_t * thd, apr_socket_t *sock)
{
	apr_pool_t * data_pool;
	struct lfd_data_sess *data_sess;

	data_sess = *plfd_data_sess;
	// create a data connection
	data_pool = apr_thread_pool_get(thd);

	data_sess->data_conn_th = thd;
	data_sess->data_pool = data_pool;
	data_sess->data_sock = sock;
	data_sess->in_progress = 0;
	return APR_SUCCESS;
}

void lfd_data_sess_destroy(struct lfd_data_sess * data_sess)
{
	// data_pool will be freed when the thread exits
	data_sess->in_progress = 0;

	apr_socket_close(data_sess->data_sock);
}
