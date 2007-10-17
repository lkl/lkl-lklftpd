#ifndef LKLFTPD_CONNECTION_H__
#define LKLFTPD_CONNECTION_H__

#include <apr.h>
#include "sess.h"
apr_status_t handle_port(struct lfd_sess* sess);
apr_status_t handle_pasv(struct lfd_sess * sess);
void lfd_dispose_transfer_fd(struct lfd_sess* sess);
apr_socket_t* lfd_get_remote_transfer_fd(struct lfd_sess* sess, const char* p_status_msg);
int data_transfer_checks_ok(struct lfd_sess* sess);
void lfd_communication_cleanup(struct lfd_sess * sess);

#endif//LKLFTPD_CONNECTION_H__

