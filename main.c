// implement the main() + config, pass control to listener
#include <stdlib.h>
#include <stdio.h>

#include <apr.h>
#include <apr_general.h>
#include <apr_errno.h>
#include <apr_atomic.h>

#include "utils.h"
#include "config.h"
#include "listen.h"

int main(int argc, char const *const * argv, char const *const * engv)
{
	apr_status_t rc;
	apr_pool_t * root_pool;
	rc = apr_app_initialize(&argc, &argv, &engv);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "apr_initialize_app failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		return 1;
	}
	atexit(apr_terminate);
	apr_pool_create(&root_pool, NULL);

	rc = lfd_config(root_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "Config file wrong. Exiting");
		return 2;
	}

	apr_atomic_init(root_pool);
	lfd_listen(root_pool);

	apr_pool_destroy(root_pool);
	return 0;
}

