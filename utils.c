// utilities that may be needed in more than one place
#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


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
	printf("\n");
	va_end(ap);
}
void bug(const char* p_text)
{
	printf("%s\n", p_text);
	exit(1);
}


apr_size_t lfd_ascii_bin_to_ascii(const char* p_in, char* p_out, apr_size_t in_len)
{
  /* Task: translate all \n into \r\n. Note that \r\n becomes \r\r\n. That's
	* what wu-ftpd does, and it's easier :-)
  */
	apr_size_t indexx = 0;
	apr_size_t written = 0;
	while (indexx < in_len)
	{
		char the_char = p_in[indexx];
		if (the_char == '\n')
		{
			*p_out++ = '\r';
			written++;
		}
		*p_out++ = the_char;
		written++;
		indexx++;
	}
	return written;
}
