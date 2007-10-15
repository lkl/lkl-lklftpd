#ifndef _FILE_DISK_H
#define _FILE_DISK_H

int lkl_disk_do_rw(void *file, unsigned long sector, unsigned long nsect,
		    char *buffer, int dir);
unsigned long lkl_disk_get_sectors(void*);
int lkl_disk_add_disk(void *f, dev_t *devno);

#endif
