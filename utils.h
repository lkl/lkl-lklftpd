#ifndef LKLFTPD_UTILS_H__
#define LKLFTPD_UTILS_H__
#include <apr.h>

#define STR_ERROR_MAX_LEN 300

enum err_levels
{
	LFD_ERROR,
	LFD_WARN,
	LFD_DBG,
};

enum err_source_t
{
	ERR_SOURCE_APR,
	ERR_SOURCE_LINUX,
};

void lfd_log(enum err_levels lvl, char * fmt, ...);
void lfd_log_err_impl(enum err_source_t errsrc, const char * file, int line, const char * func, int rc, char * fmt, ...);

#define lfd_log_err lfd_log_apr_err
#define lfd_log_apr_err(rc, fmt...)     lfd_log_err_impl(ERR_SOURCE_APR, __FILE__, __LINE__, __FUNCTION__, rc, fmt)
#define lfd_log_linux_err(rc, fmt...)   lfd_log_err_impl(ERR_SOURCE_LINUX, __FILE__, __LINE__, __FUNCTION__, rc, fmt)

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

