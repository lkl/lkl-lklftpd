#include <apr_file_io.h>

#include "drivers/disk.h"

unsigned long lkl_disk_get_sectors(void *file)
{
	apr_finfo_t finfo;


	if (apr_file_info_get(&finfo, APR_FINFO_SIZE, (apr_file_t*)file) != APR_SUCCESS)
		return 0;

	return finfo.size/512;
}

int lkl_disk_do_rw(void *_file, unsigned long sector, unsigned long nsect,
		   char *buffer, int dir)
{
	apr_off_t offset=sector*512;
	apr_size_t len=nsect*512;
	apr_file_t *file=(apr_file_t*)_file;
	apr_status_t status;

	if (apr_file_seek(file, APR_SET, &offset) != APR_SUCCESS)
		return 0;

	if (dir) 
		status=apr_file_write_full(file, buffer, len, NULL);
	else
		status=apr_file_read_full(file, buffer, len, NULL);

	return (status == APR_SUCCESS);
}



