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

void sockaddr_vars_set(apr_sockaddr_t *addr, apr_port_t port, const unsigned char * ipv4_bytes_in_network_order)
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
static apr_status_t handle_port(struct lfd_sess* p_sess)
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
		else if(lfd_cmdio_cmd_equals(sess, "TYPE"))
		{
			rc = handle_type(sess);
		}
		else if(lfd_cmdio_cmd_equals(sess, "RETR"))
		{
			rc = handle_retr(sess);
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
