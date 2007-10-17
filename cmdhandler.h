#ifndef LKLFTPD_CMDHANDLER_H__
#define LKLFTPD_CMDHANDLER_H__

#include "cmdio.h"


// login commands
int handle_user_cmd(struct lfd_sess* sess);
int handle_pass_cmd(struct lfd_sess* sess);


// abort/quit
apr_status_t handle_quit(struct lfd_sess * sess);
apr_status_t handle_abort(struct lfd_sess* sess);


// administrative commands
apr_status_t handle_syst(struct lfd_sess * sess);
apr_status_t handle_type(struct lfd_sess *psess);
apr_status_t handle_site(struct lfd_sess* sess);


// file transfer/status commands
apr_status_t handle_retr(struct lfd_sess *sess);
apr_status_t handle_stor(struct lfd_sess *sess);
apr_status_t handle_stou(struct lfd_sess *sess);
apr_status_t handle_appe(struct lfd_sess *sess);
apr_status_t handle_dele(struct lfd_sess *sess);
apr_status_t handle_list(struct lfd_sess *sess);


// directory commands
apr_status_t handle_dir_remove(struct lfd_sess *sess);
apr_status_t handle_dir_create(struct lfd_sess *sess);
apr_status_t handle_pwd(struct lfd_sess *sess);
apr_status_t handle_cwd(struct lfd_sess *sess);
apr_status_t handle_cdup(struct lfd_sess *sess);
apr_status_t handle_rnfr(struct lfd_sess *sess, char** temp_path);
apr_status_t handle_rnto(struct lfd_sess *sess, char * old_path);
apr_status_t handle_bad_rnto(struct lfd_sess *sess);

#endif//LKLFTPD_CMDHANDLER_H__
