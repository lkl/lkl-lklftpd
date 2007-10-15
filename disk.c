#include <apr_file_io.h>

#include "drivers/disk.h"

unsigned long lkl_disk_get_sectors(void *file)
{
	apr_off_t offset=0;

	if (apr_file_seek((apr_file_t*)file, APR_SET, &offset) != APR_SUCCESS)
		return 0;

	/* Double check this, apr docs are fuzzy. */
	return offset;
}

int lkl_disk_do_rw(void *file, unsigned long sector, unsigned long nsect,
		   char *buffer, int dir)
{
	apr_off_t offset=sector*512;
	apr_status_t status;

	if (apr_file_seek((apr_file_t*)file, APR_SET, &offset) != APR_SUCCESS)
		return 0;

	if (dir)
		status=apr_file_write_full((apr_file_t*)file, buffer, nsect*512, NULL);
	else
		status=apr_file_read_full((apr_file_t*)file, buffer, nsect*512, NULL);

	return (status == APR_SUCCESS);
}



