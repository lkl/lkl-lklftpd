// implement the main() + config, pass control to listener
#include <stdlib.h>
#include <stdio.h>

#include <apr.h>
#include <apr_general.h>
#include <apr_errno.h>

#include "utils.h"


int main(int argc, char const *const * argv, char const *const * engv)
{
	apr_status_t rc;
	rc = apr_app_initialize(&argc, &argv, &engv);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "apr_initialize_app failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
	}
	atexit(apr_terminate);
	printf("success\n");
	return 0;
}

