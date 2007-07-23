#ifndef LKLFTPD_SESS_H__
#define LKLFTPD_SESS_H__

#include <apr_network_io.h>
#include <apr_pools.h>
#include <apr_errno.h>
#include <apr_thread_proc.h>
#include <apr_atomic.h>

struct lfd_data_sess
{
	apr_thread_t	* data_conn_th;
	apr_socket_t	* data_sock;		// the data connection socket
	apr_pool_t	* data_pool;		// my temporary pool used for data connection
	int		  in_progress;		// status of the current transfer (in progress or not )
};

struct lfd_sess
{
	struct lfd_data_sess	* data_conn;			// only one active data connection at a time
	apr_socket_t		* comm_sock;			//the client command socket
	apr_pool_t		* loop_pool; 			//use this pool to allocate temporary data and data that is relevant to the current command only.
	apr_pool_t		* sess_pool; 			//use this pool to allocate objects that have meaning durring the whole lifetime of the session.
	char 			* cmd_input_buffer;		//buffer used for reading commands
	char			* dbg_strerror_buffer; 		//this buffer is used to map string error numbers to error descriptions.
	char			* user;				// the user's name for this session
	char			* passwd;			// user's password for this session
	char			* ftp_cmd_str;			// command body
	char			* ftp_arg_str;			// command argument
	char			* abs_path;			// the user's home directory path - different for each session
	char			* rel_path;			// the path relative to the user's home directory - it may be changed by the user
	apr_socket_t		* pasv_listen_fd;		//PASSIVE listen descriptor
	apr_sockaddr_t		* p_port_sockaddr;		//port configured by PORT to be used by ### TODO insert where it's supposed to be used
};
extern const size_t cmd_input_buffer_len;	//length of lfd_sess.cmd_input_buffer

apr_status_t lfd_sess_create(struct lfd_sess **plfd_sess, apr_thread_t * thd, apr_socket_t * sock);
void lfd_sess_destroy(struct lfd_sess *sess);
char * lfd_sess_strerror(struct lfd_sess * sess, apr_status_t rc);

apr_status_t lfd_data_sess_create(struct lfd_data_sess **plfd_data_sess, apr_thread_t * thd, apr_socket_t *sock);
void lfd_data_sess_destroy(struct lfd_data_sess *sess);

#endif//LKLFTPD_SESS_H__
