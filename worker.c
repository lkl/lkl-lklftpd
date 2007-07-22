// main FTP protocol loop handling
#include <string.h>

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


static int handle_user_cmd(struct lfd_sess* p_sess)
{
	return (0 == apr_strnatcasecmp(p_sess->user, p_sess->ftp_arg_str));
}

static int handle_pass_cmd(struct lfd_sess* p_sess)
{
	return (0 == apr_strnatcasecmp(p_sess->passwd, p_sess->ftp_arg_str) );
}

static int lfd_cmdio_cmd_equals(struct lfd_sess*sess, const char * cmd)
{
	return (0 == apr_strnatcasecmp(sess->ftp_cmd_str, cmd));
}

static apr_status_t get_username_password(struct lfd_sess* p_sess)
{
	apr_status_t rc;
	int pass_ok = 0, user_ok = 0;
	int nr_tries = 0;


	do
	{
		nr_tries ++;

		rc = lfd_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str, &p_sess->ftp_arg_str, 1);
		if(APR_SUCCESS != rc)
			return rc;

		if(lfd_cmdio_cmd_equals(p_sess, "USER"))
		{
			user_ok = handle_user_cmd(p_sess);
		}
		else
		{
			// unknown command; send error message
			lfd_cmdio_write(p_sess, FTP_LOGINERR, "Please log in with USER and PASS first.");
			continue; //don't ask for the password
		}

		lfd_cmdio_write(p_sess, FTP_GIVEPWORD, "Password required for user.");

		rc = lfd_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str, &p_sess->ftp_arg_str, 1);
		if(APR_SUCCESS != rc)
			return rc;

		if(lfd_cmdio_cmd_equals(p_sess, "PASS"))
		{
			pass_ok = handle_pass_cmd(p_sess);
			if(pass_ok && user_ok)
			{
				lfd_cmdio_write(p_sess, FTP_LOGINOK, "LOGIN OK.");
				break;
			}
		}
		else
		{
			// unknown command; send error message
			lfd_cmdio_write(p_sess, FTP_LOGINERR, "Please log in with USER and PASS first.");
		}

		lfd_cmdio_write(p_sess, FTP_LOGINERR, "Incorrect login credentials.");
	}
	while(nr_tries < lfd_config_max_login_attempts);

	if (nr_tries >= lfd_config_max_login_attempts)
		return APR_EINVAL;

	return APR_SUCCESS;
}

static apr_status_t handle_passive(struct lfd_sess * sess)
{
	return APR_SUCCESS;
}

static apr_status_t handle_active(struct lfd_sess * sess)
{
	return APR_SUCCESS;
}
static apr_status_t handle_syst(struct lfd_sess * sess)
{
	apr_status_t rc;
	//TODO: check the RFC: is this the default setting. If not have it in a config file and fix the message.
	rc = lfd_cmdio_write(sess, FTP_SYSTOK, "UNIX Type: L8");
	return rc;
}
static apr_status_t handle_quit(struct lfd_sess * sess)
{
	apr_status_t rc;
	rc = lfd_cmdio_write(sess, FTP_GOODBYE, "Goodbye, So long, Farwell.");
	return rc;
}

static apr_status_t ftp_protocol_loop(struct lfd_sess * sess)
{
	apr_status_t rc = APR_SUCCESS;
	while(APR_SUCCESS == rc)
	{
		rc = lfd_cmdio_get_cmd_and_arg(sess, &sess->ftp_cmd_str, &sess->ftp_arg_str, 1);
		if(APR_SUCCESS != rc)
			return rc;

		if(lfd_cmdio_cmd_equals(sess, "PASIVE"))
		{
			rc = handle_passive(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "ACTIVE"))
		{
			rc = handle_active(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "SYST"))
		{
			rc = handle_syst(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "QUIT"))
		{
			rc = handle_quit(sess);
			return rc;
		}
		else //default
		{
			printf("The cmd [%s] has no installed handler! \n", sess->ftp_cmd_str);
		}
	}
	return rc;
}

void * lfd_worker_protocol_main(apr_thread_t * thd, void* param)
{
	apr_status_t	rc;
	apr_socket_t 	* sock = (apr_socket_t*) param;
	struct lfd_sess * sess;

	rc = lfd_sess_create(&sess, thd, sock);
	if(APR_SUCCESS != rc)
	{
		//cannot call lfs_sess_destroy because we were unable to construct the object.
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
