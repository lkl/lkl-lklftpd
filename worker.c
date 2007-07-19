// main FTP protocol loop handling
#include <apr_network_io.h>
#include <apr_errno.h>
#include <apr_strings.h>

#include "worker.h"
#include "config.h"
#include "utils.h"
#include "ftpcodes.h"
#include "cmdio.h"
#include "sess.h"


static apr_status_t emit_greeting(struct lfd_sess * p_sess)
{
	apr_status_t rc = APR_SUCCESS;
	if(0 != strlen(lfd_config_banner_string))
	{
		rc = lfd_cmdio_write(p_sess, FTP_GREET, lfd_config_banner_string);
	}
	return rc;
}


/*static apr_status_t parse_username_password(struct lfd_sess* sess)
{
	int pass_ok, user_ok;
	pass_ok = 0;
	user_ok=0;
	while (1)
	{
	vsf_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str,
				&p_sess->ftp_arg_str, 1);
	if (str_equal_text(&p_sess->ftp_cmd_str, "USER"))
	{
	handle_user_command(p_sess);
	}
	else if (str_equal_text(&p_sess->ftp_cmd_str, "PASS"))
	{
	handle_pass_command(p_sess);
	}
	else if (str_equal_text(&p_sess->ftp_cmd_str, "QUIT"))
	{
	vsf_cmdio_write(p_sess, FTP_GOODBYE, "Goodbye.");
	vsf_sysutil_exit(0);
	}
	else if (str_equal_text(&p_sess->ftp_cmd_str, "FEAT"))
	{
	handle_feat(p_sess);
	}
	else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "AUTH"))
	{
	handle_auth(p_sess);
	}
	else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "PBSZ"))
	{
	handle_pbsz(p_sess);
	}
	else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "PROT"))
	{
	handle_prot(p_sess);
	}
	else
	{
	vsf_cmdio_write(p_sess, FTP_LOGINERR,
			"Please login with USER and PASS.");
	}
	}
}*/

static int handle_user_cmd(struct lfd_sess* p_sess)
{
if(0 == apr_strnatcasecmp(p_sess->user, p_sess->ftp_arg_str)) {
	lfd_cmdio_write(p_sess, FTP_GIVEPWORD, "Password required for user.");
	return 1;
}
else
	return 0;
}

static int handle_pass_cmd(struct lfd_sess* p_sess)
{
if(0 == apr_strnatcasecmp(p_sess->passwd, p_sess->ftp_arg_str) )
	return 1;
else
	return 0;
}

static apr_status_t get_username_password(struct lfd_sess* p_sess)
{
	int pass_ok, user_ok;

	pass_ok = 0;
	user_ok = 0;
	while (!pass_ok || !user_ok)
	{
		lfd_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str,
					&p_sess->ftp_arg_str, 1);
		if (0 == apr_strnatcasecmp(p_sess->ftp_cmd_str, "USER"))
		{
			user_ok = handle_user_cmd(p_sess);
		}
		else if (0 == apr_strnatcasecmp(p_sess->ftp_cmd_str, "PASS") )
		{
			//handle pass command
			pass_ok = handle_pass_cmd(p_sess);
		}
		else
		{
			// unknown command; send error message
			lfd_cmdio_write(p_sess, FTP_LOGINERR, "Please log in with USER and PASS first.");
		}
		if(!pass_ok || !user_ok)
			lfd_cmdio_write(p_sess, FTP_LOGINERR, "Not logged in.");
	
	}
	return APR_SUCCESS;
}

static apr_status_t ftp_protocol_loop(struct lfd_sess * sess)
{
	return APR_SUCCESS;
}

void * lfd_worker_protocol_main(apr_thread_t * thd, void* param)
{
	apr_status_t	rc;
	apr_socket_t 	* sock = (apr_socket_t*) param;
	struct lfd_sess * sess;

	rc = lfd_sess_create(&sess, thd, sock);
	if(APR_SUCCESS != rc)
	{
		apr_socket_close(sock);
		lfd_log(LFD_ERROR, "lfd_sess_create failed with errorcode %d", rc);
		return NULL;
	}
	//if any of the following commands fail, the session obliteratoin code at the end is ran.

	if(APR_SUCCESS == rc)
	{
		rc = emit_greeting(sess);
		if(APR_SUCCESS != rc)
			lfd_log(LFD_ERROR, "emit_greeting failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
	}


	if(APR_SUCCESS == rc)
	{
		rc = get_username_password(sess);
		if(APR_SUCCESS != rc)
			lfd_log(LFD_ERROR, "get_username_password failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
	}


	if(APR_SUCCESS == rc)
	{
		rc = ftp_protocol_loop(sess);
		if(APR_SUCCESS != rc)
			lfd_log(LFD_ERROR, "ftp_protocol_loop failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
	}
	

	lfd_sess_destroy(sess);
	return NULL;
}
