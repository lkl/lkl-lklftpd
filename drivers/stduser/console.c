#include <stdio.h>

extern void _stdio_write(const char *str, int len)
{
	fwrite(str, len, 1, stdout);
}
