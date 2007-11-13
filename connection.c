#include <apr.h>
#include <apr_strings.h>
#include <apr_poll.h>

#include "connection.h"
#include "config.h"
#include "utils.h"
#include "cmdio.h"
#include "ftpcodes.h"


static void port_cleanup(struct lfd_sess* sess)
{
	//there is no resource to free() except the allocated memory 
	// which will be freed when the pool gets destroyed
	sess->p_port_sockaddr = NULL;
}

static void pasv_cleanup(struct lfd_sess* sess)
{
	if (NULL != sess->pasv_listen_fd)
	{
		(void) apr_socket_close(sess->pasv_listen_fd);
		sess->pasv_listen_fd = NULL;
	}
}

void lfd_communication_cleanup(struct lfd_sess * sess)
{
	port_cleanup(sess);
	pasv_cleanup(sess);
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

/**
 *@brief parses a string of delimited numbers between 0 and 255 and stores them in the p_items buffer
 */
const unsigned char * lfd_str_parse_uchar_string_sep(char * input_str, char sep, unsigned char* p_items, unsigned int items)
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


apr_status_t handle_pasv(struct lfd_sess * sess)
{
	apr_status_t	rc;
	apr_sockaddr_t	*saddr;
	apr_port_t	port;
	unsigned char	vals[6];
	char		*addr;
	
	lfd_communication_cleanup(sess);

	rc = bind_to_random_passive_port(sess, &saddr, &port);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "handle_pasv: bind_to_random_passive_port failed with [%d] and message[%s]", rc, lfd_sess_strerror(sess, rc));
		return rc;
	}

	rc = apr_socket_listen(sess->pasv_listen_fd, 1); //backlog of one!
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "handle_pasv: apr_socket_listen failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		return rc;
	}

	//get the IP represented as a string of characters
	rc = apr_sockaddr_ip_get(&addr, saddr);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "handle_pasv: apr_sockaddr_ip_get failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		return rc;
	}

	// fill the first 4 elemnts of vals with the numbers from the IP address
	lfd_str_parse_uchar_string_sep(addr, '.', vals, 4);
	
	// append the decomposed port number
	vals[4] = (unsigned char) (port >> 8);
	vals[5] = (unsigned char) (port & 0xFF);

	rc = lfd_cmdio_write(sess, FTP_PASVOK, "Entering Passive Mode. %d,%d,%d,%d,%d,%d\n", 
				(int)vals[0], (int)vals[1], (int)vals[2], 
				(int)vals[3], (int)vals[4], (int)vals[5]);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "handle_pasv: lfd_cmdio_write failed with [%d] and message[%s]", rc, lfd_sess_strerror(sess, rc));
	}
	return rc;
}

// specify an alternate data port by use of the PORT command
apr_status_t handle_port(struct lfd_sess* sess)
{
	apr_status_t rc;
	apr_port_t the_port;
	unsigned char vals[6];
	const unsigned char* p_raw;
	struct apr_sockaddr_t * saddr;
	char * ip_str;
	
	lfd_communication_cleanup(sess);


	// the received command argumet is a string of the form:
	// ip1,ip2,ip3,ip4,po1,po2 - representing the remote host's IP and port number.
	p_raw = lfd_str_parse_uchar_string_sep(sess->ftp_arg_str, ',', vals, sizeof(vals));
	if (p_raw == 0)
	{
		lfd_cmdio_write(sess, FTP_BADCMD, "Illegal PORT command.");
		return APR_EINVAL;
	}
	the_port = (vals[4] << 8) | vals[5];

	ip_str = apr_psprintf(sess->loop_pool, "%d.%d.%d.%d", vals[0], vals[1], vals[2], vals[3]);
	rc = apr_sockaddr_info_get(&saddr, ip_str, APR_UNSPEC, the_port, 0, sess->sess_pool);

	sess->p_port_sockaddr = saddr;
	if(APR_SUCCESS != rc)
	{
		lfd_cmdio_write(sess, FTP_BADCMD, "Illegal PORT command.");
		return rc;
	}
	lfd_cmdio_write(sess, FTP_PORTOK, "PORT command successful. Consider using PASV.");

	return APR_SUCCESS;
}


static int port_active(struct lfd_sess* sess);
static int pasv_active(struct lfd_sess* sess);

static int port_active(struct lfd_sess* sess)
{
	int rc = 0;
	if (NULL != sess->p_port_sockaddr)
	{
		rc = 1;
		if (pasv_active(sess))
		{
			bug("port and pasv both active");
		}
	}
	return rc;
}

static int pasv_active(struct lfd_sess* sess)
{
	int rc = 0;
	if (NULL != sess->pasv_listen_fd)
	{
		rc = 1;
		if (port_active(sess))
		{
			bug("pasv and port both active");
		}
	}
	return rc;
}


int data_transfer_checks_ok(struct lfd_sess* sess)
{
	if (!pasv_active(sess) && !port_active(sess))
	{
		lfd_cmdio_write(sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
		return 0;
	}
	return 1;
}


static void init_data_sock_params(struct lfd_sess* sess, apr_socket_t * sock_fd)
{
	if (NULL != sess->data_conn->data_sock)
	{
		bug("data descriptor still present in init_data_sock_params");
	}
	sess->data_conn->data_sock = sock_fd;
	sess->data_conn->in_progress = 0;
	apr_socket_opt_set(sock_fd, APR_SO_KEEPALIVE, 1);

	//Set up lingering, so that we wait for all data to transfer, and report
	// more accurate transfer rates.
	apr_socket_opt_set(sock_fd, APR_SO_LINGER, 1);
}

/**
 * @brief Get a connected socket to the client's listening data port.
 * Also binds the local end of the socket to a configurable (default 20) data port.
 */
static apr_status_t get_bound_and_connected_ftp_port_sock(struct lfd_sess* sess, apr_socket_t ** psock)
{
	apr_socket_t	* sock;
	apr_sockaddr_t	* saddr;
	apr_status_t	  rc;

	*psock = NULL;

	rc = apr_sockaddr_info_get(&saddr, NULL, APR_UNSPEC, lfd_config_data_port, 0, sess->loop_pool);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_socket_create(&sock, APR_INET, SOCK_STREAM, APR_PROTO_TCP, sess->loop_pool);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_socket_opt_set(sock, APR_SO_REUSEADDR, 1);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_socket_bind(sock, saddr);
	if(APR_SUCCESS != rc)
		return rc;
	rc = apr_socket_connect (sock, sess->p_port_sockaddr);
	if(APR_SUCCESS != rc)
		return rc;

	*psock = sock;
	return APR_SUCCESS;
}

/**
 * @brief Get a connected socket to the client's listening data port.
 * Also binds the local end of the socket to a configurable (default 20) data port.
 */
static apr_status_t get_bound_and_connected_ftp_pasv_sock(struct lfd_sess* sess, apr_socket_t ** psock)
{
	apr_pollfd_t	poolfd;
	apr_int32_t	nsds;
	apr_status_t	rc = APR_SUCCESS;
	int 		nr_tries = lfd_config_pasv_max_accept_tries;
	
	poolfd.p         = sess->loop_pool;
	poolfd.desc_type = APR_POLL_SOCKET;
	poolfd.reqevents = APR_POLLIN|APR_POLLHUP|APR_POLLERR;
	poolfd.rtnevents = 0;
	poolfd.desc.s    = sess->pasv_listen_fd;
	
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
			rc = apr_socket_accept(psock, sess->pasv_listen_fd, sess->sess_pool);
			if( APR_SUCCESS == rc )
				break;
		}
		rc = APR_ENOSOCKET;
	}	
	//remove all information related to the passive listening socket.
	pasv_cleanup(sess);
	return rc;
}

static apr_status_t lfd_ftpdataio_get_pasv_fd(struct lfd_sess* sess, apr_socket_t ** psock)
{
	apr_status_t	  rc;
	apr_socket_t	* remote_fd;
	*psock = NULL;
	rc = get_bound_and_connected_ftp_pasv_sock(sess, &remote_fd);
	if (APR_SUCCESS != rc)
	{
		lfd_cmdio_write(sess, FTP_BADSENDCONN, "Failed to establish connection.");
		lfd_log(LFD_ERROR, "get_bound_and_connected_ftp_pasv_sock failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		return rc;
	}

	init_data_sock_params(sess, remote_fd);
	*psock = remote_fd;
	return APR_SUCCESS;
}

static apr_status_t ftpdataio_get_port_fd(struct lfd_sess* sess, apr_socket_t ** psock)
{
	apr_status_t	  rc;
	apr_socket_t	* remote_fd;
	*psock = NULL;
	rc = get_bound_and_connected_ftp_port_sock(sess, &remote_fd);
	if (APR_SUCCESS != rc)
	{
		lfd_cmdio_write(sess, FTP_BADSENDCONN, "Failed to establish connection.");
		lfd_log(LFD_ERROR, "get_bound_and_connected_ftp_port_sock failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		return rc;
	}

	init_data_sock_params(sess, remote_fd);
	*psock = remote_fd;
	return APR_SUCCESS;
}

/**
 * @brief gets an apr_socket_t which is to be used for data transfer.
 * On success sends the p_status_msg message to the client, confirming a successful connection.
 */
apr_socket_t* lfd_get_remote_transfer_fd(struct lfd_sess* sess, const char* p_status_msg)
{
	apr_socket_t	* remote_fd;

	if (!pasv_active(sess) && !port_active(sess))
	{
		bug("neither PORT nor PASV active in get_remote_transfer_fd");
	}
	sess->data_conn->abor_received = 0;
	if (pasv_active(sess))
	{
		lfd_ftpdataio_get_pasv_fd(sess, &remote_fd);
	}
	else
	{
		ftpdataio_get_port_fd(sess, & remote_fd);
	}
	if (NULL == remote_fd)
	{
		return NULL;
	}
	lfd_cmdio_write(sess, FTP_DATACONN, p_status_msg);
	return remote_fd;
}


void lfd_dispose_transfer_fd(struct lfd_sess* sess)
{

	apr_socket_opt_set(sess->data_conn->data_sock, APR_SO_LINGER, 0);
	apr_socket_close(sess->data_conn->data_sock);
	sess->data_conn->data_sock = NULL;
}


