// implement the main() + config, pass control to listener
//#include <stdlib.h>
//#include <stdio.h>

#include <apr.h>
#include <apr_general.h>
#include <apr_errno.h>
#include <apr_atomic.h>

#include "utils.h"
#include "config.h"
#include "listen.h"

void ftpd_main(void);

#ifdef LKL_FILE_APIS

#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/dirent.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include "lkl_thread.h"

unsigned long irqs_enabled=0;

unsigned long __local_save_flags(void)
{
        return irqs_enabled;
}

void __local_irq_restore(unsigned long flags)
{
        irqs_enabled=flags;
}

void local_irq_enable(void)
{
        irqs_enabled=1;
}

void local_irq_disable(void)
{
        irqs_enabled=0;
}

void show_mem(void)
{
}

void __udelay(unsigned long usecs)
{

}

void __ndelay(unsigned long nsecs)
{

}


void lkl_console_write(const char *str, unsigned len)
{
        write(1, str, len);
}

extern int sbull_init(void);

void get_cmd_line(char **cl)
{
        static char x[] = "root=42:0";
        *cl=x;
}

long _panic_blink(long time)
{
        assert(1);
        return 0;
}

void _mem_init(unsigned long *phys_mem, unsigned long *phys_mem_size)
{
        *phys_mem_size=256*1024*1024;
        *phys_mem=(unsigned long) memalign(4096, *phys_mem_size);
}

int _sbull_open(void)
{
	int fd;
        fd=open("disk", O_RDWR);
	
	return fd;
}

unsigned long _sbull_sectors(void)
{
        unsigned long sectors;
        int fd=open("disk", O_RDONLY);

        assert(fd > 0);
        sectors=(lseek64(fd, 0, SEEK_END)/512);
        close(fd);

        return sectors;
}

void _sbull_transfer(int fd, unsigned long sector, unsigned long nsect, char *buffer, int dir)
{
	  assert(lseek64(fd, sector*512, SEEK_SET) >= 0);
        
        if (dir)
                assert(write(fd, buffer, nsect*512) == nsect*512);
        else
                assert(read(fd, buffer, nsect*512) == nsect*512);

}

extern void start_kernel(void);

int kernel_execve(const char *filename, char *const argv[], char *const envp[])
{
#if 0
        int i=1;

        printf("%s: %s ", __FUNCTION__, filename);
        while(argv[i]) {
                printf("%s ", argv[i]);
                fflush(stdout);
                i++;
        }
        printf("\n");

#endif

        if (strcmp(filename, "/bin/init") == 0) 
	{
		//int err;
		// remount the file system with write access
		//err = sys_mount("/dev/root", "/",NULL, MS_REMOUNT | MS_VERBOSE, NULL);
		//if(err !=0)
		//	lfd_log(LFD_ERROR, "sys_mount: errorcode %d errormsg %s", -err, lfd_apr_strerror_thunsafe(-err));
		ftpd_main();	
        }
        return -1; 
}
#endif

void ftpd_main(void)
{
	apr_pool_t * pool;
	apr_status_t rc;

	apr_pool_create(&pool, NULL);
	rc = lfd_config(pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "Config file wrong. Exiting");
		apr_pool_destroy(pool);
		return;
	}

	apr_atomic_init(root_pool);
	printf("Ftp server is running.\n");
	lfd_listen(pool);

	apr_pool_destroy(pool);
}

int main(int argc, char const *const * argv, char const *const * engv)
{
	apr_status_t rc;
	rc = apr_app_initialize(&argc, &argv, &engv);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "apr_initialize_app failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		return 1;
	}
	atexit(apr_terminate);

#ifndef LKL_FILE_APIS
	ftpd_main();
#else
	apr_pool_create(&root_pool, NULL);
	apr_thread_mutex_create(&kth_mutex,APR_THREAD_MUTEX_DEFAULT,root_pool);
        apr_thread_mutex_lock(kth_mutex);

	start_kernel();

	apr_thread_mutex_destroy(kth_mutex);
	apr_pool_destroy(root_pool);	
#endif
	return 0;
}
