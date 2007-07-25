#ifndef LKLFTPD_CMDHANDLER_H__
#define LKLFTPD_CMDHANDLER_H__

#include "cmdio.h"

int lfd_cmdio_cmd_equals(struct lfd_sess*sess, const char * cmd);

// login commands
int handle_user_cmd(struct lfd_sess* p_sess);
int handle_pass_cmd(struct lfd_sess* p_sess);

// abort/quit
apr_status_t handle_quit(struct lfd_sess * sess);
apr_status_t handle_abort(struct lfd_sess* p_sess);

// administrative commands
apr_status_t handle_passive(struct lfd_sess * sess);
apr_status_t handle_active(struct lfd_sess * sess);
apr_status_t handle_syst(struct lfd_sess * sess);
apr_status_t handle_type(struct lfd_sess *psess);
apr_status_t handle_port(struct lfd_sess* p_sess);

// file transfer/status commands
apr_status_t handle_retr(struct lfd_sess *sess);

// directory commands
apr_status_t handle_dir_remove(struct lfd_sess *psess);
apr_status_t handle_dir_create(struct lfd_sess *psess);
apr_status_t handle_pwd(struct lfd_sess *p_sess);
apr_status_t handle_cwd(struct lfd_sess *p_sess);
apr_status_t handle_cdup(struct lfd_sess *p_sess);
apr_status_t handle_rnfr(struct lfd_sess *p_sess);
apr_status_t handle_rnto(struct lfd_sess *p_sess);


#endif//LKLFTPD_CMDHANDLER_H__
