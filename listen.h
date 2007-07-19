#ifndef LKLFTPD_LISTEN_H__
#define LKLFTPD_LISTEN_H__
#include <apr_pools.h>
#include <apr_network_io.h>

void lfd_listen(apr_pool_t * mp);
void lfd_connect(apr_socket_t ** pconnect_sock, apr_sockaddr_t * saddr, apr_pool_t * mp);

#endif//LKLFTPD_LISTEN_H__
