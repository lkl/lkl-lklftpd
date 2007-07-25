#ifndef LKLFTPD_UTILS_H__
#define LKLFTPD_UTILS_H__
#include <apr.h>
#include <apr_errno.h>

#define STR_ERROR_MAX_LEN 300

enum err_levels
{
	LFD_ERROR,
	LFD_WARN,
	LFS_DBG
};

void lfd_log(enum err_levels lvl, char * fmt, ...);
char* lfd_apr_strerror_thunsafe(apr_status_t rc);
void bug(const char* p_text);
apr_size_t lfd_ascii_bin_to_ascii(const char* p_in, char* p_out, apr_size_t in_len);
#endif//LKLFTPD_UTILS_H__

