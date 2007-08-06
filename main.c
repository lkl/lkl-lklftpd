// implement the main() + config, pass control to listener
//#include <stdlib.h>
//#include <stdio.h>

#include <apr.h>
#include <apr_pools.h>
#include <apr_signal.h>
#include <apr_general.h>
#include <apr_errno.h>
#include <apr_atomic.h>

#include "sys_declarations.h"
#include "utils.h"
#include "config.h"
#include "listen.h"

volatile apr_uint32_t ftp_must_exit;


void ftpd_main(void);

#ifdef LKL_FILE_APIS

#include <stdlib.h>
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
		int err = 0;
		// remount the file system with write access
		err = sys_mount("/", "/",NULL, MS_REMOUNT | MS_VERBOSE, NULL);
		if(0 != err)
			lfd_log(LFD_ERROR, "sys_mount: errorcode %d errormsg %s", -err, lfd_apr_strerror_thunsafe(-err));
		ftpd_main();
		//we shouldn't exit this function because the kernel will panic.
        }
        return -1;
}
#endif


static void sig_func(int sigid)
{
	//whenever a "shutdown" signal is received (CTRL+C, SIGHUP, etc.) we set the ftp_must_exit flag.
	// this flag is periodically checked by the loop that accepts client connections.
	// if it sees it set it will stop receiving connections and begin a teardown of the kernel.
	apr_atomic_set32(&ftp_must_exit, 1);
}
apr_pool_t	* root_pool;
void ftpd_main(void)
{
	printf("Ftp server preparing to accept client connections.\n");
	lfd_listen(root_pool);
	printf("Ftp server is not running any more. Client connections will be obliterated.\n");
#ifdef LKL_FILE_APIS
	#define	LINUX_REBOOT_MAGIC1		0xfee1dead
	#define	LINUX_REBOOT_MAGIC2		672274793
	#define	LINUX_REBOOT_CMD_POWER_OFF	0x4321FEDC
	//nothing gets past this exit call; This is a hack currently used to stop the kernel from issuing a kernel panic.
	exit(0);
	sys_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
#endif
}


int main(int argc, char const *const * argv, char const *const * engv)
{
	apr_status_t	  rc;

	rc = apr_app_initialize(&argc, &argv, &engv);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "apr_initialize_app failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		return 1;
	}
	atexit(apr_terminate);

	rc = apr_pool_create(&root_pool, NULL);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "main's apr_pool_create failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
		return 2;
	}
	rc = lfd_config(root_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "Config file wrong. Exiting");
		return 3;
	}

	apr_atomic_init(root_pool);
	apr_atomic_set32(&ftp_must_exit, 0);

	apr_signal(SIGTERM, sig_func);
	apr_signal(SIGKILL, sig_func);
	apr_signal(SIGHUP,  sig_func);
	apr_signal(SIGINT,  sig_func);



#ifndef LKL_FILE_APIS
	ftpd_main();
#else //LKL_FILE_APIS
	rc = apr_pool_create(&lkl_thread_creator_pool, root_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "main: apr_pool_create failed with errorcode %d errormsg %s", rc, lfd_apr_strerror_thunsafe(rc));
	}

	apr_thread_mutex_create(&kth_mutex, APR_THREAD_MUTEX_DEFAULT, lkl_thread_creator_pool);
	apr_thread_mutex_lock(kth_mutex);

	start_kernel();

	apr_thread_mutex_destroy(kth_mutex);
	apr_pool_destroy(lkl_thread_creator_pool);
	apr_pool_destroy(root_pool);
#endif //LKL_FILE_APIS
	return 0;
}
