#include <string.h>
#include <apr_strings.h>
#include <apr_network_io.h>

#include "cmdio.h"
#include "config.h"
#include "ftpcodes.h"

/**
	Handles client commands and other related stuff
**/

static void cmd_to_str(int cmd, char * buff)
{
	if (cmd > FTP_MORE) {
		buff[3] = '-';
		cmd-=FTP_MORE;
	} else
		buff[3] = ' ';
	buff[2] = (char) '0' + cmd % 10; cmd /= 10;
	buff[1] = (char) '0' + cmd % 10; cmd /= 10;
	buff[0] = (char) '0' + cmd % 10;
}

apr_status_t lfd_cmdio_write(struct lfd_sess * sess, int cmd, const char *msg, ...)
{
	va_list		  ap;
	char		* buff = NULL;
	apr_size_t	  len;
	apr_status_t	 rc = APR_SUCCESS;

	buff = apr_pstrcat(sess->loop_pool, "AAAA", msg, "\r\n", NULL);
	cmd_to_str(cmd, buff);

	va_start(ap, msg);
	buff = apr_pvsprintf(sess->loop_pool, buff, ap);
	len = strlen(buff);
	rc = apr_socket_send(sess->comm_sock, buff, &len);
	va_end(ap);

	if (lfd_config_debug)
		fprintf(stderr, "-> %s", buff);

	return rc;
}

apr_status_t lfd_cmdio_get_cmd_and_arg(struct lfd_sess* sess, char** p_cmd_str, char** p_arg_str)
{
	apr_status_t		  rc;
	char			* buffer;
	char			* cmd_body, * cmd_arg;
	char			* last;
	apr_size_t		  len = cmd_input_buffer_len;


	cmd_body = NULL;
	cmd_arg = NULL;

	buffer = sess->cmd_input_buffer;
	memset(buffer, '\0', cmd_input_buffer_len);
	rc = apr_socket_recv(sess->comm_sock, buffer,&len);


	if (0 == len)
	{
                return APR_EOF;
	}
	if(APR_SUCCESS != rc)
	{
		return rc;
	}

	// parse the command
	cmd_body = apr_strtok(buffer, " ", &last);
	if(NULL != cmd_body)
	{
		buffer = last;
		cmd_arg = apr_strtok(buffer, FTP_ENDCOMMAND_SEQUENCE, &last);
	}
	else
	{
		rc = APR_INCOMPLETE;
	}
	*p_cmd_str = cmd_body;
	*p_arg_str = cmd_arg;

	if (lfd_config_debug) 
		fprintf(stderr, "<- %s %s\n", cmd_body, cmd_arg);

	return rc;
}
