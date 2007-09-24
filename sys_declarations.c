#ifdef LKL_FILE_APIS

#include "sys_declarations.h"

//The Linux original names

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


//our wrappers
#include <apr_pools.h>
#include <apr_thread_mutex.h>
apr_thread_mutex_t	* barrier_mutex;
apr_pool_t 		* barrier_pool;
void barrier_init(void)
{
	apr_pool_create(&barrier_pool, NULL);
	apr_thread_mutex_create(&barrier_mutex, APR_THREAD_MUTEX_UNNESTED, barrier_pool);
}

void barrier_fini(void)
{
	apr_thread_mutex_destroy(barrier_mutex);
	apr_pool_destroy(barrier_pool);
}

void barrier_enter(void)
{
	apr_thread_mutex_lock(barrier_mutex);
}

void barrier_exit(void)
{
	apr_thread_mutex_unlock(barrier_mutex);
}

asmlinkage long wrapper_sys_sync(void)
{
	long l;
	barrier_enter();
	l = sys_sync();
	barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_mount(char __user *dev_name, char __user *dir_name,  char __user *type, unsigned long flags,    void __user *data){
	long l;
	barrier_enter();
	l = sys_mount(dev_name, dir_name,  type, flags,data);
	barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_reboot(int magic1, int magic2, unsigned int cmd,  void __user *arg){
	long l;
	barrier_enter();
	l = sys_reboot(magic1, magic2, cmd,  arg);
	barrier_exit();
	return l;
}
asmlinkage ssize_t wrapper_sys_write(unsigned int fd, const char __user *buf,    size_t count){
	ssize_t l;
	barrier_enter();
	l = sys_write(fd, buf,    count);
	barrier_exit();

	return l;
}
asmlinkage long wrapper_sys_close(unsigned int fd){
	long l;
	barrier_enter();
	l = sys_close(fd);
	barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_unlink(const char __user *pathname){
	long l;
	barrier_enter();
	l = sys_unlink(pathname);
	barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_open(const char __user *filename, int flags, int mode){
	long l;
	barrier_enter();
	l = sys_open(filename, flags, mode);
	barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_poll(struct pollfd __user *ufds, unsigned int nfds, long timeout){
	long l;
	barrier_enter();
	l = sys_poll(ufds, nfds, timeout);
	barrier_exit();
	return l;
}
asmlinkage ssize_t wrapper_sys_read(unsigned int fd, char __user *buf,  size_t count){
	ssize_t l;
	barrier_enter();
	l = sys_read(fd, buf,  count);
	barrier_exit();
	return l;
}
asmlinkage off_t wrapper_sys_lseek(unsigned int fd, off_t offset,  unsigned int origin){
	off_t l;
	barrier_enter();
	l = sys_lseek(fd, offset,  origin);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_rename(const char __user *oldname,  const char __user *newname){
	long l;
	barrier_enter();
	l = sys_rename(oldname,newname);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_flock(unsigned int fd, unsigned int cmd){
	long l;
	barrier_enter();
	l = sys_flock(fd, cmd);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_newfstat(unsigned int fd, struct stat __user *statbuf){
	long l;
	barrier_enter();
	l = sys_newfstat(fd, statbuf);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_chmod(const char __user *filename, mode_t mode){
	long l;
	barrier_enter();
	l = sys_chmod(filename, mode);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_newlstat(char __user *filename, struct stat __user *statbuf){
	long l;
	barrier_enter();
	l = sys_newlstat(filename, statbuf);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_mkdir(const char __user *pathname, int mode){
	long l;
	barrier_enter();
	l = sys_mkdir(pathname, mode);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_rmdir(const char __user *pathname){
	long l;
	barrier_enter();
	l = sys_rmdir(pathname);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_getdents(unsigned int fd, struct linux_dirent __user *dirent, unsigned int count){
	long l;
	barrier_enter();
	l = sys_getdents(fd, dirent, count);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_newstat(char __user *filename, struct stat __user *statbuf){
	long l;
	barrier_enter();
	l = sys_newstat(filename, statbuf);
 barrier_exit();
	return l;
}
asmlinkage long wrapper_sys_utimes(const char __user *filename, struct timeval __user *utimes){
	long l;
	barrier_enter();
	l = sys_utimes(filename, utimes);
 barrier_exit();
	return l;
}
#else//LKL_FILE_APIS

void barrier_init(void)
{
}

void barrier_fini(void)
{
}

#endif//LKL_FILE_APIS
