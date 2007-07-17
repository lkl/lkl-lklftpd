#include "config.h"

int lfd_config_listen_port = 9090;
const char * lfd_config_listen_host = "localhost";
const char * lfd_config_banner_string = "This is the banner string";
int lfd_config_backlog = 5;
apr_status_t lfd_config(apr_pool_t * mp)
{
	return APR_SUCCESS;
}

