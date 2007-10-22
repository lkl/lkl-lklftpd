#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/console.h>

extern int write(int, const char*, unsigned);

static void console_write(struct console *con, const char *str, unsigned len)
{
	write(1, str, len);
}

static struct console console = {
	.name	= "stdio_console",
	.write	= console_write,	
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
};

static int __init console_init(void)
{
	register_console(&console);
	return 0;
}

late_initcall(console_init);


