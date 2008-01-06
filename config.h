#ifndef LKLFTPD_CONFIG_H__
#define LKLFTPD_CONFIG_H__
#include <apr_errno.h>
#include <apr_pools.h>


extern int lfd_config_max_acceptloop_timeout;
extern int lfd_config_max_login_attempts;
extern int lfd_config_listen_port;
extern int lfd_config_data_port;
extern int lfd_config_min_pasv_port;
extern int lfd_config_max_pasv_port;
extern const char * lfd_config_listen_host;
extern const char * lfd_config_banner_string;
extern int lfd_config_backlog;
extern int lfd_config_max_sockets_to_try_to_bind;
extern int lfd_config_pasv_max_accept_tries;
extern int lfd_config_pasv_listen_socket_timeout;
extern int lfd_config_debug;
apr_status_t lfd_config(apr_pool_t * mp);
#endif//LKLFTPD_CONFIG_H__


