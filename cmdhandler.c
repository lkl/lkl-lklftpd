#include <string.h>

#include <apr_strings.h>
#include <apr_tables.h>
#include <apr_random.h>
#include <apr_poll.h>

#include <assert.h>
#include "cmdhandler.h"
#include "ftpcodes.h"
#include "utils.h"
#include "fileops.h"
#include "config.h"
#include "connection.h"


//TODO: rename and move to apppropriate place
#define VSFTP_DATA_BUFSIZE      65536

const unsigned char * lkl_str_parse_uchar_string_sep(char *, char, unsigned char*, unsigned int);

int handle_user_cmd(struct lfd_sess* sess)
{
	//###: any user is accepted
	//we're allocating from the loop pool for now. 
	// Allocate from the sess pool when the user logs in successfully
	sess->user = apr_pstrdup(sess->loop_pool, sess->ftp_arg_str);
	
	//sanitize the input: We don't accept usernames with '/' in the name 
	// because we use the name to determine the home directory as in 
	// "/home/%USERNAME%" and a slash in the name will point to a subdirectory
	// of another user.
	if(strchr(sess->user, '/'))
		return 0;
	return 1;
}

int handle_pass_cmd(struct lfd_sess* sess)
{
	//###: any pass is accepted
	return 1;
}



apr_status_t handle_syst(struct lfd_sess * sess)
{
	apr_status_t rc;
	//TODO: check the RFC: is this the default setting. If not have it in a config file and fix the message.
	rc = lfd_cmdio_write(sess, FTP_SYSTOK, "UNIX Type: L8");
	return rc;
}

apr_status_t handle_quit(struct lfd_sess * sess)
{
	apr_status_t rc;

	// wait for the active transfer to end
	if(0 != sess->data_conn->in_progress)
	{
		apr_thread_join(&rc, sess->data_conn->data_conn_th);
		lfd_data_sess_destroy(sess->data_conn);
	}
	lfd_cmdio_write(sess,  FTP_GOODBYE, "See you later, aligator.");
	return APR_SUCCESS;
}

apr_status_t handle_abort(struct lfd_sess* sess)
{
	// stop the active data transfer and close the data connection
	if(0 == sess->data_conn->in_progress)
	{
		lfd_cmdio_write(sess, FTP_ABOR_NOCONN, "Nothing to abort.");
		return APR_SUCCESS;
	}
	if(NULL != sess->data_conn->data_conn_th)
		apr_thread_exit(sess->data_conn->data_conn_th, APR_SUCCESS);

	lfd_data_sess_destroy(sess->data_conn);
	lfd_cmdio_write(sess, FTP_ABOROK, "File transfer aborted.");
	return APR_SUCCESS;
}


static char * resolve_tilde(const char * str, struct lfd_sess* sess, apr_pool_t * allocator_pool)
{
	size_t len;
	len = strlen(str);
	
	if (NULL == str)
		return NULL;
	
	
	if (len > 0 && '~' == str[0])
	{
		if (1 == len)
		{
			return apr_pstrdup(allocator_pool, sess->home_str);
		}
		else if('/' == str[1])
		{
			return apr_pstrcat(allocator_pool, sess->home_str, str+2, NULL);
		}
		else
		{
			//we don't support ~asdf kind of paths.
			return NULL;
		}
	}
	return apr_pstrdup(allocator_pool, str);
}

static char * get_abs_path(struct lfd_sess *sess, apr_pool_t * allocator_pool)
{
	char * path;

	if ( (NULL == sess->ftp_arg_str) || ('\0' == sess->ftp_arg_str[0]) )
		return NULL;
	
	//this is only temp, so allocating from loop_pool is fine.
	path = resolve_tilde(sess->ftp_arg_str, sess, sess->loop_pool);
	
	if (NULL == path)
		return NULL;
	
	
	//TODO: this is Linux centric
	if('/' == path[0])
	{
		//the path is already absolute, 
		// just need to allocate it from an appropriate pool
		path = apr_pstrdup(allocator_pool, path);
	}
	else
	{
		path = apr_pstrcat(allocator_pool, sess->cwd_path, path, NULL);
	}
	
	//TODO: remove double (or more) "/" from the path
	return path;
}




apr_status_t handle_dir_remove(struct lfd_sess *sess)
{
	apr_status_t rc;
	char * rpath;

	rc = APR_SUCCESS;
	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "Command must have an argument.");
		return rc;
	}
	
	// check to see if sess->ftp_arg_str is an absolute or relative path
	rpath = get_abs_path(sess, sess->loop_pool);
	if(NULL == rpath)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	rc = lkl_dir_remove(rpath, sess->loop_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lkl_dir_remove failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "Cannot remove directory %s.", rpath);
	}
	else
	{
		rc = lfd_cmdio_write(sess, FTP_RMDIROK, "%s Directory deleted.",rpath);
	}
	
	return rc;
}

apr_status_t handle_dir_create(struct lfd_sess *sess)
{
	apr_status_t rc;
	char *rpath;
	apr_finfo_t finfo;

	rc = APR_SUCCESS;
	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "Command must have an argument.");
		return rc;
	}
	
	// check to see if sess->ftp_arg_str is an absolute or relative path
	rpath = get_abs_path(sess, sess->loop_pool);
	if(NULL == rpath)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "The server has encountered an error processing the path.");
		return APR_EINVAL;
	}

	rc = lkl_stat(&finfo, rpath, APR_FINFO_TYPE, sess->loop_pool);
	if (APR_SUCCESS == rc)
	{
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "%s Directory exists.", rpath);
		return rc;
	}
	rc = lkl_dir_make(rpath, APR_FPROT_OS_DEFAULT, sess->loop_pool);

	if(APR_SUCCESS !=rc)
	{
		lfd_log(LFD_ERROR, "lkl_dir_make failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "Cannot create directory %s.", rpath);
	}
	else
	{
		rc = lfd_cmdio_write(sess, FTP_MKDIROK, "%s Directory created.", rpath);
	}
	
	return rc;
}

apr_status_t handle_pwd(struct lfd_sess *sess)
{
	apr_status_t rc;

	rc = lfd_cmdio_write(sess, FTP_PWDOK, sess->cwd_path);
	return rc;
}

apr_status_t handle_cwd(struct lfd_sess *sess)
{
	apr_status_t rc;
	char * path;
	apr_finfo_t finfo;
	size_t len;

	rc = APR_SUCCESS;
	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "Command must have an argument.");
		return rc;
	}
	
	if (0 == strcmp(sess->ftp_arg_str, ".."))
	{
		return handle_cdup(sess);
	}
	
	path = get_abs_path(sess, sess->loop_pool);
	if(NULL == path)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "The server has encountered an error processing the path");
		return APR_EINVAL;
	}
	
	
	//assure that the last character is a slash
	len = strlen(path);
	if('/' != path[len-1])
	{
		path = apr_pstrcat(sess->loop_pool, path, "/", NULL);
	}
	
	// verify that the path represents a valid directory
	rc = lkl_stat(&finfo, path, APR_FINFO_TYPE, sess->loop_pool);
	if((APR_SUCCESS != rc) || (APR_DIR != finfo.filetype))
	{
		lfd_log(LFD_ERROR, "lkl_stat failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "%s is not a directory.", path);
		return rc;
	}

	//allocate from a persistent pool
	sess->cwd_path = apr_pstrdup(sess->sess_pool, path);
	
	return lfd_cmdio_write(sess, FTP_CWDOK, "Directory changed to %s.", sess->cwd_path);
}

apr_status_t handle_cdup(struct lfd_sess *sess)
{
	apr_status_t rc;
	char * pos;
	size_t len;
	rc = APR_SUCCESS;
	
	// DOESN'T have an argument
	
	// changes the current directory with it's parent
	//TODO: this is Linux centric.
	
	len = strlen(sess->cwd_path);
	if(1 == len)
	{
		return lfd_cmdio_write(sess, FTP_ALLOOK, "Already in root directory.");
	}
	
	pos = sess->cwd_path + len - 2; //without '\0' and '/' from the end of the path
	//go backwards towards the root of the path
	while(pos != sess->cwd_path)
	{
		if ('/' == pos[0])
		{
			pos[1] = '\0';
			break;
		}
		pos --;
	}
	
	rc = lfd_cmdio_write(sess, FTP_ALLOOK, "Changed to directory %s.", sess->cwd_path);
	return rc;
}

apr_status_t handle_rnfr(struct lfd_sess *sess, char ** temp_path)
{
	apr_status_t rc;
	apr_finfo_t finfo;
	char * path;

	*temp_path = NULL;
	rc = APR_SUCCESS;
	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "Bad command argument.");
		return rc;
	}
	path = get_abs_path(sess, sess->sess_pool);

	if(NULL == path)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "The server has encountered an error.");
		return APR_EINVAL;
	}

	rc = lkl_stat(&finfo, path, APR_FINFO_TYPE, sess->loop_pool);
	if((APR_SUCCESS != rc) || ((APR_REG != finfo.filetype) && (APR_DIR != finfo.filetype)))
	{
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "%s is not a directory or a file.", sess->ftp_arg_str);
		return rc;
	}
	*temp_path = path;
	rc = lfd_cmdio_write(sess, FTP_RNFROK, "Requested file action pending further information.");
	return rc;
}

apr_status_t handle_bad_rnto(struct lfd_sess *sess)
{
	apr_status_t rc;

	rc = lfd_cmdio_write(sess, FTP_BADPROT, "Bad sequence of commands.");
	return rc;
}

apr_status_t handle_rnto(struct lfd_sess *sess, char * old_path)
{
	apr_status_t rc;
	char * path;

	rc = APR_SUCCESS;
	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Bad command argument.");
		return rc;
	}

	// obtain the new path and call lkl_file_rename
	path = get_abs_path(sess, sess->loop_pool);

	rc = lkl_file_rename(old_path, path, sess->loop_pool);

	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lkl_file_rename failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Cannot rename directory/file %s.", path);
	}
	else
		rc = lfd_cmdio_write(sess, FTP_RENAMEOK, "Directory/file was succesfully renamed.");
	return rc;
}

apr_status_t handle_type(struct lfd_sess *sess)
{
	//TODO: check if this is sufficient
	lfd_cmdio_write(sess, FTP_TYPEOK, "TYPE ok");
	return APR_SUCCESS;
}



/**
 * @brief Writes all the content on the buffer to the socket.
 * @param fd IN the socket on which to send the data
 * @param p_buf IN the location of the data
 * @param psize IN/OUT input: the buffer length, output: what amount get written on the socket.
*/
static apr_status_t lfd_socket_write_full(apr_socket_t * fd, const void* p_buf, apr_size_t * psize)
{
	apr_status_t	rc;
	apr_size_t	num_written;
	apr_size_t	size = *psize;

	*psize = 0;
	while (1)
	{
		num_written = size;
		rc = apr_socket_send(fd, (const char*)p_buf + *psize, &num_written);
		if (APR_SUCCESS != rc)
		{
			return rc;
		}
		else if (0 == num_written)
		{
			/* Written all we're going to write.. */
			return APR_SUCCESS;
		}

		*psize += num_written;
		size -= num_written;
		if (0 == size)
		{
			/* Hit the write target, cool. */
			return APR_SUCCESS;
		}
	}
}

struct lfd_transfer_ret
{
	int retval;
	apr_size_t transferred;

};
static struct lfd_transfer_ret do_file_send_rwloop(struct lfd_sess* sess, lkl_file_t * file_fd, int is_ascii)
{
	char		* p_readbuf;
	char		* p_asciibuf;
	char		* p_writefrom_buf;
	apr_size_t 	  chunk_size = 4096;
	apr_socket_t	* sock = sess->data_conn->data_sock;
	struct lfd_transfer_ret ret_struct = { 0, 0 };


	p_readbuf = apr_palloc(sess->loop_pool, is_ascii?VSFTP_DATA_BUFSIZE*2:VSFTP_DATA_BUFSIZE);
	p_asciibuf= apr_palloc(sess->loop_pool, VSFTP_DATA_BUFSIZE*2);

	if (is_ascii)
	{
		p_writefrom_buf = p_asciibuf;
	}
	else
	{
		p_writefrom_buf = p_readbuf;
	}

	while (1)
	{
		apr_size_t	num_to_write;
		apr_size_t	bytes_read = chunk_size;
		apr_size_t	bytes_written;
		apr_status_t	rc;
		rc = lkl_file_read(file_fd, p_readbuf, &bytes_read);
		if((APR_SUCCESS != rc) && (APR_EOF != rc))
		{
			ret_struct.retval = -1;
			lfd_log(LFD_ERROR, "lkl_file_read failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
			return ret_struct;
		}
		else if (0 == bytes_read)
		{
			/* Success - cool */
			return ret_struct;
		}
		if (is_ascii)
		{
			num_to_write = lfd_ascii_bin_to_ascii(p_readbuf, p_asciibuf, bytes_read);
		}
		else
		{
			num_to_write = bytes_read;
		}

		bytes_written = bytes_read;
		rc = lfd_socket_write_full(sock, p_writefrom_buf, &bytes_written);
		if((APR_SUCCESS != rc) || (bytes_written != bytes_read))
		{
			ret_struct.retval = -2;
			lfd_log(LFD_ERROR, "lfd_socket_write_full failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
			return ret_struct;
		}
		ret_struct.transferred += bytes_written;
	}
}

static void check_abor(struct lfd_sess* sess)
{
	/* If the client sent ABOR, respond to it here */
	if (sess->data_conn->abor_received)
	{
		sess->data_conn->abor_received = 0;
		lfd_cmdio_write(sess, FTP_ABOROK, "ABOR successful.");
	}
}

apr_status_t handle_retr(struct lfd_sess *sess)
{
	char			* msg;
	apr_off_t		  offset = sess->restart_pos;
	apr_status_t		  rc;
	lkl_file_t 		* file;
	apr_socket_t		* remote_fd;
	apr_finfo_t		  finfo;
	struct lfd_transfer_ret   trans_ret;
	char 			* path;
	sess->restart_pos = 0;

	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Command must have an argument.");
		return rc;
	}

	if (!data_transfer_checks_ok(sess))
	{
		return APR_EINVAL;
	}
	if (sess->is_ascii && offset != 0)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "No support for resume of ASCII transfer.");
		return APR_EINVAL;
	}
	
	path = get_abs_path(sess, sess->loop_pool);

	rc = lkl_file_open(&file, path, APR_FOPEN_READ|APR_FOPEN_BINARY|APR_FOPEN_LARGEFILE|APR_FOPEN_SENDFILE_ENABLED, 0, sess->loop_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, "Failed to open file.");
		return rc;
	}
	
	rc = lkl_stat(&finfo, path, APR_FINFO_TYPE|APR_FINFO_SIZE, sess->loop_pool);
	if((APR_SUCCESS != rc) || (APR_REG != finfo.filetype))
	{
		lfd_cmdio_write(sess, FTP_FILEFAIL, (APR_SUCCESS == rc)?"File type is not of regular type":"Failed to stat file.");
		lkl_file_close(file);
		return rc;
	}
	if (0 != offset)
	{
		rc = lkl_file_seek(file, APR_SET, &offset);
		if(APR_SUCCESS != rc)
		{
			lfd_cmdio_write(sess, FTP_FILEFAIL, "Failed to seek into file.");
			lkl_file_close(file);
			return rc;
		}
	}

	msg = apr_psprintf(sess->loop_pool, "Opening %s %s %s %s %u %s",
			sess->is_ascii?"ASCII":"BINARY", " mode data connection for ",
   			path, " ( ", (unsigned int)finfo.size, " bytes).");


	remote_fd = lfd_get_remote_transfer_fd(sess, msg);
	if(NULL == remote_fd)
	{
		lfd_communication_cleanup(sess);
		lkl_file_close(file);
		return APR_EINVAL;
	}

	trans_ret = do_file_send_rwloop(sess, file, sess->is_ascii);
	lfd_dispose_transfer_fd(sess);

	if (-1 == trans_ret.retval)
	{
		lfd_cmdio_write(sess, FTP_BADSENDFILE, "Failure reading local file.");
	}
	else if (-2 == trans_ret.retval)
	{
		lfd_cmdio_write(sess, FTP_BADSENDNET, "Failure writing network stream.");
	}
	else
	{
		lfd_cmdio_write(sess, FTP_TRANSFEROK, "File send OK.");
	}
	check_abor(sess);
	lfd_communication_cleanup(sess);
	lkl_file_close(file);

	return APR_SUCCESS;
}

apr_status_t handle_dele(struct lfd_sess * sess)
{
	apr_status_t rc;
	char * path;

	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Bad command argument.");
		return rc;
	}

	path = get_abs_path(sess, sess->loop_pool);

	if(NULL == path)
	{
		lfd_cmdio_write(sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}

	rc = lkl_file_remove(path, sess->loop_pool);
	if(APR_SUCCESS !=rc)
	{
		lfd_log(LFD_ERROR, "lkl_file_remove failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		rc = lfd_cmdio_write(sess, FTP_FILEFAIL, "Requested action not taken. File unavailable.");
	}
	else
		rc = lfd_cmdio_write(sess,  FTP_DELEOK,"File deleted.");
	return rc;
}


static char* get_unique_filename(char * base_str, apr_pool_t * pool)
{
	// Use silly wu-ftpd algorithm for compatibility
	// same algorithm is employed by the vsftpd for the same reasons
	char		* dest_str = NULL;
	apr_finfo_t	  finfo;
	unsigned int	  suffix = 1;
	apr_status_t	  rc;

	//apr_stat insuccess should be seen as an "error while finding the file."
		// This is good, if no file is found then we should have an unique filename in "dest_str".

	rc = lkl_stat(&finfo, base_str, APR_FINFO_TYPE, pool);
	if(APR_SUCCESS != rc)
	{
		return base_str;
	}

	// Also this is moronically race prone: we should use kernel-based unique filename generators.
	while (1)
	{
		dest_str = apr_psprintf(pool, "%s.%u", base_str, suffix);
		rc = lkl_stat(&finfo, dest_str, APR_FINFO_TYPE, pool);
		if(APR_SUCCESS != rc)
		{
			return dest_str;
		}
		++suffix;
	}
}

static struct lfd_transfer_ret do_file_recv(struct lfd_sess* sess, lkl_file_t * file_fd, int is_ascii)
{
	apr_off_t num_to_write;
	struct lfd_transfer_ret ret_struct = { 0, 0 };
	apr_off_t chunk_size = VSFTP_DATA_BUFSIZE;
	int prev_cr = 0;

	/* Now that we do ASCII conversion properly, the plus one is to cater for
		* the fact we may need to stick a '\r' at the front of the buffer if the
		* last buffer fragment eneded in a '\r' and the current buffer fragment
		* does not start with a '\n'.
	*/
	char * p_recvbuf = apr_palloc(sess->loop_pool, VSFTP_DATA_BUFSIZE + 1);

	while (1)
	{
		apr_status_t rc;
		apr_size_t bytes_recvd = chunk_size;
		apr_size_t bytes_written;
		const char* p_writebuf = p_recvbuf + 1;

		rc = apr_socket_recv(sess->data_conn->data_sock, p_recvbuf + 1, &bytes_recvd);
		if((APR_SUCCESS != rc) && (APR_EOF != rc))
		{
			lfd_log(LFD_ERROR, "apr_socket_recv in do_file_recv failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
			ret_struct.retval = -2;
			return ret_struct;
		}
		else if (0 == bytes_recvd && !prev_cr && (APR_EOF == rc))
		{
			/* Transfer done, nifty */
			return ret_struct;
		}
		num_to_write = bytes_recvd;
		ret_struct.transferred += num_to_write;
		if (is_ascii)
		{
			/* Handle ASCII conversion if we have to. Note that using the same
			* buffer for source and destination is safe, because the ASCII ->
			* binary transform only ever results in a smaller file.
			*/
			struct ascii_to_bin_ret rc = lfd_ascii_ascii_to_bin(p_recvbuf, num_to_write, prev_cr);
			num_to_write = rc.stored;
			prev_cr = rc.last_was_cr;
			p_writebuf = rc.p_buf;
		}
		rc = lkl_file_write_full(file_fd, p_writebuf, num_to_write, &bytes_written);
		if((APR_SUCCESS != rc) || (num_to_write != bytes_written))
		{
			lfd_log(LFD_ERROR, "lkl_file_write_full in do_file_recv failed with errorcode[%d] and error message[%s]. "
					"Bytes that had to be written [%d]."
					"Bytes that  got      written [%d].",
					rc, lfd_sess_strerror(sess, rc), num_to_write, bytes_written);
			ret_struct.retval = -1;
			return ret_struct;
		}
	}
}


static apr_status_t handle_upload_common(struct lfd_sess *sess, int is_append, int is_unique)
{
	char			* msg;
	char			* filename;
	apr_off_t		  offset = sess->restart_pos;
	apr_status_t		  rc;
	apr_int32_t		  flags;
	lkl_file_t 		* file;
	apr_socket_t		* remote_fd;
	struct lfd_transfer_ret   trans_ret;
	char 			* path;
	sess->restart_pos = 0;

	if(NULL == sess->ftp_arg_str)
	{
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Command must have an argument.");
		return rc;
	}

	if (!data_transfer_checks_ok(sess))
	{
		return APR_EINVAL;
	}
	path = get_abs_path(sess, sess->loop_pool);

	if (is_unique)
	{
		filename = get_unique_filename(path, sess->loop_pool);
	}
	else
	{
		filename = path;
	}

	flags = APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_APPEND | APR_FOPEN_BINARY;
	if (!is_append && (0 == offset))
	{
		flags |= APR_FOPEN_TRUNCATE;
	}
	rc = lkl_file_open(&file, filename, flags, 0, sess->loop_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "lkl_file_open failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		lfd_cmdio_write(sess, FTP_UPLOADFAIL, "Could not create file.");
		return rc;
	}

	if (!is_append && 0 != offset)
	{
		/* XXX - warning, allows seek past end of file! Check for seek > size? */
		rc = lkl_file_seek(file, APR_SET, &offset);
		if(APR_SUCCESS != rc)
		{
			lfd_log(LFD_ERROR, "lkl_file_seek handle_uppload_common failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
			lfd_cmdio_write(sess, FTP_UPLOADFAIL, "Could not seek into file.");
			return rc;
		}
	}

	msg = "Ok to send data.";
	if (is_unique) //overwrite msg
	{
		msg = apr_pstrcat(sess->loop_pool, "FILE: ", filename, NULL);
		rc = lfd_cmdio_write(sess, FTP_DATACONN, msg);
		if(APR_SUCCESS != rc)
		{
			return rc;
		}
	}
	remote_fd = lfd_get_remote_transfer_fd(sess, msg);
	if(NULL == remote_fd)
	{
		return APR_EINVAL;
	}
	trans_ret = do_file_recv(sess, file, sess->is_ascii);
	lfd_dispose_transfer_fd(sess);
	sess->data_conn->transfer_size = trans_ret.transferred;
	/* XXX - handle failure, delete file? */
	if (-1 == trans_ret.retval)
	{
		lfd_cmdio_write(sess, FTP_BADSENDFILE, "Failure writing to local file.");
	}
	else if (-2 == trans_ret.retval)
	{
		lfd_cmdio_write(sess, FTP_BADSENDNET, "Failure reading network stream.");
	}
	else
		lfd_cmdio_write(sess, FTP_TRANSFEROK, "File receive OK.");
	check_abor(sess);
	lfd_communication_cleanup(sess);
	lkl_file_close(file);
	return APR_SUCCESS;
}



apr_status_t handle_stor(struct lfd_sess *sess)
{
	return handle_upload_common(sess, 0, 0);
}


apr_status_t handle_appe(struct lfd_sess *sess)
{
	return handle_upload_common(sess, 1, 0);
}



apr_status_t handle_stou(struct lfd_sess *sess)
{
	return handle_upload_common(sess, 0, 1);
}


static apr_status_t list_dir(const char * directory, apr_pool_t * pool, char ** p_dest, int la)
{
	lkl_dir_t	* dir;
	apr_finfo_t	  finfo;
	apr_status_t	  apr_err;
	apr_int32_t	  flags = APR_FINFO_TYPE | APR_FINFO_NAME | APR_FINFO_SIZE;

	apr_err = lkl_dir_open(&dir, directory, pool);
	if(APR_SUCCESS != apr_err)
	{
		*p_dest = FTP_ENDCOMMAND_SEQUENCE;
		return apr_err;
	}
	else
	{
		*p_dest = "";
	}

	for(apr_err = lkl_dir_read(&finfo, flags, dir); apr_err == APR_SUCCESS;
		   apr_err = lkl_dir_read(&finfo, flags, dir))
	{
		char buffer[1024];

		if(finfo.filetype == APR_DIR && ((finfo.name[0] == '.' && finfo.name[1] == '\0')))
			continue;
		if (!la && finfo.name[1] == '.' && finfo.name[2] == '\0')
			continue;

		if (la) {
			char time_string[3+1+2+1+5+1];
			const char *time_fmt;
			apr_time_exp_t exp_time, exp_now;
			apr_size_t dummy;

			apr_time_exp_lt(&exp_time, finfo.mtime);
			apr_time_exp_lt(&exp_now, apr_time_now());
			if (exp_now.tm_year == exp_time.tm_year)
				time_fmt="%b %d %H:%M";
			else
				time_fmt="%b %d %Y";
			apr_strftime(time_string, &dummy, sizeof(time_string), time_fmt, &exp_time);

			snprintf(buffer, sizeof(buffer), "%c%c%c%c%c%c%c%c%c%c ftp ftp %d %s %s\n",
				 (finfo.filetype == APR_DIR)?'d':'-',
				 /* user permission */
				 (finfo.protection&00400)?'r':'-',
				 (finfo.protection&00200)?'w':'-',
				 (finfo.protection&00100)?'x':'-',
				 /* group permission */
				 (finfo.protection&00040)?'r':'-',
				 (finfo.protection&00020)?'w':'-',
				 (finfo.protection&00010)?'x':'-',
				 /* others permission */
				 (finfo.protection&00004)?'r':'-',
				 (finfo.protection&00002)?'w':'-',
				 (finfo.protection&00001)?'x':'-',
				 (int)finfo.size, time_string, finfo.name);
		} else
			snprintf(buffer, sizeof(buffer), "%s%s", finfo.name,
				 (finfo.filetype == APR_DIR)?"/":"");
		*p_dest = apr_pstrcat(pool, *p_dest, buffer, FTP_ENDCOMMAND_SEQUENCE, NULL);
	}
	lkl_dir_close(dir);
	return APR_SUCCESS;
}


apr_status_t handle_list(struct lfd_sess *sess)
{
	apr_socket_t 		* remote_fd;
	apr_status_t		  rc;
	apr_size_t		  file_list_len, bytes_written;
	char			* file_list;
	char			* path;
	int la=0;

	// if the arg was NULL, take the current directory
	if(NULL == sess->ftp_arg_str || strcmp(sess->ftp_arg_str, "-la") == 0)
	{
		if (sess->ftp_arg_str != NULL)
			la=1;
		path = apr_pstrdup(sess->loop_pool, sess->cwd_path);
	}
	else
	{
		path = get_abs_path(sess, sess->loop_pool);
	}
	
	if(NULL == path)
	{
		lfd_cmdio_write(sess, FTP_BADOPTS, "The server has encountered an error.");
		return APR_EINVAL;
	}
	
	rc = list_dir(path, sess->loop_pool, &file_list, la);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "list_dir failed with errorcode[%d] and error message[%s]", rc, lfd_sess_strerror(sess, rc));
		rc = lfd_cmdio_write(sess, FTP_BADOPTS, "Cannot list files.");
		return rc;
	}
	file_list_len = bytes_written = strlen(file_list);

	remote_fd = lfd_get_remote_transfer_fd(sess, "Here comes the directory listing");
	if(NULL == remote_fd)
	{
		return APR_EINVAL;
	}
	rc = apr_socket_send(remote_fd, file_list, &bytes_written);
	if((APR_SUCCESS == rc) && (file_list_len == bytes_written))
	{
		rc = lfd_cmdio_write(sess, FTP_TRANSFEROK, "Directory sent OK");
	}
	else
	{
		rc = lfd_cmdio_write(sess, FTP_BADSENDNET, "Error sendind data");
	}
	lfd_dispose_transfer_fd(sess);


	return rc;
}


apr_status_t handle_site(struct lfd_sess* sess)
{
	apr_status_t rc = APR_SUCCESS;
	if (0 == apr_strnatcasecmp("CHMOD", sess->ftp_arg_str) )
	{
		lfd_log(LFD_ERROR, "SITE CHMOD NIMPL");
	}
	else if (0 == apr_strnatcasecmp("UMASK", sess->ftp_arg_str) )
	{
		lfd_log(LFD_ERROR, "SITE UMASK NIMPL");
	}
	else if (0 == apr_strnatcasecmp("HELP", sess->ftp_arg_str) )
	{
		rc = lfd_cmdio_write(sess, FTP_SITEHELP, "CHMOD UMASK HELP");
	}
	else
	{
		rc = lfd_cmdio_write(sess, FTP_BADCMD, "Unknown SITE command.");
	}
	return rc;
}
/*

static void
		handle_site_chmod(struct vsf_session* sess, struct mystr* p_arg_str)
{
	static struct mystr s_chmod_file_str;
	unsigned int perms;
	int retval;
	if (str_isempty(p_arg_str))
	{
		vsf_cmdio_write(sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
		return;
	}
	str_split_char(p_arg_str, &s_chmod_file_str, ' ');
	if (str_isempty(&s_chmod_file_str))
	{
		vsf_cmdio_write(sess, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
		return;
	}
	resolve_tilde(&s_chmod_file_str, sess);
	vsf_log_start_entry(sess, kVSFLogEntryChmod);
	str_copy(&sess->log_str, &s_chmod_file_str);
	prepend_path_to_filename(&sess->log_str);
	str_append_char(&sess->log_str, ' ');
	str_append_str(&sess->log_str, p_arg_str);
	if (!vsf_access_check_file(&s_chmod_file_str))
	{
		vsf_cmdio_write(sess, FTP_NOPERM, "Permission denied.");
		return;
	}
	// Don't worry - our chmod() implementation only allows 0 - 0777
	perms = str_octal_to_uint(p_arg_str);
	retval = str_chmod(&s_chmod_file_str, perms);
	if (vsf_sysutil_retval_is_error(retval))
	{
		vsf_cmdio_write(sess, FTP_FILEFAIL, "SITE CHMOD command failed.");
	}
	else
	{
		vsf_log_do_log(sess, 1);
		vsf_cmdio_write(sess, FTP_CHMODOK, "SITE CHMOD command ok.");
	}
}

static void
		handle_site_umask(struct vsf_session* sess, struct mystr* p_arg_str)
{
	static struct mystr s_umask_resp_str;
	if (str_isempty(p_arg_str))
	{
		// Empty arg => report current umask
		str_alloc_text(&s_umask_resp_str, "Your current UMASK is ");
		str_append_text(&s_umask_resp_str,
				 vsf_sysutil_uint_to_octal(vsf_sysutil_get_umask()));
	}
	else
	{
		// Set current umask
		unsigned int new_umask = str_octal_to_uint(p_arg_str);
		vsf_sysutil_set_umask(new_umask);
		str_alloc_text(&s_umask_resp_str, "UMASK set to ");
		str_append_text(&s_umask_resp_str,
				 vsf_sysutil_uint_to_octal(vsf_sysutil_get_umask()));
	}
	vsf_cmdio_write_str(sess, FTP_UMASKOK, &s_umask_resp_str);
}
*/

