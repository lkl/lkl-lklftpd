#ifdef LKL_FILE_APIS

#define __KERNEL__

#include <linux/stat.h>
#include <asm/stat.h>

#include "fileops.h"
#include "sys_declarations.h"


apr_fileperms_t lkl_unix_mode2perms(mode_t mode)
{
	apr_fileperms_t perms = 0;

	if (mode & S_ISUID)
		perms |= APR_USETID;
	if (mode & S_IRUSR)
		perms |= APR_UREAD;
	if (mode & S_IWUSR)
		perms |= APR_UWRITE;
	if (mode & S_IXUSR)
		perms |= APR_UEXECUTE;
	if (mode & S_ISGID)
		perms |= APR_GSETID;
	if (mode & S_IRGRP)
		perms |= APR_GREAD;
	if (mode & S_IWGRP)
		perms |= APR_GWRITE;
	if (mode & S_IXGRP)
		perms |= APR_GEXECUTE;
#ifdef S_ISVTX
	if (mode & S_ISVTX)
		perms |= APR_WSTICKY;
#endif
	if (mode & S_IROTH)
		perms |= APR_WREAD;
	if (mode & S_IWOTH)
		perms |= APR_WWRITE;
	if (mode & S_IXOTH)
		perms |= APR_WEXECUTE;

	return perms;
}

mode_t lkl_unix_perms2mode(apr_fileperms_t perms)
{
	mode_t mode = 0;

	if (perms & APR_USETID)
		mode |= S_ISUID;
	if (perms & APR_UREAD)
		mode |= S_IRUSR;
	if (perms & APR_UWRITE)
		mode |= S_IWUSR;
	if (perms & APR_UEXECUTE)
		mode |= S_IXUSR;
	if (perms & APR_GSETID)
		mode |= S_ISGID;
	if (perms & APR_GREAD)
		mode |= S_IRGRP;
	if (perms & APR_GWRITE)
		mode |= S_IWGRP;
	if (perms & APR_GEXECUTE)
		mode |= S_IXGRP;
#ifdef S_ISVTX
	if (perms & APR_WSTICKY)
		mode |= S_ISVTX;
#endif
	if (perms & APR_WREAD)
		mode |= S_IROTH;
	if (perms & APR_WWRITE)
		mode |= S_IWOTH;
	if (perms & APR_WEXECUTE)
		mode |= S_IXOTH;

	return mode;
}

static apr_filetype_e filetype_from_mode(mode_t mode)
{
	apr_filetype_e type;
	switch (mode & S_IFMT) 
	{
		case S_IFREG:
			type = APR_REG; break;
		case S_IFDIR:
			type = APR_DIR; break;
		case S_IFLNK:
			type = APR_LNK; break;
		case S_IFCHR:
			type = APR_CHR; break;
		case S_IFBLK:
			type = APR_BLK; break;
		case S_IFSOCK:
			type = APR_SOCK; break;
	default:
		if (S_ISFIFO(mode)) 
		{
			type = APR_PIPE;
		} 
		else if (S_ISSOCK(mode)) 
		{
			type = APR_SOCK;
		}
	 else
		type = APR_UNKFILE;
	}
	
	return type;
}


static void fill_out_finfo(apr_finfo_t *finfo, struct stat *info,apr_int32_t wanted)
{
	finfo->valid = APR_FINFO_MIN | APR_FINFO_IDENT | APR_FINFO_NLINK
			| APR_FINFO_OWNER | APR_FINFO_PROT;
	finfo->protection = lkl_unix_mode2perms(info->st_mode);
	finfo->filetype = filetype_from_mode(info->st_mode);
	// we don't need this for now
	//finfo->user = info->st_uid;
	//finfo->group = info->st_gid;
	finfo->size = info->st_size;
	finfo->inode = info->st_ino;
	finfo->device = info->st_dev;
	finfo->nlink = info->st_nlink;
	finfo->atime = (apr_time_t) info->st_atime * APR_USEC_PER_SEC;
	finfo->mtime = (apr_time_t) info->st_mtime * APR_USEC_PER_SEC;
	finfo->ctime = (apr_time_t) info->st_ctime * APR_USEC_PER_SEC;
}

apr_status_t lkl_file_info_get_locked(apr_finfo_t *finfo, apr_int32_t wanted,
                                      lkl_file_t *thefile)
{
	struct stat info;
	apr_status_t ret;

	if (thefile->buffered)
	{
		apr_status_t rv = lkl_file_flush_locked(thefile);
		if (rv != APR_SUCCESS)
			return rv;
	}
	ret = wrapper_sys_newfstat(thefile->filedes, &info);
	if (!ret)
	{
		finfo->pool = thefile->pool;
		finfo->fname = thefile->fname;
		fill_out_finfo(finfo, &info, wanted);
		return (wanted & ~finfo->valid) ? APR_INCOMPLETE : APR_SUCCESS;
	}
	return ret;
}


apr_status_t lkl_file_info_get(apr_finfo_t *finfo, apr_int32_t wanted, lkl_file_t *thefile)
{
	struct stat info;
	int ret;

	if (thefile->buffered)
	{
		apr_status_t rv = lkl_file_flush(thefile);
		if (rv != APR_SUCCESS)
		return rv;
	}
	ret = wrapper_sys_newfstat(thefile->filedes, &info);
	if (0 == ret)
	{
		finfo->pool = thefile->pool;
		finfo->fname = thefile->fname;
		fill_out_finfo(finfo, &info, wanted);
		return (wanted & ~finfo->valid) ? APR_INCOMPLETE : APR_SUCCESS;
	}
	return -ret;
}

apr_status_t lkl_file_perms_set(const char *fname, apr_fileperms_t perms)
{
	apr_status_t ret;
	mode_t mode = lkl_unix_perms2mode(perms);

	ret = wrapper_sys_chmod(fname, mode);
	if (ret)
		return -ret;

	return APR_SUCCESS;
}

apr_status_t lkl_file_attrs_set(const char *fname, apr_fileattrs_t attributes,
				apr_fileattrs_t attr_mask, apr_pool_t *pool)
{
	apr_status_t status;
	apr_finfo_t finfo;

	/* Don't do anything if we can't handle the requested attributes */
	if (!(attr_mask & (APR_FILE_ATTR_READONLY
			| APR_FILE_ATTR_EXECUTABLE)))
	return APR_SUCCESS;

	status = lkl_stat(&finfo, fname, APR_FINFO_PROT, pool);
	if (status)
		return status;

	/* ### TODO: should added bits be umask'd? */
	if (attr_mask & APR_FILE_ATTR_READONLY)
	{
		if (attributes & APR_FILE_ATTR_READONLY)
		{
			finfo.protection &= ~APR_UWRITE;
			finfo.protection &= ~APR_GWRITE;
			finfo.protection &= ~APR_WWRITE;
		}
		else
		{
			/* ### umask this! */
			finfo.protection |= APR_UWRITE;
			finfo.protection |= APR_GWRITE;
			finfo.protection |= APR_WWRITE;
		}
		}

		if (attr_mask & APR_FILE_ATTR_EXECUTABLE)
		{
		if (attributes & APR_FILE_ATTR_EXECUTABLE)
		{
			/* ### umask this! */
			finfo.protection |= APR_UEXECUTE;
			finfo.protection |= APR_GEXECUTE;
			finfo.protection |= APR_WEXECUTE;
		}
		else
		{
			finfo.protection &= ~APR_UEXECUTE;
			finfo.protection &= ~APR_GEXECUTE;
			finfo.protection &= ~APR_WEXECUTE;
		}
	}

	return lkl_file_perms_set(fname, finfo.protection);
}

apr_status_t lkl_file_mtime_set(const char *fname, apr_time_t mtime,
				apr_pool_t *pool)
{
	apr_status_t status;
	apr_finfo_t finfo;

	status = lkl_stat(&finfo, fname, APR_FINFO_ATIME, pool);
	if (status)
		return status;

	{
		struct timeval tvp[2];

		tvp[0].tv_sec = apr_time_sec(finfo.atime);
		tvp[0].tv_usec = apr_time_usec(finfo.atime);
		tvp[1].tv_sec = apr_time_sec(mtime);
		tvp[1].tv_usec = apr_time_usec(mtime);

		status = wrapper_sys_utimes(fname, tvp);
		if (status)
			return status;
	}
	return APR_SUCCESS;
}

apr_status_t lkl_stat(apr_finfo_t *finfo,const char *fname, apr_int32_t wanted, apr_pool_t *pool)
{
	struct stat info;
	int srv =0;

	memset(&info,0,sizeof(struct stat));
	if (wanted & APR_FINFO_LINK)
		srv = wrapper_sys_newlstat((char*)fname, &info);
	else
		srv = wrapper_sys_newstat((char*)fname, &info);

	if (0 == srv)
	{
		finfo->pool = pool;
		finfo->fname = fname;
		fill_out_finfo(finfo, &info, wanted);
		if (wanted & APR_FINFO_LINK)
			wanted &= ~APR_FINFO_LINK;
		return (wanted & ~finfo->valid) ? APR_INCOMPLETE : APR_SUCCESS;
	}
	return -srv;
}

#endif
