#include <asm/page.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <asm/unistd.h>
#include <linux/fs.h>


#include <malloc.h>
#include <string.h>

#include "drivers/disk.h"
#include "barrier.h"

int get_filesystem_list(char * buf);

static void get_fs_names(char *page)
{
        char *s = page;
        int len = get_filesystem_list(page);
        char *p, *next;

        page[len] = '\0';
        for (p = page-1; p; p = next) {
                next = strchr(++p, '\n');
                if (*p++ != '\t')
                        continue;
                while ((*s++ = *p++) != '\n')
                        ;
                s[-1] = '\0';
        }

        *s = '\0';
}

static int try_mount(char *fstype, char *devno_str, char *mnt, int flags, void *data)
{
	int err;
	char *p, *fs_names;

	if (fstype)
		return sys_safe_mount(devno_str, mnt, fstype, flags, data);
		
	fs_names=malloc(PAGE_SIZE);
	get_fs_names(fs_names);
retry:
	for (p = fs_names; *p; p += strlen(p)+1) {
		err = sys_safe_mount(devno_str, mnt, p, flags, data);
		switch (err) {
			case 0:
				goto out;
			case -EACCES:
				flags |= MS_RDONLY;
				goto retry;
			case -EINVAL:
				continue;
		}
	}
out:
	free(fs_names);

	return err;
}

long wrapper_sys_mount(void *file, char *fstype)
{
	dev_t devno;
	char devno_str[] = { "/dev/xxxxxxxxxxxxxxxx" };
	char mnt[] = { "/mnt/xxxxxxxxxxxxxxxx" };
	
	if (lkl_disk_add_disk(file, &devno)) 
		goto out_error;

	/* create /dev/dev */
	snprintf(devno_str, sizeof(devno_str), "/dev/%016x", devno);
	if (sys_mknod(devno_str, S_IFBLK|0600, devno)) 
		goto out_error;

	/* create /mnt/filename */ 
	sprintf(mnt, "/mnt/%016x", devno);
	if (sys_mkdir(mnt, 0700))
		goto out_del_dev;

	if (try_mount(fstype, devno_str, mnt, MS_RDONLY, 0))
		goto out_del_mnt_dir;

	sys_chdir(mnt);
        sys_safe_mount(".", "/", NULL, MS_MOVE, NULL);
        sys_chroot(".");
		
	return 0;

out_del_mnt_dir:
	sys_unlink(mnt);
out_del_dev:
	sys_unlink(devno_str);
out_error:
	return -1;
}


long wrapper_sys_sync(void)
{
	long l;
	barrier_enter();
	l = sys_sync();
	barrier_exit();
	return l;
}

long wrapper_sys_reboot(int magic1, int magic2, unsigned int cmd,  void *arg)
{
	long l;
	barrier_enter();
	l = sys_reboot(magic1, magic2, cmd,  arg);
	barrier_exit();
	return l;
}

ssize_t wrapper_sys_write(unsigned int fd, const char *buf,    size_t count)
{
	ssize_t l;
	barrier_enter();
	l = sys_write(fd, buf,    count);
	barrier_exit();

	return l;
}

long wrapper_sys_close(unsigned int fd)
{
	long l;
	barrier_enter();
	l = sys_close(fd);
	barrier_exit();
	return l;
}

long wrapper_sys_unlink(const char *pathname)
{
	long l;
	barrier_enter();
	l = sys_unlink(pathname);
	barrier_exit();
	return l;
}

long wrapper_sys_open(const char *filename, int flags, int mode)
{
	long l;
	barrier_enter();
	l = sys_open(filename, flags, mode);
	barrier_exit();
	return l;
}

long wrapper_sys_poll(struct pollfd *ufds, unsigned int nfds, long timeout)
{
	long l;
	barrier_enter();
	l = sys_poll(ufds, nfds, timeout);
	barrier_exit();
	return l;
}

ssize_t wrapper_sys_read(unsigned int fd, char *buf,  size_t count)
{
	ssize_t l;
	barrier_enter();
	l = sys_read(fd, buf,  count);
	barrier_exit();
	return l;
}

off_t wrapper_sys_lseek(unsigned int fd, off_t offset,  unsigned int origin)
{
	off_t l;
	barrier_enter();
	l = sys_lseek(fd, offset,  origin);
	barrier_exit();
	return l;
}

long wrapper_sys_rename(const char *oldname,  const char *newname)
{
	long l;
	barrier_enter();
	l = sys_rename(oldname,newname);
	barrier_exit();
	return l;
}

long wrapper_sys_flock(unsigned int fd, unsigned int cmd)
{
	long l;
	barrier_enter();
	l = sys_flock(fd, cmd);
	barrier_exit();
	return l;
}

long wrapper_sys_newfstat(unsigned int fd, struct stat *statbuf)
{
	long l;
	barrier_enter();
	l = sys_newfstat(fd, statbuf);
	barrier_exit();
	return l;
}

long wrapper_sys_chmod(const char *filename, mode_t mode)
{
	long l;
	barrier_enter();
	l = sys_chmod(filename, mode);
	barrier_exit();
	return l;
}

long wrapper_sys_newlstat(char *filename, struct stat *statbuf)
{
	long l;
	barrier_enter();
	l = sys_newlstat(filename, statbuf);
	barrier_exit();
	return l;
}

long wrapper_sys_mkdir(const char *pathname, int mode)
{
	long l;
	barrier_enter();
	l = sys_mkdir(pathname, mode);
	barrier_exit();
	return l;
}

long wrapper_sys_rmdir(const char *pathname)
{
	long l;
	barrier_enter();
	l = sys_rmdir(pathname);
	barrier_exit();
	return l;
}

long wrapper_sys_getdents(unsigned int fd, struct linux_dirent *dirent, unsigned int count)
{
	long l;
	barrier_enter();
	l = sys_getdents(fd, dirent, count);
	barrier_exit();
	return l;
}

long wrapper_sys_newstat(char *filename, struct stat *statbuf)
{
	long l;
	barrier_enter();
	l = sys_newstat(filename, statbuf);
	barrier_exit();
	return l;
}

long wrapper_sys_utimes(const char *filename, struct timeval *utimes)
{
	long l;
	barrier_enter();
	l = sys_utimes(filename, utimes);
	barrier_exit();
	return l;
}

