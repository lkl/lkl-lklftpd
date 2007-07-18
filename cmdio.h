#ifndef LKLFTPD_CMDIO_H__
#define LKLFTPD_CMDIO_H__

#include <apr_errno.h>
#include <apr_errno.h>
#include "worker.h"
#include "sess.h"


apr_status_t lfd_cmdio_write(struct lfd_sess * p_sess, int cmd, const char *msg, ...);
#endif//LKLFTPD_CMDIO_H__

