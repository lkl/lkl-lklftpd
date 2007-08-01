//here we implement the lkl-based file wrappers.
#ifdef LKL_FILE_APIS

#define __KERNEL__

#include <linux/stat.h>
#include <asm/stat.h>

#include "fileops.h"
#include <apr_poll.h>



struct lkl_file_t {
	apr_pool_t *pool;
	int filedes;
	char *fname;
	apr_int32_t flags;
	int eof_hit;
	int is_pipe;
	apr_interval_time_t timeout;
	int buffered;
	enum {BLK_UNKNOWN, BLK_OFF, BLK_ON } blocking;
	int ungetchar;    /* Last char provided by an unget op. (-1 = no char)*/
#ifndef WAITIO_USES_POLL
	/* if there is a timeout set, then this pollset is used */
	apr_pollset_t *pollset;
#endif
	/* Stuff for buffered mode */
	char *buffer;
	int bufpos;               /* Read/Write position in buffer */
	unsigned long dataRead;   /* amount of valid data read into buffer */
	int direction;            /* buffer being used for 0 = read, 1 = write */
	apr_off_t filePtr;        /* position in file of handle */
#if APR_HAS_THREADS
	struct apr_thread_mutex_t *thlock;
#endif
};

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

apr_status_t lkl_file_open(lkl_file_t **newf, const char *fname,
			   apr_int32_t flag, apr_fileperms_t perm,
      apr_pool_t *pool)
{
	//TODO
	return APR_SUCCESS;
}

apr_status_t lkl_file_cleanup(void *thefile)
{
	//TODO
	return APR_SUCCESS;
}

apr_status_t lkl_file_close(lkl_file_t *file)
{
	return apr_pool_cleanup_run(file->pool, file,lkl_file_cleanup);
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

apr_status_t lkl_file_read(lkl_file_t *thefile, void *buf,
			   apr_size_t *nbytes)
{
	return APR_SUCCESS;
}

apr_status_t lkl_file_write(lkl_file_t *thefile, const void *buf,
			    apr_size_t *nbytes)
{
	return APR_SUCCESS;
}

apr_status_t lkl_file_write_full(lkl_file_t *thefile,
				 const void *buf,
     apr_size_t nbytes,
     apr_size_t *bytes_written)
{
	return APR_SUCCESS;
}

apr_status_t lkl_file_seek(lkl_file_t *thefile, apr_seek_where_t where, apr_off_t *offset)
{
	return APR_SUCCESS;
}




static apr_filetype_e filetype_from_mode(mode_t mode)
{
	apr_filetype_e type;
	switch (mode & S_IFMT) {
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
#if defined(S_IFFIFO)
		case S_IFFIFO:
			type = APR_PIPE; break;
#endif
#if !defined(BEOS) && defined(S_IFSOCK)
		case S_IFSOCK:
			type = APR_SOCK; break;
#endif
	default:
#if !defined(S_IFFIFO) && defined(S_ISFIFO)
			if (S_ISFIFO(mode)) {
				type = APR_PIPE;
			} else
#endif
#if !defined(BEOS) && !defined(S_IFSOCK) && defined(S_ISSOCK)
				if (S_ISSOCK(mode)) {
					type = APR_SOCK;
				} else
#endif
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
	finfo->user = info->st_uid;
	finfo->group = info->st_gid;
	finfo->size = info->st_size;
	finfo->inode = info->st_ino;
	finfo->device = info->st_dev;
	finfo->nlink = info->st_nlink;
	apr_time_ansi_put(&finfo->atime, info->st_atime);
	apr_time_ansi_put(&finfo->mtime, info->st_mtime);
	apr_time_ansi_put(&finfo->ctime, info->st_ctime);
	
}

apr_status_t lkl_stat(apr_finfo_t *finfo,const char *fname, apr_int32_t wanted, apr_pool_t *pool)
{
	struct stat info;
	int srv =0;
	
	memset(&info,0,sizeof(struct stat));
	if (wanted & APR_FINFO_LINK)
		srv = sys_newlstat(fname, &info);
	else
		srv = sys_newstat(fname, &info);
	printf("%s :%o %d %d\n",fname, info.st_mode, info.st_size, info.st_ino);
	if (srv == 0) 
	{
		finfo->pool = pool;
		finfo->fname = fname;
		fill_out_finfo(finfo, &info, wanted);
		if (wanted & APR_FINFO_LINK)
			wanted &= ~APR_FINFO_LINK;
		return (wanted & ~finfo->valid) ? APR_INCOMPLETE : APR_SUCCESS;
	}
/*	else 
	{
#if !defined(ENOENT) || !defined(ENOTDIR)
#if !defined(ENOENT)
	return APR_ENOENT;
#else
	if ((-srv) != ENOENT)
		return APR_ENOENT;
	else
		return -srv;
#endif
#else
	return -srv;
#endif
	} */
	return -srv;
} 

#endif
