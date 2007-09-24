// implement the main() + config, pass control to listener
#include <stdlib.h>
#include <signal.h>

#include <apr.h>
#include <apr_pools.h>
#include <apr_signal.h>
#include <apr_general.h>
#include <apr_errno.h>
#include <apr_atomic.h>
#include <apr_getopt.h>

#include "sys_declarations.h"
#include "utils.h"
#include "config.h"
#include "listen.h"

volatile apr_uint32_t ftp_must_exit;


void ftpd_main(void);

#ifdef LKL_FILE_APIS

#include "include/asm/callbacks.h"

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




long linux_panic_blink(long time)
{
	assert(0);
	return 0;
}

static void *_phys_mem;

void linux_mem_init(unsigned long *phys_mem, unsigned long *phys_mem_size)
{
	*phys_mem_size=256*1024*1024;
	*phys_mem=(unsigned long)malloc(*phys_mem_size);
}

void linux_halt(void)
{
	free(_phys_mem);
}

extern void threads_init(struct linux_native_operations *lnops);

static struct linux_native_operations lnops = {
	.panic_blink = linux_panic_blink,
	.mem_init = linux_mem_init,
	.main = ftpd_main,
	.halt = linux_halt
};



#endif//LKL_FILE_APIS


static void sig_func(int sigid)
{
	//whenever a "shutdown" signal is received (CTRL+C, SIGHUP, etc.) we set the ftp_must_exit flag.
	// this flag is periodically checked by the loop that accepts client connections.
	// if it sees it set it will stop receiving connections and begin a teardown of the kernel.
	printf("a shutdown signal of value [%d] was received\n", sigid);
	apr_atomic_set32(&ftp_must_exit, 1);
}

apr_pool_t	* root_pool;
void ftpd_main(void)
{
	printf("Ftp server preparing to accept client connections.\n");
	lfd_listen(root_pool);
	printf("Ftp server is not running any more. Client connections will be obliterated.\n");

#ifdef LKL_FILE_APIS
	//flush them buffers!
	sys_sync();
	//nothing gets past this exit call; This is a hack currently used to stop the kernel from issuing a kernel panic.
	exit(0);
#define	LINUX_REBOOT_MAGIC1		0xfee1dead
#define	LINUX_REBOOT_MAGIC2		672274793
#define	LINUX_REBOOT_CMD_POWER_OFF	0x4321FEDC
	sys_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
#endif
}


static const apr_getopt_option_t opt_option[] = {
	/* long-option, short-option, has-arg flag, description */
	{ "port", 'p', TRUE, "port to listen on" },
	{ "help", 'h', 0, "display this help and exit" },
	{ NULL, 0, 0, NULL },
};

void show_help(const char *name)
{
	const struct apr_getopt_option_t *i=opt_option;

	printf("Usage: %s [ OPTIONS ] \n\n", name);
	while (i->name)
	{
		printf(" -%c, --%s \t %s\n", (char)i->optch, i->name, i->description);
		i++;
	}
	printf("\nReport bugs to lkl-dev@ixlabs.cs.pub.ro\n");
	exit(0);
}

static int parse_command_line(int argc, char const *const * argv)
{
	int rv, optch;
	const char *optarg;

	apr_getopt_t *opt;

	apr_getopt_init(&opt, root_pool, argc, argv);

	while (APR_SUCCESS == (rv = apr_getopt_long(opt, opt_option, &optch, &optarg)))
	{
		switch (optch)
		{
		case 'p':
			lfd_config_listen_port=atoi(optarg);
			lfd_config_data_port=0;
			break;
		case 'h':
			show_help(argv[0]);
			break;
		}
	}

	if (APR_EOF != rv)
	{
		printf("Try `%s --help` for more information.\n", argv[0]);
		return -1;
	}

	return 0;
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

	if (parse_command_line(argc, argv) != 0)
		return 3;

	rc = lfd_config(root_pool);
	if(APR_SUCCESS != rc)
	{
		lfd_log(LFD_ERROR, "Config file wrong. Exiting");
		return 4;
	}

	apr_atomic_init(root_pool);
	apr_atomic_set32(&ftp_must_exit, 0);

	apr_signal(SIGTERM, sig_func);
#ifdef SIGKILL
	apr_signal(SIGKILL, sig_func);
#endif//SIGKILL
#ifdef SIGHUP
	apr_signal(SIGHUP,  sig_func);
#endif//SIGHUP
	apr_signal(SIGINT,  sig_func);


	barrier_init();

#ifndef LKL_FILE_APIS
	ftpd_main();
#else //LKL_FILE_APIS

	threads_init(&lnops);
	linux_start_kernel(&lnops, "root=%d:0", FILE_DISK_MAJOR);

#endif //LKL_FILE_APIS

	barrier_fini();
	return 0;
}
