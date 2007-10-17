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
int lfd_config_max_sockets_to_try_to_bind = 10; //PASSIVE mode needs to bind 
		//to a nonreserved port. This is the limit of number of tries
int lfd_config_pasv_max_accept_tries = 10; //we try to accept passive connections.
		//a port scanner might connect to our new listening socket.
		//Discard unwanted connections, but fail if we cannot get a valid
		// connection in lfd_config_pasv_max_accept_tries tries.
int lfd_config_pasv_listen_socket_timeout = 10 /*seconds*/; //because the client might have 
		//died since he requested a passive transfer mode, 
		//we shouldn't block forever, and thus we timeout after a while
apr_status_t lfd_config(apr_pool_t * mp)
{
	return APR_SUCCESS;
}

