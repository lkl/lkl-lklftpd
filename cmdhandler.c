#include <string.h>

#include <apr_strings.h>
#include <apr_tables.h>
#include <apr_random.h>
#include <apr_poll.h>

#include <assert.h>
#include "cmdhandler.h"
#include "ftpcodes.h"
#include "utils.h"
#include "fileops.h"
#include "config.h"

//TODO: rename and move to apppropriate place
#define VSFTP_DATA_BUFSIZE      65536

static void port_cleanup(struct lfd_sess* p_sess);
static void pasv_cleanup(struct lfd_sess* p_sess);
const unsigned char * lkl_str_parse_uchar_string_sep(char *, char, unsigned char*, unsigned int);

int handle_user_cmd(struct lfd_sess* p_sess)
{
	//###: any user is accepted
	//we're allocating from the loop pool for now. 
	// Allocate from the sess pool when the user logs in successfully
	p_sess->user = apr_pstrdup(p_sess->loop_pool, p_sess->ftp_arg_str);
	
	//sanitize the input: We don't accept usernames with '/' in the name 
	// because we use the name to determine the home directory as in 
	// "/home/%USERNAME%" and a slash in the name will point to a subdirectory
	// of another user.
	if(strchr(p_sess->user, '/'))
		return 0;
	return 1;
}

int handle_pass_cmd(struct lfd_sess* p_sess)
{
	//###: any pass is accepted
	return 1;
}

static apr_status_t bind_to_random_passive_port(struct lfd_sess * sess, apr_sockaddr_t	** psaddr, apr_port_t * pport)
{
	apr_port_t 	bind_retries = lfd_config_max_sockets_to_try_to_bind;
	apr_port_t	min_port = (lfd_config_min_pasv_port < 1023 ) ? 1024  : lfd_config_min_pasv_port;
	apr_port_t	max_port = (lfd_config_min_pasv_port > 65535) ? 65535 : lfd_config_max_pasv_port;
	apr_port_t	rand_port;
	unsigned char	randBuff[2];
	apr_status_t	rc = -1;
	apr_socket_t	*listen_fd;
	apr_sockaddr_t	*saddr;
	
	
	while(--bind_retries)
	{
		rc = apr_generate_random_bytes(randBuff, (apr_size_t)2);
		if(APR_SUCCESS != rc)
		{
			continue;
		}
		//get random short value
		rand_port = (apr_port_t) (randBuff[0] << 8 | randBuff[1]);
		//scale it to fit the interval
		rand_port = (apr_port_t) ( ( (apr_uint64_t) (max_port - min_port + 1)) * rand_port / max_port + min_port );
		rc = apr_sockaddr_info_get(&saddr, lfd_config_listen_host, APR_UNSPEC, rand_port, 0, sess->sess_pool);
		if(APR_SUCCESS != rc)
		{
			continue;
		}
		rc = apr_socket_create(&listen_fd, saddr->family, SOCK_STREAM, APR_PROTO_TCP, sess->sess_pool);
		if(APR_SUCCESS != rc)
		{
			continue;
		}
		
		rc = apr_socket_opt_set(listen_fd, APR_SO_REUSEADDR, 1);
		if(APR_SUCCESS != rc)
		{
			continue;
		}
		rc = apr_socket_bind(listen_fd, saddr);
		if(APR_SUCCESS == rc)
		{
			break;
		}
	}
	
	if(!bind_retries)
	{
		lfd_log(LFD_ERROR, "bind_to_random_passive_port failed");
		return APR_ENOSOCKET;
	}
	if (APR_SUCCESS == rc)
	{
		*pport = rand_port;
		*psaddr = saddr;
		sess->pasv_listen_fd = listen_fd;
	}
	return rc;
}

apr_status_t handle_passive(struct lfd_sess * sess)
{
	apr_status_t	rc;
	apr_sockaddr_t	*saddr;
	apr_port_t	port;
	unsigned char	vals[6];
	char		*addr;

	port_cleanup(sess);
	pasv_cleanup(sess);

	rc = bind_to_random_passive_port(sess, &saddr, &port);
	if(APR_SUCCESS != rc)
	{
		return rc;
	}
	
	rc = apr_socket_listen(sess->pasv_listen_fd, 1); //backlog of one!
	if(APR_SUCCESS != rc)
	{
		return rc;
	}
	
	//get the IP represented as a string of characters
	rc = apr_sockaddr_ip_get(&addr, saddr);
	if(APR_SUCCESS != rc)
	{
		return rc;
	}
	
	// fill the first 4 elemnts of vals with the numbers from the IP address
	lkl_str_parse_uchar_string_sep(addr, '.', vals, 4);
	
	// append the decomposed port number
	vals[4] = (unsigned char) (port >> 8);
	vals[5] = (unsigned char) (port & 0xFF);
	
	rc = lfd_cmdio_write(sess, FTP_PASVOK,"Entering Passive Mode. %d,%d,%d,%d,%d,%d", vals[0], vals[1], vals[2], vals[3], vals[4], vals[5]);
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
	lfd_cmdio_write(p_sess,  FTP_GOODBYE, "See you later, aligator.");
	return APR_SUCCESS;
}

apr_status_t handle_abort(struct lfd_sess* p_sess)
{
	// stop the active data transfer and close the data connection
	if(0 == p_sess->data_conn->in_progress)
	{
		lfd_cmdio_write(p_sess, FTP_ABOR_NOCONN, "Nothing to abort.");
		return APR_SUCCESS;
	}
	if(NULL != p_sess->data_conn->data_conn_th)
		apr_thread_exit(p_sess->data_conn->data_conn_th, APR_SUCCESS);

	lfd_data_sess_destroy(p_sess->data_conn);
	lfd_cmdio_write(p_sess, FTP_ABOROK, "File transfer aborted.");
	return APR_SUCCESS;
}


static char * resolve_tilde(const char * str, struct lfd_sess* p_sess, apr_pool_t * allocator_pool)
{
	size_t len;
	len = strlen(str);
	
	if (NULL == str)
		return NULL;
	
	
	if (len > 0 && '~' == str[0])
	{
		if (1 == len)
		{
			return apr_pstrdup(allocator_pool, p_sess->home_str);
		}
		else if('/' == str[1])
		{
			return apr_pstrcat(allocator_pool, p_sess->home_str, str+2, NULL);
		}
		else
		{
			//we don't support ~asdf kind of paths.
			return NULL;
		}
	}
	return apr_pstrdup(allocator_pool, str);
}

static char * get_abs_path(struct lfd_sess *p_sess, apr_pool_t * allocator_pool)
{
	char * path;

	if ( (NULL == p_sess->ftp_arg_str) || ('\0' == p_sess->ftp_arg_str[0]) )
		return NULL;
	
	//this is only temp, so allocating from loop_pool is fine.
	path = resolve_tilde(p_sess->ftp_arg_str, p_sess, p_sess->loop_pool);
	
	if (NULL == path)
		return NULL;
	
	
	//TODO: this is Linux centric
	if('/' == path[0])
	{
		//the path is already absolute, 
		// just need to allocate it from an appropriate pool
		path = apr_pstrdup(allocator_pool, path);
	}
	else
	{
		path = apr_pstrcat(allocator_pool, p_sess->cwd_path, path, NULL);
	}
	
	//TODO: remove double (or more) "/" from the path
	return path;
}




apr_status_t handle_dir_remove(struct lfd_sess *p_sess)
{
	apr_status_t rc;
	char * rpath;

	rc = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "Command must have an argument.");
		return rc;
	}
	
	// check to see if p_sess->ftp_arg_str is an absolute or relative path
	rpath = get_abs_path(p_sess, p_sess->loop_pool);
	if(NULL == rpath)
	{
		lfd_cmdio_write(p_sess, FTP_FILEFAIL, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	rc = lkl_dir_remove(rpath, p_sess->loop_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lkl_dir_remove failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(p_sess, rc));
		rc = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "Cannot remove directory %s.", rpath);
	}
	else
	{
		rc = lfd_cmdio_write(p_sess, FTP_RMDIROK, "%s Directory deleted.",rpath);
	}
	
	return rc;
}

apr_status_t handle_dir_create(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	char *rpath;
	apr_finfo_t finfo;

	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "Command must have an argument.");
		return ret;
	}
	
	// check to see if p_sess->ftp_arg_str is an absolute or relative path
	rpath = get_abs_path(p_sess, p_sess->loop_pool);
	if(NULL == rpath)
	{
		lfd_cmdio_write(p_sess, FTP_FILEFAIL, "The server has encountered an error processing the path.");
		return APR_EINVAL;
	}

	ret = lkl_stat(&finfo, rpath, APR_FINFO_TYPE, p_sess->loop_pool);
	if (APR_SUCCESS == ret)
	{
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "%s Directory exists.", rpath);
		return ret;
	}
	ret = lkl_dir_make(rpath, APR_FPROT_OS_DEFAULT, p_sess->loop_pool);

	if(APR_SUCCESS !=ret)
	{
		lfd_log(LFD_ERROR, "lkl_dir_make failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "Cannot create directory %s.", rpath);
	}
	else
	{
		ret = lfd_cmdio_write(p_sess, FTP_MKDIROK, "%s Directory created.", rpath);
	}
	
	return ret;
}

apr_status_t handle_pwd(struct lfd_sess *p_sess)
{
	apr_status_t ret;

	ret = lfd_cmdio_write(p_sess, FTP_PWDOK, p_sess->cwd_path);
	return ret;
}

apr_status_t handle_cwd(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	char * path;
	apr_finfo_t finfo;
	size_t len;

	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "Command must have an argument.");
		return ret;
	}
	
	if (0 == strcmp(p_sess->ftp_arg_str, ".."))
	{
		return handle_cdup(p_sess);
	}
	
	path = get_abs_path(p_sess, p_sess->loop_pool);
	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_FILEFAIL, "The server has encountered an error processing the path");
		return APR_EINVAL;
	}
	
	
	//assure that the last character is a slash
	len = strlen(path);
	if('/' != path[len-1])
	{
		path = apr_pstrcat(p_sess->loop_pool, path, "/", NULL);
	}
	
	// verify that the path represents a valid directory
	ret = lkl_stat(&finfo, path, APR_FINFO_TYPE, p_sess->loop_pool);
	if((APR_SUCCESS != ret) || (APR_DIR != finfo.filetype))
	{
		lfd_log(LFD_ERROR, "lkl_stat failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "%s is not a directory.", path);
		return ret;
	}

	//allocate from a persistent pool
	p_sess->cwd_path = apr_pstrdup(p_sess->sess_pool, path);
	
	return lfd_cmdio_write(p_sess, FTP_CWDOK, "Directory changed to %s.", p_sess->cwd_path);
}

apr_status_t handle_cdup(struct lfd_sess *p_sess)
{
	apr_status_t ret;
	char * pos;
	size_t len;
	ret = APR_SUCCESS;
	
	// DOESN'T have an argument
	
	// changes the current directory with it's parent
	//TODO: this is Linux centric.
	
	len = strlen(p_sess->cwd_path);
	if(1 == len)
	{
		return lfd_cmdio_write(p_sess, FTP_ALLOOK, "Already in root directory.");
	}
	
	pos = p_sess->cwd_path + len - 2; //without '\0' and '/' from the end of the path
	//go backwards towards the root of the path
	while(pos != p_sess->cwd_path)
	{
		if ('/' == pos[0])
		{
			pos[1] = '\0';
			break;
		}
		pos --;
	}
	
	ret = lfd_cmdio_write(p_sess, FTP_ALLOOK, "Changed to directory %s.", p_sess->cwd_path);
	return ret;
}

apr_status_t handle_rnfr(struct lfd_sess *p_sess, char ** temp_path)
{
	apr_status_t ret;
	apr_finfo_t finfo;
	char * path;

	*temp_path = NULL;
	ret = APR_SUCCESS;
	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "Bad command argument.");
		return ret;
	}
	path = get_abs_path(p_sess, p_sess->sess_pool);

	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_FILEFAIL, "The server has encountered an error.");
		return APR_EINVAL;
	}

	ret = lkl_stat(&finfo, path, APR_FINFO_TYPE, p_sess->loop_pool);
	if((APR_SUCCESS != ret) || ((APR_REG != finfo.filetype) && (APR_DIR != finfo.filetype)))
	{
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "%s is not a directory or a file.", p_sess->ftp_arg_str);
		return ret;
	}
	*temp_path = path;
	ret = lfd_cmdio_write(p_sess, FTP_RNFROK, "Requested file action pending further information.");
	return ret;
}

apr_status_t handle_bad_rnto(struct lfd_sess *p_sess)
{
	apr_status_t ret;

	ret = lfd_cmdio_write(p_sess, FTP_BADPROT, "Bad sequence of commands.");
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
	path = get_abs_path(p_sess, p_sess->loop_pool);

	ret = lkl_file_rename(old_path, path, p_sess->loop_pool);

	if(APR_SUCCESS != ret)
	{
		lfd_log(LFD_ERROR, "lkl_file_rename failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot rename directory/file %s.", path);
	}
	else
		ret = lfd_cmdio_write(p_sess, FTP_RENAMEOK, "Directory/file was succesfully renamed.");
	return ret;
}

apr_status_t handle_type(struct lfd_sess *sess)
{
	//TODO: check if this is sufficient
	lfd_cmdio_write(sess, FTP_TYPEOK, "TYPE ok");
	return APR_SUCCESS;
}


/**
 *@brief parses a string of delimited numbers between 0 and 255 and stores them in the p_items buffer
 */
const unsigned char * lkl_str_parse_uchar_string_sep(char * input_str, char sep, unsigned char* p_items, unsigned int items)
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
	//there is no resource to free() except the allocated memory 
	// which will be freed when the pool gets destroyed
	p_sess->p_port_sockaddr = NULL;
}

static void pasv_cleanup(struct lfd_sess* p_sess)
{
	if (NULL != p_sess->pasv_listen_fd)
	{
		(void) apr_socket_close(p_sess->pasv_listen_fd);
		p_sess->pasv_listen_fd = NULL;
	}
}



// specify an alternate data port by use of the PORT command
apr_status_t handle_port(struct lfd_sess* p_sess)
{
	apr_status_t rc;
	apr_port_t the_port;
	unsigned char vals[6];
	const unsigned char* p_raw;
	struct apr_sockaddr_t * saddr;
	char * ip_str;
	pasv_cleanup(p_sess);
	port_cleanup(p_sess);

	// the received command argumet is a string of the form:
	// ip1,ip2,ip3,ip4,po1,po2 - representing the remote host's IP and port number.
	p_raw = lkl_str_parse_uchar_string_sep(p_sess->ftp_arg_str, ',', vals, sizeof(vals));
	if (p_raw == 0)
	{
		lfd_cmdio_write(p_sess, FTP_BADCMD, "Illegal PORT command.");
		return APR_EINVAL;
	}
	the_port = (vals[4] << 8) | vals[5];

	ip_str = apr_psprintf(p_sess->loop_pool, "%d.%d.%d.%d", vals[0], vals[1], vals[2], vals[3]);
	rc = apr_sockaddr_info_get(&saddr, ip_str, APR_UNSPEC, the_port, 0, p_sess->sess_pool);

	p_sess->p_port_sockaddr = saddr;
	if(APR_SUCCESS != rc)
	{
		lfd_cmdio_write(p_sess, FTP_BADCMD, "Illegal PORT command.");
		return rc;
	}
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

/**
 * @brief Get a connected socket to the client's listening data port.
 * Also binds the local end of the socket to a configurable (default 20) data port.
 */
static apr_status_t get_bound_and_connected_ftp_port_sock(struct lfd_sess* p_sess, apr_socket_t ** psock)
{
	apr_socket_t	* sock;
	apr_sockaddr_t	* saddr;
	apr_status_t	  rc;

	*psock = NULL;

	rc = apr_sockaddr_info_get(&saddr, NULL, APR_UNSPEC, lfd_config_data_port, 0, p_sess->loop_pool);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_socket_create(&sock, APR_INET, SOCK_STREAM, APR_PROTO_TCP, p_sess->loop_pool);
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

/**
 * @brief Get a connected socket to the client's listening data port.
 * Also binds the local end of the socket to a configurable (default 20) data port.
 */
static apr_status_t get_bound_and_connected_ftp_pasv_sock(struct lfd_sess* p_sess, apr_socket_t ** psock)
{
	apr_pollfd_t	poolfd;
	apr_int32_t	nsds;
	apr_status_t	rc = APR_SUCCESS;
	int 		nr_tries = lfd_config_pasv_max_accept_tries;
	
	poolfd.p         = p_sess->loop_pool;
	poolfd.desc_type = APR_POLL_SOCKET;
	poolfd.reqevents = APR_POLLIN|APR_POLLHUP|APR_POLLERR;
	poolfd.rtnevents = 0;
	poolfd.desc.s    = p_sess->pasv_listen_fd;
	
	while( nr_tries )
	{
		while( (-- nr_tries) && (APR_SUCCESS == rc) )
		{
			poolfd.rtnevents = 0;
			rc = apr_poll(&poolfd, 1, &nsds, apr_time_from_sec(lfd_config_pasv_listen_socket_timeout));
			if ( APR_STATUS_IS_TIMEUP(rc) || APR_STATUS_IS_EINTR(rc) )
			{
				rc = APR_SUCCESS;
				continue;
			}
			//if there is no error but, there isn't anyone trying to connect to us
			if ( (APR_SUCCESS == rc) && (0 == (APR_POLLIN & poolfd.rtnevents)) )
				continue;
			
			//on other errors or when someone wants to connect to us break
			break;
		}
		
		
		if ( (APR_SUCCESS == rc) && (APR_POLLIN & poolfd.rtnevents) )
		{
			//we exited and we have an outstanding socket to accept.
			rc = apr_socket_accept(psock, p_sess->pasv_listen_fd, p_sess->sess_pool);
			if( APR_SUCCESS == rc )
				break;
		}
		rc = APR_ENOSOCKET;
	}	
	//remove all information related to the passive listening socket.
	pasv_cleanup(p_sess);
	return rc;
}

static apr_status_t lfd_ftpdataio_get_pasv_fd(struct lfd_sess* p_sess, apr_socket_t ** psock)
{
	apr_status_t	  rc;
	apr_socket_t	* remote_fd;
	*psock = NULL;
	rc = get_bound_and_connected_ftp_pasv_sock(p_sess, &remote_fd);
	if (APR_SUCCESS != rc)
	{
		lfd_cmdio_write(p_sess, FTP_BADSENDCONN, "Failed to establish connection.");
		lfd_log(LFD_ERROR, "get_bound_and_connected_ftp_pasv_sock failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(p_sess, rc));
		return rc;
	}

	init_data_sock_params(p_sess, remote_fd);
	*psock = remote_fd;
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
		lfd_log(LFD_ERROR, "get_bound_and_connected_ftp_port_sock failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(p_sess, rc));
		return rc;
	}

	init_data_sock_params(p_sess, remote_fd);
	*psock = remote_fd;
	return APR_SUCCESS;
}

/**
 * @brief gets an apr_socket_t which is to be used for data transfer.
 * On success sends the p_status_msg message to the client, confirming a successful connection.
 */
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
		lfd_ftpdataio_get_pasv_fd(p_sess, &remote_fd);
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

/**
 * @brief Writes all the content on the buffer to the socket.
 * @param fd IN the socket on which to send the data
 * @param p_buf IN the location of the data
 * @param psize IN/OUT input: the buffer length, output: what amount get written on the socket.
*/
static apr_status_t lfd_socket_write_full(apr_socket_t * fd, const void* p_buf, apr_size_t * psize)
{
	apr_status_t	rc;
	apr_size_t	num_written;
	apr_size_t	size = *psize;

	*psize = 0;
	while (1)
	{
		num_written = size;
		rc = apr_socket_send(fd, (const char*)p_buf + *psize, &num_written);
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
static struct lfd_transfer_ret do_file_send_rwloop(struct lfd_sess* p_sess, lkl_file_t * file_fd, int is_ascii)
{
	char		* p_readbuf;
	char		* p_asciibuf;
	char		* p_writefrom_buf;
	apr_size_t 	  chunk_size = 4096;
	apr_socket_t	* sock = p_sess->data_conn->data_sock;
	struct lfd_transfer_ret ret_struct = { 0, 0 };


	p_readbuf = apr_palloc(p_sess->loop_pool, is_ascii?VSFTP_DATA_BUFSIZE*2:VSFTP_DATA_BUFSIZE);
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
			lfd_log(LFD_ERROR, "lkl_file_read failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(p_sess, rc));
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
			lfd_log(LFD_ERROR, "lfd_socket_write_full failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(p_sess, rc));
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
{
	char			* msg;
	apr_off_t		  offset = sess->restart_pos;
	apr_status_t		  rc;
	lkl_file_t 		* file;
	apr_socket_t		* remote_fd;
	apr_finfo_t		  finfo;
	struct lfd_transfer_ret   trans_ret;
	char 			* path;
	sess->restart_pos = 0;

	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Command must have an argument.");
		return rc;
	}

	if (!data_transfer_checks_ok(sess))
	{
		return APR_EINVAL;
	}
	if (sess->is_ascii && offset != 0)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "No support for resume of ASCII transfer.");
		return APR_EINVAL;
	}
	
	path = get_abs_path(sess, sess->loop_pool);

	rc = lkl_file_open(&file, path, APR_FOPEN_READ|APR_FOPEN_BINARY|APR_FOPEN_LARGEFILE|APR_FOPEN_SENDFILE_ENABLED, 0, sess->loop_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "Failed to open file.");
		return rc;
	}
	
	rc = lkl_stat(&finfo, path, APR_FINFO_TYPE|APR_FINFO_SIZE, sess->loop_pool);
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
   			path, " ( ", (unsigned int)finfo.size, " bytes).");


	remote_fd = get_remote_transfer_fd(sess, msg);
	if(NULL == remote_fd)
	{
		port_cleanup(sess);
		pasv_cleanup(sess);
		lkl_file_close(file);
		return APR_EINVAL;
	}

	trans_ret = do_file_send_rwloop(sess, file, sess->is_ascii);
	lfd_dispose_transfer_fd(sess);

	if (-1 == trans_ret.retval)
	{
		lfd_cmdio_write(sess, FTP_BADSENDFILE, "Failure reading local file.");
	}
	else if (-2 == trans_ret.retval)
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

apr_status_t handle_dele(struct lfd_sess * p_sess)
{
	apr_status_t ret;
	char * path;

	if(NULL == p_sess->ftp_arg_str)
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Bad command argument.");
		return ret;
	}

	path = get_abs_path(p_sess, p_sess->loop_pool);

	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}

	ret = lkl_file_remove(path, p_sess->loop_pool);
	if(APR_SUCCESS !=ret)
	{
		lfd_log(LFD_ERROR, "lkl_file_remove failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_FILEFAIL, "Requested action not taken. File unavailable.");
	}
	else
		ret = lfd_cmdio_write(p_sess,  FTP_DELEOK,"File deleted.");
	return ret;
}


static char* get_unique_filename(char * base_str, apr_pool_t * pool)
{
	// Use silly wu-ftpd algorithm for compatibility
	// same algorithm is employed by the vsftpd for the same reasons
	char		* dest_str = NULL;
	apr_finfo_t	  finfo;
	unsigned int	  suffix = 1;
	apr_status_t	  rc;

	//apr_stat insuccess should be seen as an "error while finding the file."
		// This is good, if no file is found then we should have an unique filename in "dest_str".

	rc = lkl_stat(&finfo, base_str, APR_FINFO_TYPE, pool);
	if(APR_SUCCESS != rc)
	{
		return base_str;
	}

	// Also this is moronically race prone: we should use kernel-based unique filename generators.
	while (1)
	{
		dest_str = apr_psprintf(pool, "%s.%u", base_str, suffix);
		rc = lkl_stat(&finfo, dest_str, APR_FINFO_TYPE, pool);
		if(APR_SUCCESS != rc)
		{
			return dest_str;
		}
		++suffix;
	}
}

static struct lfd_transfer_ret do_file_recv(struct lfd_sess* p_sess, lkl_file_t * file_fd, int is_ascii)
{
	apr_off_t num_to_write;
	struct lfd_transfer_ret ret_struct = { 0, 0 };
	apr_off_t chunk_size = VSFTP_DATA_BUFSIZE;
	int prev_cr = 0;

	/* Now that we do ASCII conversion properly, the plus one is to cater for
		* the fact we may need to stick a '\r' at the front of the buffer if the
		* last buffer fragment eneded in a '\r' and the current buffer fragment
		* does not start with a '\n'.
	*/
	char * p_recvbuf = apr_palloc(p_sess->loop_pool, VSFTP_DATA_BUFSIZE + 1);

	while (1)
	{
		apr_status_t rc;
		apr_size_t bytes_recvd = chunk_size;
		apr_size_t bytes_written;
		const char* p_writebuf = p_recvbuf + 1;

		rc = apr_socket_recv(p_sess->data_conn->data_sock, p_recvbuf + 1, &bytes_recvd);
		if((APR_SUCCESS != rc) && (APR_EOF != rc))
		{
			lfd_log(LFD_ERROR, "apr_socket_recv in do_file_recv failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(p_sess, rc));
			ret_struct.retval = -2;
			return ret_struct;
		}
		else if (0 == bytes_recvd && !prev_cr && (APR_EOF == rc))
		{
			/* Transfer done, nifty */
			return ret_struct;
		}
		num_to_write = bytes_recvd;
		ret_struct.transferred += num_to_write;
		if (is_ascii)
		{
			/* Handle ASCII conversion if we have to. Note that using the same
			* buffer for source and destination is safe, because the ASCII ->
			* binary transform only ever results in a smaller file.
			*/
			struct ascii_to_bin_ret ret = lfd_ascii_ascii_to_bin(p_recvbuf, num_to_write, prev_cr);
			num_to_write = ret.stored;
			prev_cr = ret.last_was_cr;
			p_writebuf = ret.p_buf;
		}
		rc = lkl_file_write_full(file_fd, p_writebuf, num_to_write, &bytes_written);
		if((APR_SUCCESS != rc) || (num_to_write != bytes_written))
		{
			lfd_log(LFD_ERROR, "lkl_file_write_full in do_file_recv failed with errorcode[%d] and error message[%s]. "
					"Bytes that had to be written [%d]."
					"Bytes that  got      written [%d].",
					rc, lfd_sess_strerror(p_sess, rc), num_to_write, bytes_written);
			ret_struct.retval = -1;
			return ret_struct;
		}
	}
}


static apr_status_t handle_upload_common(struct lfd_sess *sess, int is_append, int is_unique)
{
	char			* msg;
	char			* filename;
	apr_off_t		  offset = sess->restart_pos;
	apr_status_t		  rc;
	apr_int32_t		  flags;
	lkl_file_t 		* file;
	apr_socket_t		* remote_fd;
	struct lfd_transfer_ret   trans_ret;
	char 			* path;
	sess->restart_pos = 0;

	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Command must have an argument.");
		return rc;
	}

	if (!data_transfer_checks_ok(sess))
	{
		return APR_EINVAL;
	}
	path = get_abs_path(sess, sess->loop_pool);

	if (is_unique)
	{
		filename = get_unique_filename(path, sess->loop_pool);
	}
	else
	{
		filename = path;
	}

	flags = APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_APPEND | APR_FOPEN_BINARY;
	if (!is_append && (0 == offset))
	{
		flags |= APR_FOPEN_TRUNCATE;
	}
	rc = lkl_file_open(&file, filename, flags, 0, sess->loop_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lkl_file_open failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		lfd_cmdio_write(sess, FTP_UPLOADFAIL, "Could not create file.");
		return rc;
	}

	if (!is_append && 0 != offset)
	{
		/* XXX - warning, allows seek past end of file! Check for seek > size? */
		rc = lkl_file_seek(file, APR_SET, &offset);
		if(APR_SUCCESS != rc)
		{
			lfd_log(LFD_ERROR, "lkl_file_seek handle_uppload_common failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
			lfd_cmdio_write(sess, FTP_UPLOADFAIL, "Could not seek into file.");
			return rc;
		}
	}

	msg = "Ok to send data.";
	if (is_unique) //overwrite msg
	{
		msg = apr_pstrcat(sess->loop_pool, "FILE: ", filename, NULL);
		rc = lfd_cmdio_write(sess, FTP_DATACONN, msg);
		if(APR_SUCCESS != rc)
		{
			return rc;
		}
	}
	remote_fd = get_remote_transfer_fd(sess, msg);
	if(NULL == remote_fd)
	{
		return APR_EINVAL;
	}
	trans_ret = do_file_recv(sess, file, sess->is_ascii);
	lfd_dispose_transfer_fd(sess);
	sess->data_conn->transfer_size = trans_ret.transferred;
	/* XXX - handle failure, delete file? */
	if (-1 == trans_ret.retval)
	{
		lfd_cmdio_write(sess, FTP_BADSENDFILE, "Failure writing to local file.");
	}
	else if (-2 == trans_ret.retval)
	{
		lfd_cmdio_write(sess, FTP_BADSENDNET, "Failure reading network stream.");
	}
	else
		lfd_cmdio_write(sess, FTP_TRANSFEROK, "File receive OK.");
	check_abor(sess);
	port_cleanup(sess);
	pasv_cleanup(sess);
	lkl_file_close(file);
	return APR_SUCCESS;
}



apr_status_t handle_stor(struct lfd_sess *sess)
{
	return handle_upload_common(sess, 0, 0);
}


apr_status_t handle_appe(struct lfd_sess *sess)
{
	return handle_upload_common(sess, 1, 0);
}



apr_status_t handle_stou(struct lfd_sess *sess)
{
	return handle_upload_common(sess, 0, 1);
}


static apr_status_t list_dir(const char * directory, apr_pool_t * pool, char ** p_dest, int la)
{
	lkl_dir_t	* dir;
	apr_finfo_t	  finfo;
	apr_status_t	  apr_err;
	apr_int32_t	  flags = APR_FINFO_TYPE | APR_FINFO_NAME | APR_FINFO_SIZE;

	apr_err = lkl_dir_open(&dir, directory, pool);
	if(APR_SUCCESS != apr_err)
	{
		*p_dest = FTP_ENDCOMMAND_SEQUENCE;
		return apr_err;
	}
	else
	{
		*p_dest = "";
	}

	for(apr_err = lkl_dir_read(&finfo, flags, dir); apr_err == APR_SUCCESS;
		   apr_err = lkl_dir_read(&finfo, flags, dir))
	{
		char buffer[1024];

		if(finfo.filetype == APR_DIR && ((finfo.name[0] == '.' && finfo.name[1] == '\0')))
			continue;
		if (!la && finfo.name[1] == '.' && finfo.name[2] == '\0')
			continue;

		if (la) {
			char time_string[3+1+2+1+5+1];
			const char *time_fmt;
			apr_time_exp_t exp_time, exp_now;
			apr_size_t dummy;

			apr_time_exp_lt(&exp_time, finfo.mtime);
			apr_time_exp_lt(&exp_now, apr_time_now());
			if (exp_now.tm_year == exp_time.tm_year)
				time_fmt="%b %d %H:%M";
			else
				time_fmt="%b %d %Y";
			apr_strftime(time_string, &dummy, sizeof(time_string), time_fmt, &exp_time);

			snprintf(buffer, sizeof(buffer), "%c%c%c%c%c%c%c%c%c%c ftp ftp %d %s %s\n",
				 (finfo.filetype == APR_DIR)?'d':'-',
				 /* user permission */
				 (finfo.protection&00400)?'r':'-',
				 (finfo.protection&00200)?'w':'-',
				 (finfo.protection&00100)?'x':'-',
				 /* group permission */
				 (finfo.protection&00040)?'r':'-',
				 (finfo.protection&00020)?'w':'-',
				 (finfo.protection&00010)?'x':'-',
				 /* others permission */
				 (finfo.protection&00004)?'r':'-',
				 (finfo.protection&00002)?'w':'-',
				 (finfo.protection&00001)?'x':'-',
				 (int)finfo.size, time_string, finfo.name);
		} else
			snprintf(buffer, sizeof(buffer), "%s%s", finfo.name,
				 (finfo.filetype == APR_DIR)?"/":"");
		*p_dest = apr_pstrcat(pool, *p_dest, buffer, FTP_ENDCOMMAND_SEQUENCE, NULL);
	}
	lkl_dir_close(dir);
	return APR_SUCCESS;
}


apr_status_t handle_list(struct lfd_sess *p_sess)
{
	apr_socket_t 		* remote_fd;
	apr_status_t		  ret;
	apr_size_t		  file_list_len, bytes_written;
	char			* file_list;
	char			* path;
	int la=0;

	// if the arg was NULL, take the current directory
	if(NULL == p_sess->ftp_arg_str || strcmp(p_sess->ftp_arg_str, "-la") == 0)
	{
		if (p_sess->ftp_arg_str != NULL)
			la=1;
		path = apr_pstrdup(p_sess->loop_pool, p_sess->cwd_path);
	}
	else
	{
		path = get_abs_path(p_sess, p_sess->loop_pool);
	}
	
	if(NULL == path)
	{
		lfd_cmdio_write(p_sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	ret = list_dir(path, p_sess->loop_pool, &file_list, la);
	if(APR_SUCCESS != ret)
	{
		lfd_log(LFD_ERROR, "list_dir failed with errorcode[%d] and error message[%s]", ret, lfd_sess_strerror(p_sess, ret));
		ret = lfd_cmdio_write(p_sess, FTP_BADOPTS, "Cannot list files.");
		return ret;
	}
	file_list_len = bytes_written = strlen(file_list);

	remote_fd = get_remote_transfer_fd(p_sess, "Here comes the directory listing");
	if(NULL == remote_fd)
	{
		return APR_EINVAL;
	}
	ret = apr_socket_send(remote_fd, file_list, &bytes_written);
	if((APR_SUCCESS == ret) && (file_list_len == bytes_written))
	{
		ret = lfd_cmdio_write(p_sess, FTP_TRANSFEROK, "Directory sent OK");
	}
	else
	{
		ret = lfd_cmdio_write(p_sess, FTP_BADSENDNET, "Error sendind data");
	}
	lfd_dispose_transfer_fd(p_sess);


	return ret;
}


apr_status_t handle_site(struct lfd_sess* p_sess)
{
	apr_status_t rc = APR_SUCCESS;
	if (0 == apr_strnatcasecmp("CHMOD", p_sess->ftp_arg_str) )
	{
		lfd_log(LFD_ERROR, "SITE CHMOD NIMPL");
	}
	else if (0 == apr_strnatcasecmp("UMASK", p_sess->ftp_arg_str) )
	{
		lfd_log(LFD_ERROR, "SITE UMASK NIMPL");
	}
	else if (0 == apr_strnatcasecmp("HELP", p_sess->ftp_arg_str) )
	{
		rc = lfd_cmdio_write(p_sess, FTP_SITEHELP, "CHMOD UMASK HELP");
	}
	else
	{
		rc = lfd_cmdio_write(p_sess, FTP_BADCMD, "Unknown SITE command.");
	}
	return rc;
}
/*

static void
		handle_site_chmod(struct vsf_session* p_sess, struct mystr* p_arg_str)
{
	static struct mystr s_chmod_file_str;
	unsigned int perms;
	int retval;
	if (str_isempty(p_arg_str))
	{
		vsf_cmdio_write(p_sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
		return;
	}
	str_split_char(p_arg_str, &s_chmod_file_str, ' ');
	if (str_isempty(&s_chmod_file_str))
	{
		vsf_cmdio_write(p_sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
		return;
	}
	resolve_tilde(&s_chmod_file_str, p_sess);
	vsf_log_start_entry(p_sess, kVSFLogEntryChmod);
	str_copy(&p_sess->log_str, &s_chmod_file_str);
	prepend_path_to_filename(&p_sess->log_str);
	str_append_char(&p_sess->log_str, ' ');
	str_append_str(&p_sess->log_str, p_arg_str);
	if (!vsf_access_check_file(&s_chmod_file_str))
	{
		vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
		return;
	}
	// Don't worry - our chmod() implementation only allows 0 - 0777
	perms = str_octal_to_uint(p_arg_str);
	retval = str_chmod(&s_chmod_file_str, perms);
	if (vsf_sysutil_retval_is_error(retval))
	{
		vsf_cmdio_write(p_sess, FTP_FILEFAIL, "SITE CHMOD command failed.");
	}
	else
	{
		vsf_log_do_log(p_sess, 1);
		vsf_cmdio_write(p_sess, FTP_CHMODOK, "SITE CHMOD command ok.");
	}
}

static void
		handle_site_umask(struct vsf_session* p_sess, struct mystr* p_arg_str)
{
	static struct mystr s_umask_resp_str;
	if (str_isempty(p_arg_str))
	{
		// Empty arg => report current umask
		str_alloc_text(&s_umask_resp_str, "Your current UMASK is ");
		str_append_text(&s_umask_resp_str,
				 vsf_sysutil_uint_to_octal(vsf_sysutil_get_umask()));
	}
	else
	{
		// Set current umask
		unsigned int new_umask = str_octal_to_uint(p_arg_str);
		vsf_sysutil_set_umask(new_umask);
		str_alloc_text(&s_umask_resp_str, "UMASK set to ");
		str_append_text(&s_umask_resp_str,
				 vsf_sysutil_uint_to_octal(vsf_sysutil_get_umask()));
	}
	vsf_cmdio_write_str(p_sess, FTP_UMASKOK, &s_umask_resp_str);
}
*/

