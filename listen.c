// implement the listener and thread dispatcher
#include <string.h>
#include <apr_thread_proc.h>

#include "listen.h"
#include "config.h"
#include "utils.h"
#include "worker.h"


static void create_listen_socket(apr_socket_t**plisten_sock, apr_pool_t*mp)
{
	apr_status_t	rc;
	apr_socket_t	*listen_sock;
	apr_sockaddr_t	*saddr;

	*plisten_sock = NULL;

	rc = apr_sockaddr_info_get(&saddr, lfd_config_listen_host, APR_UNSPEC, lfd_config_listen_port, 0, mp);
	if (APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "apr_sockaddr_info_get failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		return;
	}
	if (NULL == saddr)
	{
		lfd_log(LFD_ERROR, "apr_sockaddr_info_get returned NO entries");
		return;
	}
	do
	{
		rc = apr_socket_create(&listen_sock, saddr->family, SOCK_STREAM, APR_PROTO_TCP, mp);
		if(APR_SUCCESS != rc)
		{
			lfd_log(LFD_ERROR, "apr_socket_create failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		}
		else
		{
			apr_socket_opt_set(listen_sock, APR_SO_REUSEADDR, 1);
			rc = apr_socket_bind(listen_sock, saddr);
			if(APR_SUCCESS != rc)
			{
				lfd_log(LFD_ERROR, "apr_socket_bind failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
				apr_socket_close(listen_sock); listen_sock = NULL;
			}
			else
			{
				break;
			}
		}
		saddr = saddr->next;
	}while(NULL != saddr);
	*plisten_sock = listen_sock;
}

void lfd_listen(apr_pool_t * mp)
{
	apr_status_t		rc;
	apr_pool_t		* thd_pool;
	apr_socket_t		* listen_sock;
	apr_socket_t		* client_sock;
	apr_thread_t 		* thd;
	apr_threadattr_t	* thattr;

	create_listen_socket(&listen_sock, mp);
	if(NULL == listen_sock)
	{
		lfd_log(LFD_ERROR, "could not create listen socket");
		return;
	}
	rc = apr_socket_listen(listen_sock, lfd_config_backlog);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "apr_socket_listen failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		return;
	}
	rc = apr_threadattr_create(&thattr, mp);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "apr_threadattr_create failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		return;
	}
	while(1)
	{
		//###: Should I allocate the pool as a subpool of the root pool?
		//What is the amount allocated per pool and is it freed when the child pool is destroyed?
		//rc = apr_pool_create(&thd_pool, mp);
		rc = apr_pool_create(&thd_pool, NULL);
		if(APR_SUCCESS != rc)
		{
			lfd_log(LFD_ERROR, "apr_pool_create of thd_pool failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
			continue;
		}

		rc = apr_socket_accept(&client_sock, listen_sock, thd_pool);
		if(APR_SUCCESS != rc)
		{
			//###: For which errorcode must we break out of the loop?
			lfd_log(LFD_ERROR, "apr_socket_accept failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
			//maybe cache the successfully allocated pool and reuse it next iteration?
			apr_pool_destroy(thd_pool);
			continue;
		}

		rc = apr_thread_create(&thd, thattr, &lfd_worker_protocol_main, (void*)client_sock, thd_pool);
		if(APR_SUCCESS != rc)
		{
			lfd_log(LFD_ERROR, "apr_thread_create failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
			apr_socket_close(client_sock);
			apr_pool_destroy(thd_pool);
			continue;
		}
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
			lfd_log(LFD_ERROR, "apr_socket_create failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
			return;
		}
	rc = apr_socket_connect(connect_sock, saddr);
	if(APR_SUCCESS != rc)
		{
			lfd_log(LFD_ERROR, "apr_socket_connect failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
			apr_socket_close(connect_sock);
			return;
		}
	*pconnect_sock = connect_sock;
}

