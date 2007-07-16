#ifndef LKLFTPD_CONFIG_H__
#define LKLFTPD_CONFIG_H__
#include <apr_errno.h>
#include <apr_pools.h>


extern int lfd_config_listen_port;
extern const char * lfd_config_listen_host;
extern int lfd_config_backlog;
apr_status_t lfd_config(apr_pool_t * mp);
#endif//LKLFTPD_CONFIG_H__

