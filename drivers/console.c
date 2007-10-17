#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/console.h>

extern int printf(const char *, ...);

static void console_write(struct console *con, const char *str, unsigned len)
{
	printf("%s", str);
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


