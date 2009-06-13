#ifndef LKLFTPD_UTILS_H__
#define LKLFTPD_UTILS_H__
#include <apr.h>
#include <apr_errno.h>

#define STR_ERROR_MAX_LEN 300

enum err_levels
{
	LFD_ERROR,
	LFD_WARN,
	LFD_DBG
};

void lfd_log(enum err_levels lvl, char * fmt, ...);
char* lfd_apr_strerror_thunsafe(apr_status_t rc);
void bug(const char* p_text);


struct ascii_to_bin_ret
{
	apr_size_t stored;
	int last_was_cr;
	char* p_buf;
};
struct ascii_to_bin_ret lfd_ascii_ascii_to_bin(char* p_in, apr_size_t in_len, int prev_cr);
apr_size_t lfd_ascii_bin_to_ascii(const char* p_in, char* p_out, apr_size_t in_len);
#endif//LKLFTPD_UTILS_H__

