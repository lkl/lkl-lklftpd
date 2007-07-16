// main FTP protocol loop handling
#include <apr_network_io.h>

#include "worker.h"
void * lfd_worker_protocol_main(apr_thread_t * thd, void* param)
{
	//apr_pool_t	* mp;
	//apr_status_t	rc;
	//apr_socket_t 	* sock = (apr_socket_t*) param;

	printf("hello\n");
	return NULL;
}

