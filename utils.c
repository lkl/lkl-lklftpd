// utilities that may be needed in more than one place
#include <apr_strings.h>
#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <apr_errno.h>
#include <string.h>



void lfd_log(enum err_levels lvl, char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	//for now just ignore the error level
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

static const char * err_source_str(enum err_source_t errsrc)
{
	switch (errsrc)
	{
	case ERR_SOURCE_APR:
		return "APR";
	case ERR_SOURCE_LINUX:
		return "LINUX";
	default:
		return "<unknown-err-src>";
	}
}

static const char * err_source_strerror(enum err_source_t errsrc, int rc, char * error_buff, size_t size)
{
	char * error_msg = NULL;
	switch(errsrc)
	{
	case ERR_SOURCE_APR:
		error_msg = apr_strerror(rc, error_buff, size);
		break;
	case ERR_SOURCE_LINUX:
		// the kernel internally uses "-errno" vals; we must negate it once more
		error_msg = strerror_r(-rc, error_buff, size);
		/*
		if (ret != 0)
		{
			printf("{{nested error while printing errmsg: strerror_r returned %d %s for for errnum = %d}}\n", ret, strerror(ret), -rc);
		}
		error_msg = error_buff;
		*/
		break;
	default:
		error_msg = "<cannot-determine-err-msg>";
		break;
	}
	return error_msg;
}

void lfd_log_err_impl(enum err_source_t errsrc, const char * file, int line, const char * func, int rc, char * fmt, ...)
{
	char error_buff[STR_ERROR_MAX_LEN];
	va_list ap;

	memset(error_buff, 0, sizeof(error_buff));
	printf("[ERROR][%s] %s:%d %s() -- ", err_source_str(errsrc), file, line, func);

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");


	printf("\t rc = %d, strerr = [%s]\n", rc, err_source_strerror(errsrc, rc, error_buff, STR_ERROR_MAX_LEN));
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
 * @brief translate all \r\n into plain \n. A plain \r not followed by \n must not be removed.
 */
struct ascii_to_bin_ret lfd_ascii_ascii_to_bin(char* p_buf, apr_size_t in_len, int prev_cr)
{
	struct ascii_to_bin_ret rc;
	apr_size_t indexx = 0;
	apr_size_t written = 0;
	char* p_out = p_buf + 1;
	rc.last_was_cr = 0;
	if (prev_cr && (!in_len || p_out[0] != '\n'))
	{
		p_buf[0] = '\r';
		rc.p_buf = p_buf;
		written++;
	}
	else
	{
		rc.p_buf = p_out;
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
			rc.last_was_cr = 1;
		}
		else if (p_buf[indexx + 2] != '\n')
		{
			*p_out++ = the_char;
			written++;
		}
		indexx++;
	}
	rc.stored = written;
	return rc;
}



