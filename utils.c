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

/**
 * @brief translate all \r\n into plain \n. A plain \r not followed by \n must
 * not be removed.
 */
struct ascii_to_bin_ret lfd_ascii_ascii_to_bin(char* p_buf, apr_size_t in_len, int prev_cr)
{
	struct ascii_to_bin_ret ret;
	apr_size_t indexx = 0;
	apr_size_t written = 0;
	char* p_out = p_buf + 1;
	ret.last_was_cr = 0;
	if (prev_cr && (!in_len || p_out[0] != '\n'))
	{
		p_buf[0] = '\r';
		ret.p_buf = p_buf;
		written++;
	}
	else
	{
		ret.p_buf = p_out;
	}
	while (indexx < in_len)
	{
		char the_char = p_buf[indexx + 1];
		if (the_char != '\r')
		{
			*p_out++ = the_char;
			written++;
		}
		else if (indexx == in_len - 1)
		{
			ret.last_was_cr = 1;
		}
		else if (p_buf[indexx + 2] != '\n')
		{
			*p_out++ = the_char;
			written++;
		}
		indexx++;
	}
	ret.stored = written;
	return ret;
}

