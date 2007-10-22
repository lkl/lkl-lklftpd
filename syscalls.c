#include <asm/page.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <asm/unistd.h>
#include <linux/fs.h>
#include <asm/callbacks.h>

#include <malloc.h>
#include <string.h>

#include "syscall_helpers.h"


#define SYSCALL_REQ(_syscall, _params...) \
{ \
	.syscall = __NR_##_syscall, \
	.params = { _params }, \
        .done = syscall_done \
}

#define DO_SYSCALL \
	syscall_enter(); \
	linux_trigger_irq_with_data(SYSCALL_IRQ, &sr); \
	syscall_exit(); \
	return sr.ret; \



long wrapper_sys_sync(void)
{
	struct syscall_req sr = SYSCALL_REQ(sync);
	DO_SYSCALL;
}

long wrapper_sys_reboot(int magic1, int magic2, unsigned int cmd,  void *arg)
{
	struct syscall_req sr = SYSCALL_REQ(reboot, magic1, magic2, cmd, (long)arg);
	DO_SYSCALL;
}

ssize_t wrapper_sys_write(unsigned int fd, const char *buf, size_t count)
{
	struct syscall_req sr = SYSCALL_REQ(write, fd, (long)buf, count);
	DO_SYSCALL;
}

long wrapper_sys_close(unsigned int fd)
{
	struct syscall_req sr = SYSCALL_REQ(close, fd);
	DO_SYSCALL;
}

long wrapper_sys_unlink(const char *pathname)
{
	struct syscall_req sr = SYSCALL_REQ(unlink, (long)pathname);
	DO_SYSCALL;
}

long wrapper_sys_open(const char *filename, int flags, int mode)
{
	struct syscall_req sr = SYSCALL_REQ(open, (long)filename, flags, mode);
	DO_SYSCALL;
}

long wrapper_sys_poll(struct pollfd *ufds, unsigned int nfds, long timeout)
{
	struct syscall_req sr = SYSCALL_REQ(poll, (long)ufds, nfds, timeout);
	DO_SYSCALL;
}

ssize_t wrapper_sys_read(unsigned int fd, char *buf, size_t count)
{
	struct syscall_req sr = SYSCALL_REQ(read, fd, (long)buf, count);
	DO_SYSCALL;
}

off_t wrapper_sys_lseek(unsigned int fd, off_t offset, unsigned int origin)
{
	struct syscall_req sr = SYSCALL_REQ(lseek, fd, offset, origin);
	DO_SYSCALL;
}

long wrapper_sys_rename(const char *oldname, const char *newname)
{
	struct syscall_req sr = SYSCALL_REQ(rename, (long)oldname, (long)newname);
	DO_SYSCALL;
}

long wrapper_sys_flock(unsigned int fd, unsigned int cmd)
{
	struct syscall_req sr = SYSCALL_REQ(flock, fd, cmd);
	DO_SYSCALL;
}

long wrapper_sys_newfstat(unsigned int fd, struct stat *statbuf)
{
	struct syscall_req sr = SYSCALL_REQ(newfstat, fd, (long)statbuf);
	DO_SYSCALL;
}

long wrapper_sys_chmod(const char *filename, mode_t mode)
{
	struct syscall_req sr = SYSCALL_REQ(chmod, (long)filename, mode);
	DO_SYSCALL;
}

long wrapper_sys_newlstat(char *filename, struct stat *statbuf)
{
	struct syscall_req sr = SYSCALL_REQ(newlstat, (long)filename, (long)statbuf);
	DO_SYSCALL;
}

long wrapper_sys_mkdir(const char *pathname, int mode)
{
	struct syscall_req sr = SYSCALL_REQ(mkdir, (long)pathname, mode);
	DO_SYSCALL;
}

long wrapper_sys_rmdir(const char *pathname)
{
	struct syscall_req sr = SYSCALL_REQ(rmdir, (long)pathname);
	DO_SYSCALL;
}

long wrapper_sys_getdents(unsigned int fd, struct linux_dirent *dirent, unsigned int count)
{
	struct syscall_req sr = SYSCALL_REQ(getdents, fd, (long)dirent, count);
	DO_SYSCALL;
}

long wrapper_sys_newstat(char *filename, struct stat *statbuf)
{
	struct syscall_req sr = SYSCALL_REQ(newstat, (long)filename, (long)statbuf);
	DO_SYSCALL;
}

long wrapper_sys_utimes(const char *filename, struct timeval *utimes)
{
	struct syscall_req sr = SYSCALL_REQ(utime, (long)filename, (long)utimes);
	DO_SYSCALL;
}

long _wrapper_sys_mount(const char *dev, const char *mnt_point, const char *fs, int flags, void *data)
{
	struct syscall_req sr= SYSCALL_REQ(mount, (long)dev, (long)mnt_point, (long)fs, flags, (long)data);
	DO_SYSCALL;
}


long wrapper_sys_chdir(const char *dir)
{
	struct syscall_req sr= SYSCALL_REQ(chdir, (long)dir);
	DO_SYSCALL;
}


long wrapper_sys_mknod(const char *filename, int mode, unsigned dev)
{
	struct syscall_req sr= SYSCALL_REQ(mknod, (long)filename, mode, dev);
	DO_SYSCALL;
}


long wrapper_sys_chroot(const char *dir)
{
	struct syscall_req sr= SYSCALL_REQ(chroot, (long)dir);
	DO_SYSCALL;
}


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
		return _wrapper_sys_mount(devno_str, mnt, fstype, flags, data);
		
	fs_names=malloc(PAGE_SIZE);
	get_fs_names(fs_names);
retry:
	for (p = fs_names; *p; p += strlen(p)+1) {
		err = _wrapper_sys_mount(devno_str, mnt, p, flags, data);
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


long wrapper_sys_mount(void *file, int devno, char *fstype)
{
	char devno_str[] = { "/dev/xxxxxxxxxxxxxxxx" };
	char mnt[] = { "/mnt/xxxxxxxxxxxxxxxx" };
	
	/* create /dev/dev */
	snprintf(devno_str, sizeof(devno_str), "/dev/%016x", devno);
	if (wrapper_sys_mknod(devno_str, S_IFBLK|0600, devno)) 
		goto out_error;

	/* create /mnt/filename */ 
	sprintf(mnt, "/mnt/%016x", devno);
	if (wrapper_sys_mkdir(mnt, 0700))
		goto out_del_dev;

	if (try_mount(fstype, devno_str, mnt, MS_RDONLY, 0))
		goto out_del_mnt_dir;

	wrapper_sys_chdir(mnt);
        _wrapper_sys_mount(".", "/", NULL, MS_MOVE, NULL);
        wrapper_sys_chroot(".");
		
	return 0;

out_del_mnt_dir:
	wrapper_sys_unlink(mnt);
out_del_dev:
	wrapper_sys_unlink(devno_str);
out_error:
	return -1;
}
