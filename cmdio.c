
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

void lfd_cmdio_get_cmd_and_arg(struct lfd_sess* p_sess, char** p_cmd_str, char** p_arg_str, int set_alarm)
{
	/* Prepare an alarm to timeout the session.. */

	/* Blocks */
	/*control_getline(p_cmd_str, p_sess);
	str_split_char(p_cmd_str, p_arg_str, ' ');
	str_upper(p_cmd_str);
	if (tunable_log_ftp_protocol)
	{
	static struct mystr s_log_str;
	if (str_equal_text(p_cmd_str, "PASS"))
	{
	str_alloc_text(&s_log_str, "PASS <password>");
	}
	else
	{
	str_copy(&s_log_str, p_cmd_str);
	if (!str_isempty(p_arg_str))
	{
	str_append_char(&s_log_str, ' ');
	str_append_str(&s_log_str, p_arg_str);
	}
	}
	*/
}


