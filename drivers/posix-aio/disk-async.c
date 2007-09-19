#include <stdio.h>
#include <assert.h>
#include <aio.h>
#include <malloc.h>
#include <string.h>
#include "../../include/asm/callbacks.h"

#include "lkl/file_disk-async.h"

void aio_notify(sigval_t sv)
{
	struct completion_status *cs=(struct completion_status*)sv.sival_ptr;
	struct aiocb *aio=(struct aiocb*)cs->native_cookie;

	if (aio_error(aio) == 0)
		cs->status=1;
	else
		cs->status=0;
	linux_trigger_irq_with_data(FILE_DISK_IRQ, cs);
}

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

        return sectors;
}

void _file_rw_async(void *_f, unsigned long sector, unsigned long nsect, char *buffer,
		    int dir, struct completion_status *cs)
{
	int x;
	FILE *f=(FILE*)_f;
	struct aiocb *aio=malloc(sizeof(*aio));

	assert(aio != NULL);

	memset(aio, 0, sizeof(*aio));
	aio->aio_fildes=fileno(f);
	aio->aio_offset=sector*512;
	aio->aio_buf=buffer;
	aio->aio_nbytes=nsect*512;
	aio->aio_sigevent.sigev_notify=SIGEV_THREAD;
	aio->aio_sigevent.sigev_notify_function=aio_notify;
	aio->aio_sigevent.sigev_value.sival_ptr=cs;
	cs->native_cookie=aio;

	if (dir)
		x=aio_write(aio);
	else
		x=aio_read(aio);

	assert(x == 0);
}

