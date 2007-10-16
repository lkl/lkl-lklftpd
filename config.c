#include <apr_time.h>
#include "config.h"


int lfd_config_max_acceptloop_timeout = APR_USEC_PER_SEC >> 1;
int lfd_config_max_login_attempts = 3;
int lfd_config_listen_port = 21;
int lfd_config_data_port = 20;
int lfd_config_connect_port = 20;
int lfd_config_min_pasv_port = 1024;
int lfd_config_max_pasv_port = 65535;
const char * lfd_config_listen_host = "localhost";
const char * lfd_config_banner_string = "Welcome to LklFtp Server.";
int lfd_config_backlog = 5;
apr_status_t lfd_config(apr_pool_t * mp)
{
	return APR_SUCCESS;
}

