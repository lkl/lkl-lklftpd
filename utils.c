// utilities that may be needed in more than one place
#include <apr_strings.h>
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


/**
 *@brief parses a string of delimited numbers between 0 and 255 and stores them in the p_items buffer
 */
const unsigned char * lkl_str_parse_uchar_string_sep(char * input_str, char sep, unsigned char* p_items, unsigned int items)
{
	char * last, * str;
	unsigned int i;
	apr_status_t rc;
	char sep_str[] = " ";
	sep_str[0] = sep;

	last = input_str;
	for (i = 0; i < items; i++)
	{
		apr_off_t this_number;

		str = apr_strtok(input_str, sep_str, &last);
		if((NULL == str) || ('\0' == *str))
			return 0;

		/* Sanity - check for too many or two few tokens! */
		if (    (i <  (items-1) && (0 == strlen(last))) ||
				       (i == (items-1) && (0 != strlen(last))))
		{
			return 0;
		}

		rc = apr_strtoff(&this_number, input_str, NULL, 10);
		if(APR_SUCCESS != rc)
			return 0;

		// validate range fits into one byte
		if(this_number < 0 || this_number > 255)
			return 0;

		/* If this truncates from int to uchar, we don't care */
		p_items[i] = (unsigned char) this_number;

		/* The right hand side of the comma now becomes the new string to breakdown */
		input_str = last;
	}
	return p_items;
}

