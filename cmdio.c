
#include <apr_strings.h>
#include <apr_network_io.h>

#include "cmdio.h"

/**
	Handles client commands and other related stuff
**/

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
		if(APR_SUCCESS == rc)
		{
			len = 1;
			buff = "\n";
			rc = apr_socket_send(sess->comm_sock, buff, &len);
		}
	}
	va_end(ap);
	return rc;
}

apr_status_t lfd_cmdio_get_cmd_and_arg(struct lfd_sess* p_sess, char** p_cmd_str, char** p_arg_str, int set_alarm)
{
	apr_status_t ret;
	char *buffer;
	char *cmd_body, *cmd_arg;
	char * last;
	const char *sep =" \r\n";
	apr_size_t len = cmd_input_buffer_len;
	cmd_body = NULL;
	cmd_arg = NULL;

	buffer = p_sess->cmd_input_buffer;

	ret = apr_socket_recv(p_sess->comm_sock, buffer,&len);

	if (0 == len)
	{
                return APR_EOF;
	}
	if(APR_SUCCESS != ret)
	{
		return ret;
	}

	// parse the command
	cmd_body = apr_strtok(buffer, sep, &last);
	if(NULL != cmd_body)
	{
		buffer = last;
		cmd_arg = apr_strtok(buffer, sep, &last);
	}
	else
	{
		ret=APR_INCOMPLETE;
	}
	*p_cmd_str = cmd_body;
	*p_arg_str = cmd_arg;

	return ret;
}
