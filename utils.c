// utilities that may be needed in more than one place
#include "utils.h"
#include <stdarg.h>
#include <stdio.h>

#define STR_ERROR_MAX_LEN 300


char* lfd_apr_strerror_thunsafe(apr_status_t rc)
{
	static char static_error_buff[STR_ERROR_MAX_LEN];
	return apr_strerror(rc, static_error_buff, STR_ERROR_MAX_LEN);
}


void lfd_log(enum err_levels lvl, char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	//for now just ignore the error level
	vprintf(fmt, ap);
	va_end(ap);
}

