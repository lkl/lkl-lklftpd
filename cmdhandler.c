#include <apr_strings.h>

#include "cmdhandler.h"
#include "ftpcodes.h"

int handle_user_cmd(struct lfd_sess* p_sess)
{
	return (0 == apr_strnatcasecmp(p_sess->user, p_sess->ftp_arg_str));
}

int handle_pass_cmd(struct lfd_sess* p_sess)
{
	return (0 == apr_strnatcasecmp(p_sess->passwd, p_sess->ftp_arg_str) );
}

int lfd_cmdio_cmd_equals(struct lfd_sess*sess, const char * cmd)
{
	return (0 == apr_strnatcasecmp(sess->ftp_cmd_str, cmd));
}

apr_status_t handle_passive(struct lfd_sess * sess)
{
	//TODO:implement
	return APR_SUCCESS;
}

apr_status_t handle_active(struct lfd_sess * sess)
{
	//TODO:implement
	return APR_SUCCESS;
}

apr_status_t handle_syst(struct lfd_sess * sess)
{
	apr_status_t rc;
	//TODO: check the RFC: is this the default setting. If not have it in a config file and fix the message.
	rc = lfd_cmdio_write(sess, FTP_SYSTOK, "UNIX Type: L8");
	return rc;
}

apr_status_t handle_quit(struct lfd_sess * p_sess)
{
	apr_status_t ret;
	
	// wait for the active transfer to end
	if(0 != p_sess->data_conn->in_progress)
	{
		apr_thread_join(&ret, p_sess->data_conn->data_conn_th);
		lfd_data_sess_destroy(p_sess->data_conn);
	}	
	lfd_cmdio_write(p_sess,  FTP_GOODBYE, "See you later, aligator.\r\n");
	lfd_sess_destroy(p_sess);
	return APR_SUCCESS;
}

apr_status_t handle_abort(struct lfd_sess* p_sess)
{
	// stop the active data transfer and close the data connection
	if(0 == p_sess->data_conn->in_progress)
	{
		lfd_cmdio_write(p_sess, FTP_ABOR_NOCONN, "Nothing to abort.\r\n");
		return APR_SUCCESS;
	}
	apr_thread_exit(p_sess->data_conn->data_conn_th, APR_SUCCESS);
	lfd_data_sess_destroy(p_sess->data_conn);
	lfd_cmdio_write(p_sess, FTP_ABOROK, "File transfer aborted.\r\n");	
	return APR_SUCCESS;
}

apr_status_t handle_dir_remove(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	//char * path;
	
	// make path for the directory psess->ftp_arg_str
	//TODO

	ret = APR_SUCCESS;
	if (NULL == p_sess->ftp_arg_str)
	{
		lfd_cmdio_write(p_sess, FTP_BADCMD, "Bad command argument.");
		return APR_SUCCESS;
	}
	//ret = lkl_dir_remove(path,psess->loop_pool);
	
	if(APR_SUCCESS != ret)
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot remove directory.");
	else
		lfd_cmdio_write(p_sess, FTP_RMDIROK, "Directory was removed.");	
	
	return APR_SUCCESS;
}

apr_status_t handle_dir_create(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	//char * path;
	
	// make path for the desired directory
	//TODO
	
	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		lfd_cmdio_write(p_sess, FTP_BADCMD, "Bad command argument.");
		return APR_SUCCESS;
	}
	//ret = lkl_dir_make(path, /*flags*/, p_sess->loop_pool);
	if(APR_SUCCESS !=ret)
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot create directory.");
	else
		lfd_cmdio_write(p_sess, FTP_MKDIROK, "Directory created.");
	
	return APR_SUCCESS;
}
