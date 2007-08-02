//here we implement the lkl-based file wrappers.
#ifdef LKL_FILE_APIS

#include "fileops.h"

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
		} while (-1 == written && errno == EINTR);
		if (written == -1) 
			rv = errno;
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

	if (thefile->buffered) {
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
	#ifndef WAITIO_USES_POLL
	if (NULL != file->pollset) 
	{
		int pollset_rv = apr_pollset_destroy(file->pollset);
		/* If the file close failed, return its error value,
		* not apr_pollset_destroy()'s.
		*/
		if (rv == APR_SUCCESS)
			rv = pollset_rv;
	}
	#endif /* !WAITIO_USES_POLL */
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
	#ifndef WAITIO_USES_POLL
	/* Start out with no pollset.  apr_wait_for_io_or_timeout() will
	* initialize the pollset if needed.
	*/
	(*new)->pollset = NULL;
	#endif
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

apr_status_t lkl_file_read(lkl_file_t *thefile, void *buf,
			   apr_size_t *nbytes)
{
	//TODO
	return APR_SUCCESS;
}

apr_status_t lkl_file_write(lkl_file_t *thefile, const void *buf,
			    apr_size_t *nbytes)
{
	//TODO
	return APR_SUCCESS;
}

apr_status_t lkl_file_write_full(lkl_file_t *thefile,
				 const void *buf,
     apr_size_t nbytes,
     apr_size_t *bytes_written)
{
	//TODO
	return APR_SUCCESS;
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
		if (rv >=0)
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
		int rc = EINVAL;
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
		if (rv <0) 
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
	while ((rc = sys_flock(thefile->filedes, ltype)) < 0 && errno == EINTR)
		continue;

	if (rc <0)
		return -rc;
	return APR_SUCCESS;
}

apr_status_t lkl_file_unlock(lkl_file_t *thefile)
{
	int rc;
	
	while ((rc = sys_flock(thefile->filedes, LOCK_UN)) < 0 && errno == EINTR)
		continue;

	if (rc <0)
		return -rc;
	return APR_SUCCESS;
}

#endif
