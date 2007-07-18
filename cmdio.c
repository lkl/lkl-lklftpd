
#include <apr_strings.h>
#include <apr_network_io.h>

#include "cmdio.h"



apr_status_t lfd_cmdio_write(struct lfd_sess * sess, int cmd, const char *msg, ...)
{
	va_list ap;
	char * buff = NULL;
	apr_size_t len;
	apr_status_t rc = APR_SUCCESS;


	va_start(ap, msg);
	buff = apr_pvsprintf(sess->loop_pool, msg, ap);
	len = strlen(buff);
	if(NULL != buff)
	{
		rc = apr_socket_send(sess->comm_sock, buff, &len);
		printf("%s\n", buff);
	}
	va_end(ap);
	return rc;
}

