#ifndef LKLFTD__wrapper_sys_DECLARATIONS__H__
#define LKLFTD__wrapper_sys_DECLARATIONS__H__

#include <sys/types.h>
#include <sys/time.h>

struct pollfd;
struct stat;
struct linux_dirent;
long wrapper_sys_sync(void);
long wrapper_sys_mount(void *disk, char *fstype);
long wrapper_sys_reboot(int magic1, int magic2, unsigned int cmd,  void *arg);
ssize_t wrapper_sys_write(unsigned int fd, const char *buf,    size_t count);
long wrapper_sys_close(unsigned int fd);
long wrapper_sys_unlink(const char *pathname);
long wrapper_sys_open(const char *filename, int flags, int mode);
long wrapper_sys_poll(struct pollfd *ufds, unsigned int nfds, long timeout);
ssize_t wrapper_sys_read(unsigned int fd, char *buf,  size_t count);
off_t wrapper_sys_lseek(unsigned int fd, off_t offset,  unsigned int origin);
long wrapper_sys_rename(const char *oldname,  const char *newname);
long wrapper_sys_flock(unsigned int fd, unsigned int cmd);
long wrapper_sys_newfstat(unsigned int fd, struct stat *statbuf);
long wrapper_sys_chmod(const char *filename, mode_t mode);
long wrapper_sys_newlstat(char *filename, struct stat *statbuf);
long wrapper_sys_mkdir(const char *pathname, int mode);
long wrapper_sys_rmdir(const char *pathname);
long wrapper_sys_getdents(unsigned int fd, struct linux_dirent *dirent, unsigned int count);
long wrapper_sys_newstat(char *filename, struct stat *statbuf);
long wrapper_sys_utimes(const char *filename, struct timeval *utimes);

void barrier_init(void);
void barrier_fini(void);

#endif //LKLFTD__wrapper_sys_DECLARATIONS__H__
