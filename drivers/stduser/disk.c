#include <stdio.h>
#include <assert.h>

#include "lkl/file_disk.h"

void* _file_open(void)
{
        return fopen("disk", "r+b");
}

unsigned long _file_sectors(void)
{
        long sectors;
        FILE* f=fopen("disk", "r");
	int x;

        assert(f > 0);
        x=fseek(f, 0, SEEK_END);
	assert(x == 0);
	sectors=ftell(f);
	assert(sectors >= 0);

        return sectors/512;
}

void _file_rw(void *_f, unsigned long sector, unsigned long nsect, char *buffer, int dir)
{
	int x;
	FILE *f=(FILE*)_f;

        x=fseek(f, sector*512, SEEK_SET);
	assert(x == 0);
        if (dir)
                x=fwrite(buffer, 512, nsect, f);
        else
                x=fread(buffer, 512, nsect, f);
	assert(x == nsect);
}

