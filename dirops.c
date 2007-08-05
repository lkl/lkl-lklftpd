#ifdef LKL_FILE_APIS

#include "fileops.h"
#include "sys_declarations.h"

#define BUF_SIZE 4096


static apr_status_t dir_cleanup(void *thedir)
{
	lkl_dir_t *dir = thedir;
	apr_status_t ret;

	ret = sys_close(dir->fd);
	return ret;
}

apr_status_t lkl_dir_open(lkl_dir_t **new, const char *dirname,
                          apr_pool_t *pool)
{
	int dir = sys_open(dirname,O_RDONLY|O_DIRECTORY|O_LARGEFILE, 0);

	if (dir < 0)
		return APR_EINVAL;
	(*new) = (lkl_dir_t *) apr_palloc(pool, sizeof(lkl_dir_t));
	(*new)->pool = pool;
	(*new)->dirname = apr_pstrdup(pool, dirname);
	(*new)->fd = dir;
	(*new)->size = 0;
	(*new)->offset = 0;
	(*new)->entry = NULL;
	(*new)->data = (struct linux_dirent*) apr_pcalloc(pool,BUF_SIZE);
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
	apr_status_t ret;
	mode_t mode = lkl_unix_perms2mode(perm);

	ret = sys_mkdir(path, mode);
	return -ret;
}

apr_status_t lkl_dir_remove(const char *path, apr_pool_t *pool)
{
	apr_status_t ret;

	ret = sys_rmdir(path);
	return -ret;
}

#ifdef DIRENT_TYPE
static apr_filetype_e filetype_from_dirent_type(int type)
{
	switch (type)
	{
		case DT_REG:
			return APR_REG;
		case DT_DIR:
			return APR_DIR;
		case DT_LNK:
			return APR_LNK;
		case DT_CHR:
			return APR_CHR;
		case DT_BLK:
			return APR_BLK;
#if defined(DT_FIFO)
		case DT_FIFO:
			return APR_PIPE;
#endif
#if !defined(BEOS) && defined(DT_SOCK)
		case DT_SOCK:
			return APR_SOCK;
#endif
		default:
			return APR_UNKFILE;
	}
}
#endif

struct dirent * lkl_readdir(lkl_dir_t *thedir)
{
	struct dirent * de;

	if(thedir->offset >= thedir->size)
	{
		/* We've emptied out our buffer.  Refill it.  */
		int bytes = sys_getdents(thedir->fd, thedir->data, BUF_SIZE);
		if(bytes <= 0)
			return NULL;
		thedir->size = bytes;
		thedir->offset = 0;
	}
	de = (struct dirent*) ((char*) thedir->data+thedir->offset);
	thedir->offset += de->d_reclen;

	return de;
}

apr_status_t lkl_dir_read(apr_finfo_t * finfo, apr_int32_t wanted, lkl_dir_t * thedir)
{

	apr_status_t ret = 0;
#ifdef DIRENT_TYPE
	apr_filetype_e type;
#endif

    // We're about to call a non-thread-safe readdir()
	thedir->entry = lkl_readdir(thedir);
	if (NULL == thedir->entry)
		ret = APR_ENOENT;
	finfo->fname = NULL;

	if (ret)
	{
		finfo->valid = 0;
		return ret;
	}

#ifdef DIRENT_TYPE
	type = filetype_from_dirent_type(thedir->entry->DIRENT_TYPE);
	if (APR_UNKFILE != type)
		wanted &= ~APR_FINFO_TYPE;
#endif
#ifdef DIRENT_INODE
	if (thedir->entry->DIRENT_INODE && thedir->entry->DIRENT_INODE != -1)
		wanted &= ~APR_FINFO_INODE;

#endif

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
		ret = lkl_stat(finfo, fspec, APR_FINFO_LINK | wanted, thedir->pool);
		// We passed a stack name that will disappear
		finfo->fname = NULL;
	}

	if (wanted && (APR_SUCCESS == ret || APR_INCOMPLETE == ret))
	{
		wanted &= ~finfo->valid;
	}
	else
	{
        // We don't bail because we fail to stat, when we are only -required-
	//	* to readdir... but the result will be APR_INCOMPLETE

		finfo->pool = thedir->pool;
		finfo->valid = 0;
#ifdef DIRENT_TYPE
		if (APR_UNKFILE != type)
		{
			finfo->filetype = type;
			finfo->valid |= APR_FINFO_TYPE;
		}
#endif
#ifdef DIRENT_INODE
		if (thedir->entry->DIRENT_INODE && thedir->entry->DIRENT_INODE != -1)
		{
			finfo->inode = thedir->entry->DIRENT_INODE;
			finfo->valid |= APR_FINFO_INODE;
		}
#endif
	}

	finfo->name = apr_pstrdup(thedir->pool, thedir->entry->d_name);
	finfo->valid |= APR_FINFO_NAME;

	if (wanted)
		return APR_INCOMPLETE;

	return APR_SUCCESS;
}

#endif
