#ifndef LKLOPS_H__
#define LKLOPS_H__

#define LKL_FINI_DONT_UMOUNT_ROOT 1

void lkl_init(int (*)(void));
void lkl_fini(unsigned int flag);

#endif

