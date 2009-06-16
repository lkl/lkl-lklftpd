// implement the listener and thread dispatcher
#include <string.h>

#include <apr_atomic.h>
#include <apr_thread_proc.h>
#include <apr_poll.h>

#include "listen.h"
#include "config.h"
#include "utils.h"
#include "worker.h"


extern volatile apr_uint32_t ftp_must_exit;
static void create_listen_socket(apr_socket_t**plisten_sock, apr_pool_t*mp)
{
	apr_status_t	rc;
	apr_socket_t	*listen_sock;
	apr_sockaddr_t	*saddr;

	*plisten_sock = NULL;

	/*
	 * Get a list of addresses for this HOST:PORT
	 */
	rc = apr_sockaddr_info_get(&saddr, lfd_config_listen_host, APR_UNSPEC, lfd_config_listen_port, 0, mp);
	if (APR_SUCCESS != rc)
	{
		lfd_log_apr_err(rc, "apr_sockaddr_info_get failed");
		return;
	}
	if (NULL == saddr)
	{
		lfd_log(LFD_ERROR, "apr_sockaddr_info_get returned NO entries");
		return;
	}

	/*
	 * iterate through all address info descriptors for this host
	 * until you find one that you can bind on, or until there are
	 * no more addresses to try.
	 */
	do {
		// create a socket
		rc = apr_socket_create(&listen_sock, saddr->family, SOCK_STREAM, APR_PROTO_TCP, mp);
		if(APR_SUCCESS != rc)
		{
			lfd_log_apr_err(rc, "apr_socket_create failed");
			listen_sock = NULL;
			goto try_next_addr;
		}

		// bind it - we don't really care if REUSEADDR fails or not
		apr_socket_opt_set(listen_sock, APR_SO_REUSEADDR, 1);
		rc = apr_socket_bind(listen_sock, saddr);
		if(APR_SUCCESS != rc)
		{
			lfd_log_apr_err(rc, "apr_socket_bind failed");
			apr_socket_close(listen_sock);
			listen_sock = NULL;
			goto try_next_addr;
		}

		// and listen on it
		rc = apr_socket_listen(listen_sock, lfd_config_backlog);
		if(APR_SUCCESS != rc) {
			lfd_log_apr_err(rc, "apr_socket_listen failed");
			apr_socket_close(listen_sock);
			listen_sock = NULL;
			goto try_next_addr;
		}

try_next_addr:
		saddr = saddr->next;
	} while((NULL == listen_sock) && (NULL != saddr));

	/* if one iteration finished succcessfully listen_sock will be non-NULL.
	 * if none finish successfully, listen_sock will be NULL
	 */
	*plisten_sock = listen_sock;
}

void create_pollfd_from_socket(apr_pollfd_t * pfd, apr_socket_t * sock, apr_pool_t * mp)
{
	pfd->p = mp;
	pfd->desc_type = APR_POLL_SOCKET;
	pfd->reqevents = APR_POLLIN|APR_POLLHUP|APR_POLLERR;
	pfd->rtnevents = 0;
	pfd->desc.s = sock;
	pfd->client_data = NULL;
}

void lfd_listen(apr_pool_t * mp)
{
	apr_pool_t		* thd_pool = NULL;
	apr_socket_t		* listen_sock;
	apr_socket_t		* client_sock;
	apr_thread_t *thd;
	apr_threadattr_t	* thattr;
	apr_pollfd_t		  pfd;
	apr_interval_time_t	  timeout = lfd_config_max_acceptloop_timeout;
	apr_int32_t		  nsds;
	apr_status_t		  rc;

	create_listen_socket(&listen_sock, mp);
	if(NULL == listen_sock)
	{
		lfd_log(LFD_ERROR, "lfd_listen: could not create listen socket");
		return;
	}

	rc = apr_threadattr_create(&thattr, mp);
	if(APR_SUCCESS != rc)
	{
		lfd_log_apr_err(rc, "apr_threadattr_create failed");
		return;
	}
	while(1)
	{
		//###: Should I allocate the pool as a subpool of the root pool?
		//What is the amount allocated per pool and is it freed when the child pool is destroyed?
		//rc = apr_pool_create(&thd_pool, mp);
		if(NULL == thd_pool)
		{
			rc = apr_pool_create(&thd_pool, NULL);
			if(APR_SUCCESS != rc)
			{
				lfd_log_apr_err(rc, "apr_pool_create of thd_pool failed");
				continue;
			}
		}

		create_pollfd_from_socket(&pfd, listen_sock, mp);

		rc = apr_poll(&pfd, 1, &nsds, timeout);
		if((APR_SUCCESS != rc) && (!APR_STATUS_IS_TIMEUP(rc)) && (!APR_STATUS_IS_EINTR(rc)))
		{
			//break - an unrecoverable error occured
			lfd_log_apr_err(rc, "apr_poll failed");
			break;
		}

		if(apr_atomic_read32(&ftp_must_exit))
		{
			//if the flag says we must exit, we comply, so bye bye!
			return;
		}
		if(APR_STATUS_IS_TIMEUP(rc) || APR_STATUS_IS_EINTR(rc) || (APR_POLLIN != pfd.rtnevents))
		{
			continue;
		}


		rc = apr_socket_accept(&client_sock, listen_sock, thd_pool);
		if(APR_SUCCESS != rc)
		{
			//###: For which errorcode must we break out of the loop?
			lfd_log_apr_err(rc, "apr_socket_accept failed");
			if(APR_STATUS_IS_EAGAIN(rc))
			{
				lfd_log(LFD_ERROR, "lfd_listen: APR_STATUS_IS_EAGAIN");
			}
			continue;
		}
		rc = apr_thread_create(&thd, thattr, &lfd_worker_protocol_main, (void*)client_sock, thd_pool);
		if(APR_SUCCESS != rc)
		{
			lfd_log_apr_err(rc, "apr_thread_create failed");
			apr_socket_close(client_sock);
			continue;
		}
		thd_pool = NULL;
	}
}

void lfd_connect(apr_socket_t ** pconnect_sock,apr_sockaddr_t * saddr, apr_pool_t * mp)
{
	apr_status_t rc;
	apr_socket_t * connect_sock;

	*pconnect_sock = NULL;
	rc = apr_socket_create(&connect_sock, saddr->family, SOCK_STREAM, APR_PROTO_TCP, mp);
	if(APR_SUCCESS != rc)
	{
		lfd_log_apr_err(rc, "apr_socket_create failed");
		return;
	}
	rc = apr_socket_connect(connect_sock, saddr);
	if(APR_SUCCESS != rc)
	{
		lfd_log_err(LFD_ERROR, "apr_socket_connect failed");
		apr_socket_close(connect_sock);
		return;
	}
	*pconnect_sock = connect_sock;
}

