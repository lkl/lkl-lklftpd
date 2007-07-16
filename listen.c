// implement the listener and thread dispatcher
#include <string.h>
#include <apr_network_io.h>
#include "listen.h"
#include "config.h"
#include "utils.h"

static void create_listen_socket(apr_socket_t**plisten_sock, apr_pool_t*mp)
{
	apr_status_t rc;
	apr_socket_t * listen_sock;
	apr_sockaddr_t * saddr;

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
			rc = apr_socket_bind(listen_sock, saddr);
			if(APR_SUCCESS != rc)
			{
				lfd_log(LFD_ERROR, "apr_socket_bind failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
				apr_socket_close(listen_sock); listen_sock = NULL;
			}
			else
			{
				break;
				//###:does a socket support multiple bind()s?
			}
		}
		saddr = saddr->next;
	}while(NULL != saddr);
	*plisten_sock = listen_sock;
}

void lfd_listen(apr_pool_t * mp)
{
	apr_status_t rc;
	apr_socket_t * listen_sock;
	apr_socket_t * client_sock;
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
	while(1)
	{
		rc = apr_socket_accept(&client_sock, listen_sock, mp);
		if(APR_SUCCESS != rc)
		{
			//###: For which errorcode must we break out of the loop?
			lfd_log(LFD_ERROR, "apr_socket_accept failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
			continue;
		}

		{
			#define CONNECTION_DBG_STR "THIS IS A DEBUG MESSAGE! IF YOU SEE THIS CONNECTION SUCCEEDED!"
			apr_size_t len = strlen(CONNECTION_DBG_STR);
			apr_socket_send(client_sock, CONNECTION_DBG_STR, &len);
		}
	}
}

