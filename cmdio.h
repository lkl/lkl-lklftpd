#ifndef LKLFTPD_CMDIO_H__
#define LKLFTPD_CMDIO_H__

#include <apr_errno.h>
#include <apr_errno.h>
#include "worker.h"
#include "sess.h"


apr_status_t lfd_cmdio_write(struct lfd_sess * p_sess, int cmd, const char *msg, ...);

/**
* PURPOSE - Read an FTP command (and optional argument) from the FTP control connection.
* PARAMETERS:
	p_sess - The current session object
	p_cmd_str - Where to put the FTP command string (may be empty)
	p_arg_str - Where to put the FTP argument string (may be empty)
	set_alarm - If true, the control connection inactivity monitor is used
**/
void lfd_cmdio_get_cmd_and_arg(struct lfd_sess* p_sess, char** p_cmd_str, char** p_arg_str, int set_alarm);

#endif//LKLFTPD_CMDIO_H__
