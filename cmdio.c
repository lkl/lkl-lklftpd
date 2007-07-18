
#include <apr_strings.h>
#include <apr_network_io.h>

#include "cmdio.h"

static void cmd_to_str(int cmd, char * buff)
{
	buff[3] = ' ';
	buff[2] = (char) '0' + cmd % 10; cmd /= 10;
	buff[1] = (char) '0' + cmd % 10; cmd /= 10;
	buff[0] = (char) '0' + cmd % 10;
}

static apr_status_t lfd_cmdio_write_cmd(struct lfd_sess * sess, int cmd)
{
	char buff[4];
	apr_size_t len = 4;
	cmd_to_str(cmd, buff);
	return apr_socket_send(sess->comm_sock, buff, &len);
}

apr_status_t lfd_cmdio_write(struct lfd_sess * sess, int cmd, const char *msg, ...)
{
	va_list ap;
	char * buff = NULL;
	apr_size_t len;
	apr_status_t rc = APR_SUCCESS;

	rc = lfd_cmdio_write_cmd(sess, cmd);
	if(APR_SUCCESS != rc)
	{
		return rc;
	}
	va_start(ap, msg);
	buff = apr_pvsprintf(sess->loop_pool, msg, ap);
	len = strlen(buff);
	if(NULL != buff)
	{
		rc = apr_socket_send(sess->comm_sock, buff, &len);
/*		if(APR_SUCCESS != rc)
		{
			len = 1;
			buff = "\r";
			rc = apr_socket_send(sess->comm_sock, buff, &len);
		}*/
	}
	va_end(ap);
	return rc;
}

