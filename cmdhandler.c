#include <apr_strings.h>

#include "cmdhandler.h"
#include "ftpcodes.h"
#include "utils.h"
#include "fileops.h"

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
	char * path;
	
	ret = APR_SUCCESS;
	if (NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADCMD, "Command must have an argument.");
		return ret;
	}
	// make absolute path of the directory we want to remove ( user's home dir + the current path in this dir + arg )
	// if the path is invalid then lkl_dir_remove should return an appropiate error
	if('/' == *(p_sess->ftp_arg_str))
	{
		// absolute path
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->ftp_arg_str+1, NULL);
	}
	else
	{
		if(NULL != p_sess->rel_path)
			path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path, p_sess->ftp_arg_str, NULL);
		else
			path = apr_pstrcat(p_sess->loop_pool,p_sess->home_str, p_sess->ftp_arg_str, NULL);
	}
	if (NULL == path)
	{	
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	ret = lkl_dir_remove(path,p_sess->loop_pool);
	lfd_log(LFD_ERROR, "lkl_dir_remove failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
	if(APR_SUCCESS != ret)
	{
		lfd_log(LFD_ERROR, "lkl_dir_make failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot remove directory %s.", p_sess->ftp_arg_str);
	}
	else
		ret = lfd_cmdio_write(p_sess, FTP_RMDIROK, "Directory %s was removed.", p_sess->ftp_arg_str);

	return ret;
}

apr_status_t handle_dir_create(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	char * path;
	
	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADCMD, "Command must have an argument.");
		return ret;
	}
	// check to see if p_sess->ftp_arg_str is an absolute or relative path
	if('/' == *(p_sess->ftp_arg_str))
	{
		// absolute path
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->ftp_arg_str+1, NULL);
	}
	else
	{
		if(NULL != p_sess->rel_path)
			path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path, p_sess->ftp_arg_str, NULL);
		else
			path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->ftp_arg_str, NULL);
	}
	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	ret = lkl_dir_make(path, APR_FPROT_OS_DEFAULT, p_sess->loop_pool);
	if(APR_SUCCESS !=ret)
	{
		lfd_log(LFD_ERROR, "lkl_dir_make failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot create directory %s.", p_sess->ftp_arg_str);
	}
	else
		ret = lfd_cmdio_write(p_sess, FTP_MKDIROK, "Directory created.");

	return ret;
}

apr_status_t handle_pwd(struct lfd_sess *p_sess)
{
	apr_status_t ret;
		
	if(NULL == p_sess->rel_path)
		ret = lfd_cmdio_write(p_sess, FTP_PWDOK, "/");
	else
		ret = lfd_cmdio_write(p_sess, FTP_PWDOK, "/%s", p_sess->rel_path);
	return ret;
}

apr_status_t handle_cwd(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	char * path;
	
	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Command must have an argument.");
		return ret;
	}
	
	// is psess->ftp_arg_str a relative or absolute path ?
	if('/' == *(p_sess->ftp_arg_str))
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->ftp_arg_str+1, NULL);
	else
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path, p_sess->ftp_arg_str, NULL);
	
	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	// verify that the path represents a valid directory
	//TODO
	// ret = ??
	if(APR_SUCCESS == ret)
	{
		// add to the current rel_path ftp_arg_str
		if('/' == *(p_sess->ftp_arg_str))
			path = p_sess->ftp_arg_str;
		else
			path = apr_pstrcat(p_sess->sess_pool, p_sess->rel_path, p_sess->ftp_arg_str, NULL);
		if(NULL == path)
		{
			lfd_cmdio_write(p_sess, FTP_BADOPTS, "Server has encountered an internal error.");
			return APR_EINVAL;
		}
		p_sess->rel_path = path;
		ret = lfd_cmdio_write(p_sess, FTP_CWDOK, "Directory changed to %s.", path);
	}

	return ret;
}

apr_status_t handle_cdup(struct lfd_sess *sess)
{
	apr_status_t ret;

	ret = APR_SUCCESS;
	// shouldn't have an argument
	
	//TODO
	return ret;
}

apr_status_t handle_rnfr(struct lfd_sess *sess)
{
	apr_status_t ret;

	ret = APR_SUCCESS;
	if(NULL == sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(sess, FTP_BADOPTS, "Bad command argument.");
		return ret;
	}
	//TODO
	return ret;
}
	
apr_status_t handle_rnto(struct lfd_sess *sess)
{
	apr_status_t ret;

	ret = APR_SUCCESS;
	if(NULL == sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(sess, FTP_BADOPTS, "Bad command argument.");
		return ret;
	}
	//TODO
	return ret;
}

apr_status_t handle_type(struct lfd_sess *sess)
{
	lfd_cmdio_write(sess, FTP_TYPEOK, "TYPE ok");
	return APR_SUCCESS;
}


/**
 *@brief parses a string of delimited numbers between 0 and 255 and stores them in the p_items buffer
 */
const unsigned char * lkl_str_parse_uchar_string_sep(
		char * input_str, char sep, unsigned char* p_items,
  unsigned int items)
{
	char * last, * str;
	unsigned int i;
	apr_status_t rc;
	char sep_str[] = " ";
	sep_str[0] = sep;

	last = input_str;
	for (i = 0; i < items; i++)
	{
		apr_off_t this_number;

		str = apr_strtok(input_str, sep_str, &last);
		if((NULL == str) || ('\0' == *str))
			return 0;

		/* Sanity - check for too many or two few tokens! */
		if (    (i <  (items-1) && (0 == strlen(last))) ||
				       (i == (items-1) && (0 != strlen(last))))
		{
			return 0;
		}

		rc = apr_strtoff(&this_number, input_str, NULL, 10);
		if(APR_SUCCESS != rc)
			return 0;

		// validate range fits into one byte
		if(this_number < 0 || this_number > 255)
			return 0;

		/* If this truncates from int to uchar, we don't care */
		p_items[i] = (unsigned char) this_number;

		/* The right hand side of the comma now becomes the new string to breakdown */
		input_str = last;
	}
	return p_items;
}

static void port_cleanup(struct lfd_sess* p_sess)
{
	p_sess->p_port_sockaddr = NULL;
	//vsf_sysutil_sockaddr_clear(&p_sess->p_port_sockaddr);
}

static void pasv_cleanup(struct lfd_sess* p_sess)
{
	if (NULL != p_sess->pasv_listen_fd)
	{
		apr_socket_close(p_sess->pasv_listen_fd);
		p_sess->pasv_listen_fd = NULL;
	}
}



static void sockaddr_vars_set(apr_sockaddr_t *addr, apr_port_t port, const unsigned char * ipv4_bytes_in_network_order)
{
	addr->family = APR_INET;
	addr->sa.sin.sin_family = APR_INET;
	addr->sa.sin.sin_port = htons(port);
	addr->port = port;

	addr->salen = sizeof(struct sockaddr_in);
	addr->addr_str_len = 16;
	addr->ipaddr_ptr = &(addr->sa.sin.sin_addr);
	addr->ipaddr_len = sizeof(struct in_addr);
	memcpy(addr->ipaddr_ptr, ipv4_bytes_in_network_order, 4); //only copy 4 bytes=the length of the IPv4 address
}


// specify an alternate data port by use of the PORT command
apr_status_t handle_port(struct lfd_sess* p_sess)
{
	unsigned short the_port;
	unsigned char vals[6];
	const unsigned char* p_raw;
	struct apr_sockaddr_t * saddr;
	pasv_cleanup(p_sess);
	port_cleanup(p_sess);
	p_raw = lkl_str_parse_uchar_string_sep(p_sess->ftp_arg_str, ',', vals, sizeof(vals));
	if (p_raw == 0)
	{
		lfd_cmdio_write(p_sess, FTP_BADCMD, "Illegal PORT command.");
		return APR_EINVAL;
	}
	the_port = vals[4] << 8;
	the_port |= vals[5];
	saddr = apr_pcalloc(p_sess->sess_pool, sizeof(struct apr_sockaddr_t));

	//The PORT command supports IPv4 only
	saddr->pool = p_sess->sess_pool;
	sockaddr_vars_set(saddr, the_port, vals);

	p_sess->p_port_sockaddr = saddr;
	lfd_cmdio_write(p_sess, FTP_PORTOK, "PORT command successful. Consider using PASV.");
	return APR_SUCCESS;
}


static int port_active(struct lfd_sess* p_sess);
static int pasv_active(struct lfd_sess* p_sess);

static int port_active(struct lfd_sess* p_sess)
{
	int ret = 0;
	if (NULL != p_sess->p_port_sockaddr)
	{
		ret = 1;
		if (pasv_active(p_sess))
		{
			bug("port and pasv both active");
		}
	}
	return ret;
}

static int pasv_active(struct lfd_sess* p_sess)
{
	int ret = 0;
	if (NULL != p_sess->pasv_listen_fd)
	{
		ret = 1;
		if (port_active(p_sess))
		{
			bug("pasv and port both active");
		}
	}
	return ret;
}


static int data_transfer_checks_ok(struct lfd_sess* p_sess)
{
	if (!pasv_active(p_sess) && !port_active(p_sess))
	{
		lfd_cmdio_write(p_sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
		return 0;
	}
	return 1;
}

static void resolve_tilde(char ** p_str, struct lfd_sess* p_sess)
{
	unsigned int len;
	char * str;

	if (NULL == p_str)
		return;
	str = *p_str;
	len = strlen(str);

	if (len > 0 && '~' == str[0])
	{
		if (1 == len)
		{
			*p_str = apr_pstrdup(p_sess->sess_pool, p_sess->home_str);
		}
		else if('/' == str[1])
		{
			*p_str = apr_pstrcat(p_sess->sess_pool, p_sess->home_str, str+1, NULL);
		}
	}
}


apr_status_t handle_retr(struct lfd_sess *sess)
{/*
	int remote_fd;
	int opened_file;
	int is_ascii = 0;*/
	apr_off_t offset = sess->restart_pos;

	sess->restart_pos = 0;

	if (!data_transfer_checks_ok(sess))
	{
		return APR_EINVAL;
	}
	if (sess->is_ascii && offset != 0)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL,
				"No support for resume of ASCII transfer.");
		return APR_EINVAL;
	}
	resolve_tilde(&sess->ftp_arg_str, sess);



/*

	if (!vsf_access_check_file(&p_sess->ftp_arg_str))
	{
		vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
		return;
	}
	opened_file = str_open(&p_sess->ftp_arg_str, kVSFSysStrOpenReadOnly);
	if (vsf_sysutil_retval_is_error(opened_file))
	{
		vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");
		return;
	}
	// Lock file if required
	if (tunable_lock_upload_files)
	{
		vsf_sysutil_lock_file_read(opened_file);
	}
	vsf_sysutil_fstat(opened_file, &s_p_statbuf);
	// No games please
	if (!vsf_sysutil_statbuf_is_regfile(s_p_statbuf))
	{
		// Note - pretend open failed
		vsf_cmdio_write(p_sess, FTP_FILEFAIL, "Failed to open file.");
		goto file_close_out;
	}
	// Set the download offset (from REST) if any
	if (offset != 0)
	{
		vsf_sysutil_lseek_to(opened_file, offset);
	}
	str_alloc_text(&s_mark_str, "Opening ");
	if (tunable_ascii_download_enable && p_sess->is_ascii)
	{
		str_append_text(&s_mark_str, "ASCII");
		is_ascii = 1;
	}
	else
	{
		str_append_text(&s_mark_str, "BINARY");
	}
	str_append_text(&s_mark_str, " mode data connection for ");
	str_append_str(&s_mark_str, &p_sess->ftp_arg_str);
	str_append_text(&s_mark_str, " (");
	str_append_filesize_t(&s_mark_str,
			       vsf_sysutil_statbuf_get_size(s_p_statbuf));
	str_append_text(&s_mark_str, " bytes).");
	remote_fd = get_remote_transfer_fd(p_sess, str_getbuf(&s_mark_str));
	if (vsf_sysutil_retval_is_error(remote_fd))
	{
		goto port_pasv_cleanup_out;
	}
	trans_ret = vsf_ftpdataio_transfer_file(p_sess, remote_fd,
			opened_file, 0, is_ascii);
	vsf_ftpdataio_dispose_transfer_fd(p_sess);
	p_sess->transfer_size = trans_ret.transferred;
	// Log _after_ the blocking dispose call, so we get transfer times right
	if (trans_ret.retval == 0)
	{
		vsf_log_do_log(p_sess, 1);
	}
  // Emit status message _after_ blocking dispose call to avoid buggy FTP
	// clients truncating the transfer.

	if (trans_ret.retval == -1)
	{
		vsf_cmdio_write(p_sess, FTP_BADSENDFILE, "Failure reading local file.");
	}
	else if (trans_ret.retval == -2)
	{
		vsf_cmdio_write(p_sess, FTP_BADSENDNET, "Failure writing network stream.");
	}
	else
	{
		vsf_cmdio_write(p_sess, FTP_TRANSFEROK, "File send OK.");
	}
	check_abor(p_sess);
port_pasv_cleanup_out:
		port_cleanup(p_sess);
  pasv_cleanup(p_sess);
file_close_out:
		vsf_sysutil_close(opened_file);
	*/
	return APR_SUCCESS;
}


