#ifndef LKLFTPD_CMDHANDLER_H__
#define LKLFTPD_CMDHANDLER_H__

#include "cmdio.h"

int lfd_cmdio_cmd_equals(struct lfd_sess*sess, const char * cmd);

int handle_user_cmd(struct lfd_sess* p_sess);
int handle_pass_cmd(struct lfd_sess* p_sess);

apr_status_t handle_passive(struct lfd_sess * sess);
apr_status_t handle_active(struct lfd_sess * sess);
apr_status_t handle_syst(struct lfd_sess * sess);
apr_status_t handle_quit(struct lfd_sess * sess);
apr_status_t handle_abort(struct lfd_sess* p_sess);
apr_status_t handle_dir_remove(struct lfd_sess *psess);
apr_status_t handle_dir_create(struct lfd_sess *psess);
apr_status_t handle_type(struct lfd_sess *psess);
apr_status_t handle_retr(struct lfd_sess *sess);

#endif//LKLFTPD_CMDHANDLER_H__
