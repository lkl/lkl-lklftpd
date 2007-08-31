#ifndef LKLFTD__SYS_DECLARATIONS__H__
#define LKLFTD__SYS_DECLARATIONS__H__

#include <sys/types.h>
#include <sys/time.h>

#define asmlinkage
#define __user
struct linux_dirent;
struct pollfd ;
struct stat ;
asmlinkage long sys_sync(void);
asmlinkage long sys_mount(char __user *dev_name, char __user *dir_name,  char __user *type, unsigned long flags,    void __user *data);
asmlinkage long sys_reboot(int magic1, int magic2, unsigned int cmd,  void __user *arg);
asmlinkage ssize_t sys_write(unsigned int fd, const char __user *buf,    size_t count);
asmlinkage long sys_close(unsigned int fd);
asmlinkage long sys_unlink(const char __user *pathname);
asmlinkage long sys_open(const char __user *filename, int flags, int mode);
asmlinkage long sys_poll(struct pollfd __user *ufds, unsigned int nfds, long timeout);
asmlinkage ssize_t sys_read(unsigned int fd, char __user *buf,  size_t count);
asmlinkage off_t sys_lseek(unsigned int fd, off_t offset,  unsigned int origin);
asmlinkage long sys_rename(const char __user *oldname,  const char __user *newname);
asmlinkage long sys_flock(unsigned int fd, unsigned int cmd);
asmlinkage long sys_newfstat(unsigned int fd, struct stat __user *statbuf);
asmlinkage long sys_chmod(const char __user *filename, mode_t mode);
asmlinkage long sys_newlstat(char __user *filename, struct stat __user *statbuf);
asmlinkage long sys_mkdir(const char __user *pathname, int mode);
asmlinkage long sys_rmdir(const char __user *pathname);
asmlinkage long sys_getdents(unsigned int fd, struct linux_dirent __user *dirent, unsigned int count);
asmlinkage long sys_newstat(char __user *filename, struct stat __user *statbuf);
asmlinkage long sys_utimes(const char __user *filename, struct timeval __user *utimes);

#endif //LKLFTD__SYS_DECLARATIONS__H__
