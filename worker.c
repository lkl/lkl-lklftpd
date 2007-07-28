// main FTP protocol loop handling
#include <string.h>

#include <apr_network_io.h>
#include <apr_errno.h>
#include <apr_strings.h>

#include "worker.h"
#include "config.h"
#include "utils.h"
#include "ftpcodes.h"
#include "cmdhandler.h"
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

static int lfd_cmdio_cmd_equals(struct lfd_sess*sess, const char * cmd)
{
	return sess->ftp_cmd_str && (0 == apr_strnatcasecmp(sess->ftp_cmd_str, cmd));
}

static void init_username_related_fields(struct lfd_sess * sess)
{
	sess->user = apr_pstrdup(sess->sess_pool, sess->user);
	sess->home_str = apr_pstrcat(sess->sess_pool, "/home/", sess->user, "/", NULL);
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

	init_username_related_fields(p_sess);

	return APR_SUCCESS;
}

static apr_status_t ftp_protocol_loop(struct lfd_sess * sess)
{
	apr_status_t rc = APR_SUCCESS;
	int rnfrto; // "rename from" and "rename to" should go togheter
	char * temp_name;

	temp_name = NULL;
	rnfrto = 0;
	while(APR_SUCCESS == rc)
	{
		apr_pool_clear(sess->loop_pool);
		rc = lfd_cmdio_get_cmd_and_arg(sess, &sess->ftp_cmd_str, &sess->ftp_arg_str, 1);
		if(APR_SUCCESS != rc)
			return rc;
		// special case
		if(lfd_cmdio_cmd_equals(sess, "RNTO"))
		{
			if(rnfrto)
			{
				rnfrto = 0;
				rc = handle_rnto(sess, temp_name);
			}
			else
				rc = handle_bad_rnto(sess);
			continue;
		}
		// here we treat all the other cases
		if(rnfrto){
			rnfrto = 0;
			rc = handle_bad_rnto(sess);
			continue;
		}

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
		else if(lfd_cmdio_cmd_equals(sess, "ABOR"))
		{
			rc = handle_abort(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "PORT"))
		{
			rc = handle_port(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "RMD"))
		{
			rc = handle_dir_remove(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "MKD"))
		{
			rc = handle_dir_create(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "PWD"))
		{
			rc = handle_pwd(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "CWD"))
		{
			rc = handle_cwd(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "CDUP"))
		{
			rc = handle_cdup(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "RNFR"))
		{
			rc = handle_rnfr(sess, &temp_name);
			if(APR_SUCCESS == rc && NULL != temp_name)
				rnfrto = 1;
		}
		else if(lfd_cmdio_cmd_equals(sess, "TYPE"))
		{
			rc = handle_type(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "RETR"))
		{
			rc = handle_retr(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "STOR"))
		{
			rc = handle_stor(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "DELE"))
		{
			rc = handle_dele(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "STOU"))
		{
			rc = handle_stou(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "LIST"))
		{
			rc = handle_list(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "APPE"))
		{
			rc = handle_appe(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "ALLO"))
		{
			rc = lfd_cmdio_write(sess, FTP_ALLOOK, "ALLO command ignored.");
		}
		else if(lfd_cmdio_cmd_equals(sess, "REIN"))
		{
			rc = lfd_cmdio_write(sess, FTP_COMMANDNOTIMPL, "REIN not implemented.");
		}
		else if(lfd_cmdio_cmd_equals(sess, "ACCT"))
		{
			rc = lfd_cmdio_write(sess, FTP_COMMANDNOTIMPL, "ACCT not implemented.");
		}
		else if(lfd_cmdio_cmd_equals(sess, "SMNT"))
		{
			rc = lfd_cmdio_write(sess, FTP_COMMANDNOTIMPL, "SMNT not implemented.");
		}

		else //default
		{
			printf("The cmd [%s] has no installed handler! \n", sess->ftp_cmd_str);
			if(NULL != sess->ftp_arg_str)
				printf("The cmd args were [%s] \n", sess->ftp_arg_str);
			lfd_cmdio_write(sess, FTP_COMMANDNOTIMPL, "Command not implemented.");
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
	//if any of the following stages fail, the session obliteration code at the end is run.

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
