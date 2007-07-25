#include <apr_strings.h>

#include "cmdhandler.h"
#include "ftpcodes.h"
#include "utils.h"
#include "fileops.h"

//TODO: rename and move to apppropriate place
#define VSFTP_DATA_BUFSIZE      65536

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
		if(0 != apr_strnatcmp(p_sess->rel_path,"/"))
			path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path+1, p_sess->ftp_arg_str, NULL);
		else
			path = apr_pstrcat(p_sess->loop_pool,p_sess->home_str, p_sess->ftp_arg_str, NULL);
	}
	if (NULL == path)
	{	
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	ret = lkl_dir_remove(path,p_sess->loop_pool);
	
	if(APR_SUCCESS != ret)
	{
		lfd_log(LFD_ERROR, "lkl_dir_remove failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
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
		if(0 != apr_strnatcmp( p_sess->rel_path, "/"))
			path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path+1, p_sess->ftp_arg_str, NULL);
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
		ret = lfd_cmdio_write(p_sess, FTP_MKDIROK, "%s/%s Directory created.",p_sess->rel_path, p_sess->ftp_arg_str);

	return ret;
}

apr_status_t handle_pwd(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	
	ret = lfd_cmdio_write(p_sess, FTP_PWDOK,p_sess->rel_path);
	return ret;
}

static apr_status_t dir_exists(const char * path, apr_pool_t * pool)
{
	apr_finfo_t finfo;
	apr_status_t ret;
	// TODO - check if the given path represents a real directory
	
	ret = apr_stat(&finfo, path, APR_FINFO_TYPE, pool);
	if( ret == APR_SUCCESS || ret == APR_INCOMPLETE )
	{
		if (finfo.valid & APR_FINFO_TYPE)
			if(finfo.filetype == APR_DIR)
				return APR_SUCCESS;
	}
	return APR_EINVAL;
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
		if(0 ==apr_strnatcmp(p_sess->rel_path, "/"))
			path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->ftp_arg_str, NULL);
	else
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path+1, p_sess->ftp_arg_str, NULL);
	
	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	// verify that the path represents a valid directory
	ret = dir_exists(path, p_sess->loop_pool);

	if(APR_SUCCESS == ret)
	{
		// add to the current rel_path ftp_arg_str
		if('/' == *(p_sess->ftp_arg_str))
			p_sess->rel_path = apr_psprintf(p_sess->loop_pool,"%s",p_sess->ftp_arg_str);
		else if( 0 == apr_strnatcmp(p_sess->rel_path,"/"))
			p_sess->rel_path = apr_psprintf(p_sess->loop_pool,"/%s", p_sess->ftp_arg_str);
		else 
			p_sess->rel_path = apr_psprintf(p_sess->loop_pool,"%s/%s", p_sess->rel_path, p_sess->ftp_arg_str);
		
		ret = lfd_cmdio_write(p_sess, FTP_CWDOK, "Directory changed to %s.", p_sess->rel_path);
	}
	else 
	{
		lfd_log(LFD_ERROR, "dir_exists failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot change directory.");
	}
	return ret;
}

apr_status_t handle_cdup(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	char * pos, * last;
	
	ret = APR_SUCCESS;
	// shouldn't have an argument
	// changes the current directory with it's parent
	if(0 == apr_strnatcmp( p_sess->rel_path, "/"))
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Already in root directory.");
		return ret;
	}
	pos = p_sess->rel_path;
	last = pos;
	while(*pos)
	{
		if(*pos == '/')
			last = pos;
		pos++; 
	}
	// end the string here, before pos 
	if(last == p_sess->rel_path)
		p_sess->rel_path = "/";
	else
		*last = '\0';
	
	ret = lfd_cmdio_write(p_sess, FTP_ALLOOK, "Changed to directory %s.",p_sess->rel_path);
	return ret;
}

apr_status_t handle_rnfr(struct lfd_sess *p_sess, char ** temp_path)
{
	apr_status_t ret;
	char * path;
	
	*temp_path = NULL;
	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Bad command argument.");
		return ret;
	}
	
	//check if the argument is a valid path 
	if('/' == *(p_sess->ftp_arg_str))
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->ftp_arg_str+1, NULL);
	else
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path, p_sess->ftp_arg_str, NULL);
	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	ret = dir_exists(path, p_sess->loop_pool);

	if(APR_SUCCESS == ret)
	{
		*temp_path = path;
		ret = lfd_cmdio_write(p_sess, FTP_RNFROK, "Rename from ok.");
	}
	else
	{
		lfd_log(LFD_ERROR, "dir_exists failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot rename directory %s.", p_sess->ftp_arg_str);
	}
	return ret;
}
	
apr_status_t handle_rnto(struct lfd_sess *p_sess, char * old_path)
{
	apr_status_t ret;
	char * path;

	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Bad command argument.");
		return ret;
	}
	
	// obtain the new path and call lkl_file_rename
	if('/' == *(p_sess->ftp_arg_str))
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->ftp_arg_str+1, NULL);
	else
		path = apr_pstrcat(p_sess->loop_pool, p_sess->home_str, p_sess->rel_path, p_sess->ftp_arg_str, NULL);
	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	ret = lkl_file_rename(old_path, path, p_sess->loop_pool);
	
	if(APR_SUCCESS != ret)
	{
		lfd_log(LFD_ERROR, "lkl_file_rename failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot rename directory %s.", p_sess->ftp_arg_str);	
	}
	else
		ret = lfd_cmdio_write(p_sess, FTP_RENAMEOK, "Directory was succesfully renamed.");
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

static void init_data_sock_params(struct lfd_sess* p_sess, apr_socket_t * sock_fd)
{
	if (NULL != p_sess->data_conn->data_sock)
	{
		bug("data descriptor still present in init_data_sock_params");
	}
	p_sess->data_conn->data_sock = sock_fd;
	p_sess->data_conn->in_progress = 0;
	apr_socket_opt_set(sock_fd, APR_SO_KEEPALIVE, 1);

	//Set up lingering, so that we wait for all data to transfer, and report
	// more accurate transfer rates.
	apr_socket_opt_set(sock_fd, APR_SO_LINGER, 1);
}

static apr_status_t get_bound_and_connected_ftp_port_sock(struct lfd_sess* p_sess, apr_socket_t ** psock)
{
	apr_socket_t	* sock;
	apr_sockaddr_t	* saddr;
	apr_status_t	  rc;

	*psock = NULL;

	rc = apr_sockaddr_info_get(&saddr, NULL, APR_UNSPEC, 20, 0, p_sess->loop_pool);
	if(APR_SUCCESS != rc)
		return rc;

	rc = apr_socket_create(&sock, APR_UNSPEC, SOCK_STREAM, APR_PROTO_TCP, p_sess->loop_pool);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_socket_opt_set(sock, APR_SO_REUSEADDR, 1);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_socket_bind(sock, saddr);
	if(APR_SUCCESS != rc)
		return rc;

	rc = apr_socket_connect (sock, p_sess->p_port_sockaddr);
	if(APR_SUCCESS != rc)
		return rc;

	*psock = sock;
	return APR_SUCCESS;
}


static apr_status_t ftpdataio_get_port_fd(struct lfd_sess* p_sess, apr_socket_t ** psock)
{
	apr_status_t	  rc;
	apr_socket_t	* remote_fd;
	*psock = NULL;
	rc = get_bound_and_connected_ftp_port_sock(p_sess, &remote_fd);
	if (APR_SUCCESS != rc)
	{
		lfd_cmdio_write(p_sess, FTP_BADSENDCONN, "Failed to establish connection.");
		return rc;
	}

	init_data_sock_params(p_sess, remote_fd);
	*psock = remote_fd;

	return APR_SUCCESS;
}


static apr_socket_t* get_remote_transfer_fd(struct lfd_sess* p_sess, const char* p_status_msg)
{
	apr_socket_t	* remote_fd;

	if (!pasv_active(p_sess) && !port_active(p_sess))
	{
		bug("neither PORT nor PASV active in get_remote_transfer_fd");
	}
	p_sess->data_conn->abor_received = 0;
	if (pasv_active(p_sess))
	{
		//remote_fd = lfd_ftpdataio_get_pasv_fd(p_sess);
	}
	else
	{
		ftpdataio_get_port_fd(p_sess, & remote_fd);
	}
	if (NULL == remote_fd)
	{
		return NULL;
	}
	lfd_cmdio_write(p_sess, FTP_DATACONN, p_status_msg);
	return remote_fd;
}

static apr_status_t lfd_socket_write_full(apr_socket_t * fd, const void* p_buf, apr_size_t * psize)
{
	apr_status_t	rc;
	apr_size_t	num_written;
	apr_size_t	size = *psize;
	if (size > INT_MAX)
	{
		bug("size too big in vsf_sysutil_write_loop");
	}

	*psize = 0;
	while (1)
	{
		num_written = size;
		rc = apr_socket_send(fd, (const char*)p_buf + num_written, &num_written);
		if (APR_SUCCESS != rc)
		{
			return rc;
		}
		else if (0 == num_written)
		{
			/* Written all we're going to write.. */
			return APR_SUCCESS;
		}

		*psize += num_written;
		size -= num_written;
		if (0 == size)
		{
			/* Hit the write target, cool. */
			return APR_SUCCESS;
		}
	}
}

struct lfd_transfer_ret
{
	int retval;
	apr_size_t transferred;

};
static struct lfd_transfer_ret do_file_send_rwloop(struct lfd_sess* p_sess, apr_socket_t * sock, lkl_file_t * file_fd, int is_ascii)
{
	char* p_readbuf;
	char* p_asciibuf;
	struct lfd_transfer_ret ret_struct = { 0, 0 };
	apr_size_t chunk_size = 4096;
	char* p_writefrom_buf;
	p_readbuf = apr_palloc(p_sess->loop_pool, VSFTP_DATA_BUFSIZE);
	p_asciibuf= apr_palloc(p_sess->loop_pool, VSFTP_DATA_BUFSIZE*2);

	if (is_ascii)
	{
		p_writefrom_buf = p_asciibuf;
	}
	else
	{
		p_writefrom_buf = p_readbuf;
	}

	while (1)
	{
		apr_size_t	num_to_write;
		apr_size_t	bytes_read = chunk_size;
		apr_size_t	bytes_written;
		apr_status_t	rc;
		rc = lkl_file_read(file_fd, p_readbuf, &bytes_read);
		if((APR_SUCCESS != rc) && (APR_EOF != rc))
		{
			ret_struct.retval = -1;
			return ret_struct;
		}
		else if (0 == bytes_read)
		{
			/* Success - cool */
			return ret_struct;
		}
		if (is_ascii)
		{
			num_to_write = lfd_ascii_bin_to_ascii(p_readbuf, p_asciibuf, bytes_read);
		}
		else
		{
			num_to_write = bytes_read;
		}

		bytes_written = bytes_read;
		rc = lfd_socket_write_full(sock, p_writefrom_buf, &bytes_written);
		if((APR_SUCCESS != rc) || (bytes_written != bytes_read))
		{
			ret_struct.retval = -2;
			return ret_struct;
		}
		ret_struct.transferred += bytes_written;
	}
}

void lfd_dispose_transfer_fd(struct lfd_sess* p_sess)
{

	apr_socket_opt_set(p_sess->data_conn->data_sock, APR_SO_LINGER, 0);
	apr_socket_close(p_sess->data_conn->data_sock);
	p_sess->data_conn->data_sock = NULL;
}

static void check_abor(struct lfd_sess* p_sess)
{
	/* If the client sent ABOR, respond to it here */
	if (p_sess->data_conn->abor_received)
	{
		p_sess->data_conn->abor_received = 0;
		lfd_cmdio_write(p_sess, FTP_ABOROK, "ABOR successful.");
	}
}

apr_status_t handle_retr(struct lfd_sess *sess)
{/*
	int remote_fd;
	int opened_file;
	int is_ascii = 0;*/
	char			* msg;
	apr_off_t		  offset = sess->restart_pos;
	apr_status_t		  rc;
	lkl_file_t 		* file;
	apr_socket_t		* remote_fd;
	apr_finfo_t		  finfo;
	struct lfd_transfer_ret   trans_ret;

	sess->restart_pos = 0;

	if (!data_transfer_checks_ok(sess))
	{
		return APR_EINVAL;
	}
	if (sess->is_ascii && offset != 0)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "No support for resume of ASCII transfer.");
		return APR_EINVAL;
	}
	resolve_tilde(&sess->ftp_arg_str, sess);


	rc = lkl_file_open(&file, sess->ftp_arg_str, APR_FOPEN_READ|APR_FOPEN_BINARY|APR_FOPEN_LARGEFILE|APR_FOPEN_SENDFILE_ENABLED, 0, sess->loop_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "Failed to open file.");
		return rc;
	}
	rc = apr_stat(&finfo, sess->ftp_arg_str, APR_FINFO_TYPE|APR_FINFO_SIZE, sess->loop_pool);
	if((APR_SUCCESS != rc) || (APR_REG != finfo.filetype))
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, (APR_SUCCESS == rc)?"File type is not of regular type":"Failed to stat file.");
		lkl_file_close(file);
		return rc;
	}
	if (0 != offset)
	{
		rc = lkl_file_seek(file, APR_SET, &offset);
		if(APR_SUCCESS != rc)
		{
			lfd_cmdio_write(sess, FTP_FILEFAIL, "Failed to seek into file.");
			lkl_file_close(file);
			return rc;
		}
	}


	msg = apr_psprintf(sess->loop_pool, "Opening %s %s %s %s %u %s",
			sess->is_ascii?"ASCII":"BINARY", " mode data connection for ",
      			sess->ftp_arg_str, " ( ", (unsigned int)finfo.size, " bytes).");



	remote_fd = get_remote_transfer_fd(sess, msg);
	if(NULL == remote_fd)
	{
		port_cleanup(sess);
		pasv_cleanup(sess);
		lkl_file_close(file);
		return APR_EINVAL;
	}

	trans_ret = do_file_send_rwloop(sess, remote_fd, file, sess->is_ascii);
	lfd_dispose_transfer_fd(sess);

	if (trans_ret.retval == -1)
	{
		lfd_cmdio_write(sess, FTP_BADSENDFILE, "Failure reading local file.");
	}
	else if (trans_ret.retval == -2)
	{
		lfd_cmdio_write(sess, FTP_BADSENDNET, "Failure writing network stream.");
	}
	else
	{
		lfd_cmdio_write(sess, FTP_TRANSFEROK, "File send OK.");
	}
	check_abor(sess);
	port_cleanup(sess);
	pasv_cleanup(sess);

	lkl_file_close(file);

	return APR_SUCCESS;
}


