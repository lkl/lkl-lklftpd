#ifdef LKL_FILE_APIS

#include "fileops.h"


#define BUF_SIZE 4096


static apr_status_t dir_cleanup(void *thedir)
{
	lkl_dir_t *dir = thedir;
	apr_status_t rc;

	rc = lkl_sys_close(dir->fd);
	return rc;
}

apr_status_t lkl_dir_open(lkl_dir_t **new, const char *dirname,
                          apr_pool_t *pool)
{
	int dir = lkl_sys_open(dirname,O_RDONLY|O_DIRECTORY|O_LARGEFILE, 0);

	if (dir < 0)
		return APR_EINVAL;
	(*new) = (lkl_dir_t *) apr_palloc(pool, sizeof(lkl_dir_t));
	(*new)->pool = pool;
	(*new)->dirname = apr_pstrdup(pool, dirname);
	(*new)->fd = dir;
	(*new)->size = 0;
	(*new)->offset = 0;
	(*new)->entry = NULL;
	(*new)->data = (struct __kernel_dirent*) apr_pcalloc(pool,BUF_SIZE);
	apr_pool_cleanup_register((*new)->pool, *new, dir_cleanup,
                          apr_pool_cleanup_null);

return APR_SUCCESS;
}

apr_status_t lkl_dir_close(lkl_dir_t *thedir)
{
	return apr_pool_cleanup_run(thedir->pool, thedir, dir_cleanup);
}


apr_status_t lkl_dir_make(const char *path, apr_fileperms_t perm,
                          apr_pool_t *pool)
{
	apr_status_t rc;
	mode_t mode = lkl_unix_perms2mode(perm);

	rc = lkl_sys_mkdir(path, mode);
	return -rc;
}

apr_status_t lkl_dir_remove(const char *path, apr_pool_t *pool)
{
	apr_status_t rc;

	rc = lkl_sys_rmdir(path);
	return -rc;
}


struct __kernel_dirent * lkl_readdir(lkl_dir_t *thedir)
{
	struct __kernel_dirent * de;

	if(thedir->offset >= thedir->size)
	{
		/* We've emptied out our buffer.  Refill it.  */
		int bytes = lkl_sys_getdents(thedir->fd, thedir->data, BUF_SIZE);
		if(bytes <= 0)
			return NULL;
		thedir->size = bytes;
		thedir->offset = 0;
	}
	de = (struct __kernel_dirent*) ((char*) thedir->data+thedir->offset);
	thedir->offset += de->d_reclen;

	return de;
}

apr_status_t lkl_dir_read(apr_finfo_t * finfo, apr_int32_t wanted, lkl_dir_t * thedir)
{

	apr_status_t rc = 0;

    // We're about to call a non-thread-safe readdir()
	thedir->entry = lkl_readdir(thedir);
	if (NULL == thedir->entry)
		rc = APR_ENOENT;
	finfo->fname = NULL;

	if (rc)
	{
		finfo->valid = 0;
		return rc;
	}

	if (thedir->entry->d_ino && thedir->entry->d_ino != -1)
		wanted &= ~APR_FINFO_INODE;

	wanted &= ~APR_FINFO_NAME;

	if (wanted)
	{
		char fspec[APR_PATH_MAX];
		int off;
		apr_cpystrn(fspec, thedir->dirname, sizeof(fspec));
		off = strlen(fspec);
		if ((fspec[off - 1] != '/') && (off + 1 < sizeof(fspec)))
			fspec[off++] = '/';
		apr_cpystrn(fspec + off, thedir->entry->d_name, sizeof(fspec) - off);
		rc = lkl_stat(finfo, fspec, APR_FINFO_LINK | wanted, thedir->pool);
		// We passed a stack name that will disappear
		finfo->fname = NULL;
	}

	if (wanted && (APR_SUCCESS == rc || APR_INCOMPLETE == rc))
	{
		wanted &= ~finfo->valid;
	}
	else
	{
        // We don't bail because we fail to stat, when we are only -required-
	//	* to readdir... but the result will be APR_INCOMPLETE

		finfo->pool = thedir->pool;
		finfo->valid = 0;

		if (thedir->entry->d_ino && thedir->entry->d_ino != -1)
		{
			finfo->inode = thedir->entry->d_ino;
			finfo->valid |= APR_FINFO_INODE;
		}
	}

	finfo->name = apr_pstrdup(thedir->pool, thedir->entry->d_name);
	finfo->valid |= APR_FINFO_NAME;

	if (wanted)
		return APR_INCOMPLETE;

	return APR_SUCCESS;
}

#endif
