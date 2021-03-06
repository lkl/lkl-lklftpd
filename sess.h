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
	int 		  is_ascii;
	apr_off_t	  restart_pos;		//if non zero represents the offset from where to begin sending file data.
	apr_size_t	  transfer_size;
	int 		  abor_received;

};

struct lfd_sess
{
	struct lfd_data_sess	* data_conn;			// only one active data connection at a time
	apr_socket_t		* comm_sock;			//the client command socket
	apr_pool_t		* loop_pool; 			//use this pool to allocate temporary data and data that is relevant to the current command only.
	apr_pool_t		* sess_pool; 			//use this pool to allocate objects that have meaning durring the whole lifetime of the session.
	char 			* cmd_input_buffer;		//buffer used for reading commands
	char			* ftp_cmd_str;			// command body
	char			* ftp_arg_str;			// command argument
	char			* cwd_path;			// the user's current working directory
	apr_socket_t		* pasv_listen_fd;		//PASSIVE listen descriptor. The data socket is determined by accepting a socket on this listening socket.
	apr_sockaddr_t		* p_port_sockaddr;		//port configured by PORT to be used in active connections. The data socket is obtained by connecting a socket to this address
	apr_off_t		  restart_pos;			//REST value.
	int 			  is_ascii;			//current mode is ASCII.
};
extern const apr_size_t cmd_input_buffer_len;	//length of lfd_sess.cmd_input_buffer

apr_status_t lfd_sess_create(struct lfd_sess **plfd_sess, apr_thread_t * thd, apr_socket_t * sock);
void lfd_sess_destroy(struct lfd_sess *sess);

apr_status_t lfd_data_sess_create(struct lfd_data_sess **plfd_data_sess, apr_thread_t * thd, apr_socket_t *sock);
void lfd_data_sess_destroy(struct lfd_data_sess *sess);

#endif//LKLFTPD_SESS_H__
