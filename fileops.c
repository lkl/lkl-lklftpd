//here we implement the lkl-based file wrappers.
#ifdef LKL_FILE_APIS

#include "fileops.h"
#include <linux/poll.h>

#define APR_FILE_BUFSIZE 4096

apr_status_t lkl_file_flush_locked(lkl_file_t *thefile)
{
	apr_status_t rv = APR_SUCCESS;
	
	if (1 == thefile->direction && thefile->bufpos) 
	{
		apr_ssize_t written;
	
		do 
		{
			written = sys_write(thefile->filedes, thefile->buffer, thefile->bufpos);
		}
		while (written == -EINTR);
		if (written < 0) 
			rv = APR_EINVAL;
		else 
		{
			thefile->filePtr += written;
			thefile->bufpos = 0;
		}
	}
	return rv;
}

apr_status_t lkl_file_flush(lkl_file_t *thefile)
{
	apr_status_t rv = APR_SUCCESS;

	if (thefile->buffered) 
	{
		file_lock(thefile);
		rv = lkl_file_flush_locked(thefile);
		file_unlock(thefile);
	}
	/* There isn't anything to do if we aren't buffering the output
	* so just return success.
	*/
	return rv; 
}

static apr_status_t file_cleanup(lkl_file_t *file)
{
	apr_status_t rv = APR_SUCCESS;
	int ret;
	
	ret =  sys_close(file->filedes);
	if (0 == ret) 
	{
		file->filedes = -1;
		if (file->flags & APR_DELONCLOSE) 
			sys_unlink(file->fname);
	#if APR_HAS_THREADS
		if (file->thlock) 
			rv = apr_thread_mutex_destroy(file->thlock);
	#endif
	}
	else
		rv = -ret;

	return rv;
}


apr_status_t lkl_unix_file_cleanup(void *thefile)
{
	lkl_file_t *file = thefile;
	apr_status_t flush_rv = APR_SUCCESS, rv = APR_SUCCESS;
	
	if (file->buffered) 
		flush_rv = lkl_file_flush(file);
	rv = file_cleanup(file);
	
	return rv != APR_SUCCESS ? rv : flush_rv;
}

apr_status_t lkl_unix_child_file_cleanup(void *thefile)
{
	return file_cleanup(thefile);
}


apr_status_t lkl_file_open(lkl_file_t **new, const char *fname,
			apr_int32_t flag, apr_fileperms_t perm,
			apr_pool_t *pool)
{
	int fd;
	int oflags = 0;
#if APR_HAS_THREADS
	apr_thread_mutex_t *thlock;
	apr_status_t rv;
#endif

	if ((flag & APR_READ) && (flag & APR_WRITE))
	{
		oflags = O_RDWR;
	}
	else if (flag & APR_READ) 
	{
		oflags = O_RDONLY;
	}
	else if (flag & APR_WRITE) 
	{
		oflags = O_WRONLY;
	}
	else 
		return APR_EACCES; 
	
	if (flag & APR_CREATE) 
	{
		oflags |= O_CREAT; 
		if (flag & APR_EXCL) 
			oflags |= O_EXCL;
	}
	if ((flag & APR_EXCL) && !(flag & APR_CREATE)) 
	{
		return APR_EACCES;
	}   
	
	if (flag & APR_APPEND) 
	{
		oflags |= O_APPEND;
	}
	if (flag & APR_TRUNCATE) 
	{
		oflags |= O_TRUNC;
	}
	#ifdef O_BINARY
	if (flag & APR_BINARY) 
	{
		oflags |= O_BINARY;
	}
	#endif
	
	#if APR_HAS_LARGE_FILES && defined(_LARGEFILE64_SOURCE)
	oflags |= O_LARGEFILE;
	#elif defined(O_LARGEFILE)
	if (flag & APR_LARGEFILE) 
	{
		oflags |= O_LARGEFILE;
	}
	#endif
	
	#if APR_HAS_THREADS
	if ((flag & APR_BUFFERED) && (flag & APR_XTHREAD)) 
	{
		rv = apr_thread_mutex_create(&thlock,
					APR_THREAD_MUTEX_DEFAULT, pool);
		if (rv) 
			return rv;
	}
	#endif
	
	if (perm == APR_OS_DEFAULT) 
	{
		fd = sys_open(fname, oflags, 0666);
	}
	else 
	{
		fd = sys_open(fname, oflags, lkl_unix_perms2mode(perm));
	} 
	if (fd < 0) 
		return APR_EINVAL;
	
	(*new) = (lkl_file_t *)apr_pcalloc(pool, sizeof(lkl_file_t));
	(*new)->pool = pool;
	(*new)->flags = flag;
	(*new)->filedes = fd;
	
	(*new)->fname = apr_pstrdup(pool, fname);
	
	(*new)->blocking = BLK_ON;
	(*new)->buffered = (flag & APR_BUFFERED) > 0;
	
	if ((*new)->buffered) 
	{
		(*new)->buffer = apr_palloc(pool, APR_FILE_BUFSIZE);
	#if APR_HAS_THREADS
		if ((*new)->flags & APR_XTHREAD) 
		{
			(*new)->thlock = thlock;
		}
	#endif
	}
	else 
		(*new)->buffer = NULL;
	
	(*new)->is_pipe = 0;
	(*new)->timeout = -1;
	(*new)->ungetchar = -1;
	(*new)->eof_hit = 0;
	(*new)->filePtr = 0;
	(*new)->bufpos = 0;
	(*new)->dataRead = 0;
	(*new)->direction = 0;
	
	if (!(flag & APR_FILE_NOCLEANUP)) 
	{
		apr_pool_cleanup_register((*new)->pool, (void *)(*new), 
					lkl_unix_file_cleanup, 
					lkl_unix_child_file_cleanup);
	}
	return APR_SUCCESS;
}


apr_status_t lkl_file_close(lkl_file_t *file)
{
	 return apr_pool_cleanup_run(file->pool, file, lkl_unix_file_cleanup);
}

static apr_status_t lkl_wait_for_io_or_timeout(lkl_file_t *f, int for_read)
{
	struct pollfd pfd;
	int rc, timeout;

	timeout    = f->timeout / 1000;
	pfd.fd     = f->filedes;
	pfd.events = for_read ? POLLIN : POLLOUT;

	do 
	{
		rc = sys_poll(&pfd, 1, timeout);
	} 
	while (rc == -EINTR);
	if (!rc) 
		return APR_TIMEUP;
	else if (rc > 0) 
		return APR_SUCCESS;
	
	return -rc;
}

static apr_status_t file_read_buffered(lkl_file_t *thefile, void *buf,
				       apr_size_t *nbytes)
{
	apr_ssize_t rv;
	char *pos = (char *)buf;
	apr_uint64_t blocksize;
	apr_uint64_t size = *nbytes;

	if (thefile->direction == 1) 
	{
		rv = lkl_file_flush_locked(thefile);
		if (rv) 
			return rv;
		thefile->bufpos = 0;
		thefile->direction = 0;
		thefile->dataRead = 0;
	}

	rv = 0;
	if (thefile->ungetchar != -1) 
	{
		*pos = (char)thefile->ungetchar;
		++pos;
		--size;
		thefile->ungetchar = -1;
	}
	while (rv == 0 && size > 0) 
	{
		if (thefile->bufpos >= thefile->dataRead) 
		{
			int bytesread = sys_read(thefile->filedes, thefile->buffer, APR_FILE_BUFSIZE);
			if (bytesread == 0) 
			{
				thefile->eof_hit = 1;
				rv = APR_EOF;
				break;
			}
			else if (bytesread < 0) 
			{
				rv = -bytesread;
				break;
			}
			thefile->dataRead = bytesread;
			thefile->filePtr += thefile->dataRead;
			thefile->bufpos = 0;
		}

		blocksize = size > thefile->dataRead - thefile->bufpos ? thefile->dataRead - thefile->bufpos : size;
		memcpy(pos, thefile->buffer + thefile->bufpos, blocksize);
		thefile->bufpos += blocksize;
		pos += blocksize;
		size -= blocksize;
	}

	*nbytes = pos - (char *)buf;
	if (*nbytes) 
		rv = 0;
	
	return rv;
}

apr_status_t lkl_file_read(lkl_file_t *thefile, void *buf,
			   apr_size_t *nbytes)
{
	apr_ssize_t rv;
	apr_size_t bytes_read;

	if (*nbytes <= 0) 
	{
		*nbytes = 0;
		return APR_SUCCESS;
	}

	if (thefile->buffered) {
		file_lock(thefile);
		rv = file_read_buffered(thefile, buf, nbytes);
		file_unlock(thefile);

		return rv;
	}
	else 
	{
		bytes_read = 0;
		if (thefile->ungetchar != -1) 
		{
			bytes_read = 1;
			*(char *)buf = (char)thefile->ungetchar;
			buf = (char *)buf + 1;
			(*nbytes)--;
			thefile->ungetchar = -1;
			if (*nbytes == 0) 
			{
				*nbytes = bytes_read;
				return APR_SUCCESS;
			}
		}

		do 
		{
			rv = sys_read(thefile->filedes, buf, *nbytes);
		}
		while (rv == -EINTR);
// WAIT FOR IO
		if ((rv == -EAGAIN || rv == -EWOULDBLOCK) && thefile->timeout != 0) 
		{
			apr_status_t arv = lkl_wait_for_io_or_timeout(thefile, 1);
			if (arv != APR_SUCCESS) 
			{
				*nbytes = bytes_read;
				return arv;
			}
			else 
			{
				do 
				{
					rv = sys_read(thefile->filedes, buf, *nbytes);
				} 
				while (rv == -EINTR);
			}
		}  

		*nbytes = bytes_read;
		if (rv == 0) 
		{
			  thefile->eof_hit = 1;
			  return APR_EOF;
		}
		 if (rv > 0) 
		 {
			 *nbytes += rv;
		 	return APR_SUCCESS;
		}
		return rv;
	}
}

apr_status_t lkl_file_write(lkl_file_t *thefile, const void *buf,
			    apr_size_t *nbytes)
{
	apr_size_t rv;

	if (thefile->buffered) 
	{
		char *pos = (char *)buf;
		int blocksize;
		int size = *nbytes;

		file_lock(thefile);

		if ( thefile->direction == 0 ) 
		{
            // Position file pointer for writing at the offset we are 
		// logically reading from
	    //
			apr_int64_t offset = thefile->filePtr - thefile->dataRead + thefile->bufpos;
			if (offset != thefile->filePtr)
				sys_lseek(thefile->filedes, offset, SEEK_SET);
			thefile->bufpos = thefile->dataRead = 0;
			thefile->direction = 1;
		}

		rv = 0;
		while (rv == 0 && size > 0) 
		{
			if (thefile->bufpos == APR_FILE_BUFSIZE)   // write buffer is full
				rv = lkl_file_flush_locked(thefile);

			blocksize = size > APR_FILE_BUFSIZE - thefile->bufpos ? 
					APR_FILE_BUFSIZE - thefile->bufpos : size;
			memcpy(thefile->buffer + thefile->bufpos, pos, blocksize);
			thefile->bufpos += blocksize;
			pos += blocksize;
			size -= blocksize;
		}

		file_unlock(thefile);

		return rv;
	}
	else 
	{
		do 
		{
			rv = sys_write(thefile->filedes, buf, *nbytes);
		} 
		while (rv == -EINTR);
// USE WAIT FOR IO
		if ( (rv == -EAGAIN || rv == -EWOULDBLOCK) && thefile->timeout != 0) 
		{
			apr_status_t arv = lkl_wait_for_io_or_timeout(thefile, 0);
			if (arv != APR_SUCCESS) 
			{
				*nbytes = 0;
				return arv;
			}
			else 
			{
				do 
				{
					do 
					{
						rv = sys_write(thefile->filedes, buf, *nbytes);
					}
					while (rv == -EINTR);
					if ((rv == -EAGAIN || rv == -EWOULDBLOCK))
					{
						*nbytes /= 2; // yes, we'll loop if kernel lied
						// and we can't even write 1 byte
					}
					else 
					{
						break;
					}
				}
				while (1);
			}
		}  
		if (rv < 0)
		{
			(*nbytes) = 0;
			return -rv;
		}
		*nbytes = rv;
	}
	return APR_SUCCESS;
}

apr_status_t lkl_file_read_full(lkl_file_t *thefile, void *buf,
				 apr_size_t nbytes, apr_size_t *bytes_read)
{
	apr_status_t status;
	apr_size_t total_read = 0;

	do 
	{
		apr_size_t amt = nbytes;

		status = lkl_file_read(thefile, buf, &amt);
		buf = (char *)buf + amt;
		nbytes -= amt;
		total_read += amt;
	} 
	while (status == APR_SUCCESS && nbytes > 0);

	if (bytes_read != NULL)
		*bytes_read = total_read;

	return status;
}

apr_status_t lkl_file_write_full(lkl_file_t *thefile, const void *buf,
				apr_size_t nbytes, apr_size_t *bytes_written)
{
	apr_status_t status;
	apr_size_t total_written = 0;

	do
	{
		apr_size_t amt = nbytes;

		status = lkl_file_write(thefile, buf, &amt);
		buf = (char *)buf + amt;
		nbytes -= amt;
		total_written += amt;
	} 
	while (status == APR_SUCCESS && nbytes > 0);

	if (bytes_written != NULL)
		*bytes_written = total_written;

	return status;
}

static apr_status_t setptr(lkl_file_t *thefile, apr_off_t pos )
{
	apr_off_t newbufpos;
	apr_status_t rv;
	
	if (thefile->direction == 1) 
	{
		rv = lkl_file_flush_locked(thefile);
		if (rv)
			return rv;
		thefile->bufpos = thefile->direction = thefile->dataRead = 0;
	}
	
	newbufpos = pos - (thefile->filePtr - thefile->dataRead);
	if (newbufpos >= 0 && newbufpos <= thefile->dataRead) 
	{
		thefile->bufpos = newbufpos;
		rv = APR_SUCCESS;
	} 
	else 
	{
		rv = sys_lseek(thefile->filedes, pos, SEEK_SET);
		if (rv >= 0)
		{
			thefile->bufpos = thefile->dataRead = 0;
			thefile->filePtr = pos;
			rv = APR_SUCCESS;
		}
	}
	
	return -rv;
}


apr_status_t lkl_file_seek(lkl_file_t *thefile, apr_seek_where_t where, apr_off_t *offset)
{
	apr_off_t rv;
	
	thefile->eof_hit = 0;
	
	if (thefile->buffered) 
	{
		int rc = -EINVAL;
		apr_finfo_t finfo;
		
		file_lock(thefile);
		
		switch (where) 
		{
		case APR_SET:
			rc = setptr(thefile, *offset);
			break;
		
		case APR_CUR:
			rc = setptr(thefile, thefile->filePtr - thefile->dataRead + thefile->bufpos + *offset);
			break;
		
		case APR_END:
			rc = lkl_file_info_get_locked(&finfo, APR_FINFO_SIZE, thefile);
			if (rc == APR_SUCCESS)
			rc = setptr(thefile, finfo.size + *offset);
			break;
		}
	
		*offset = thefile->filePtr - thefile->dataRead + thefile->bufpos;
		
		file_unlock(thefile);
		
		return rc;
	}
	else 
	{
		rv = sys_lseek(thefile->filedes, *offset, where);
		if (rv < 0) 
		{
			*offset = -1;
			return -rv;
		}
		else 
		{
			*offset = rv;
			return APR_SUCCESS;
		}
	}
}

apr_status_t lkl_file_eof(lkl_file_t *fptr)
{
	if (fptr->eof_hit == 1) 
		return APR_EOF;
	return APR_SUCCESS;
}   


apr_status_t lkl_file_remove(const char *path, apr_pool_t *pool)
{
	apr_status_t ret;
	
	ret = sys_unlink(path);
	return -ret;
}

apr_status_t lkl_file_rename(const char *from_path, const char *to_path,
			apr_pool_t *pool)
{
	apr_status_t ret;
	
	ret = sys_rename(from_path, to_path);
	return -ret;
}

apr_status_t lkl_file_lock(lkl_file_t *thefile, int type)
{
	int rc;
	int ltype;

	if ((type & APR_FLOCK_TYPEMASK) == APR_FLOCK_SHARED)
		ltype = LOCK_SH;
	else
		ltype = LOCK_EX;
	if ((type & APR_FLOCK_NONBLOCK) != 0)
		ltype |= LOCK_NB;

	/* keep trying if flock() gets interrupted (by a signal) */
	while ((rc = sys_flock(thefile->filedes, ltype)) == -EINTR)
		continue;

	if (rc < 0)
		return -rc;
	return APR_SUCCESS;
}

apr_status_t lkl_file_unlock(lkl_file_t *thefile)
{
	int rc;
	
	while ((rc = sys_flock(thefile->filedes, LOCK_UN)) == -EINTR)
		continue;

	if (rc < 0)
		return -rc;
	return APR_SUCCESS;
}

#endif
