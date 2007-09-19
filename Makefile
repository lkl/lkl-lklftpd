#uncoment the next 2 lines to get LKL's file APIs
LKL_DEFINES+=-DLKL_FILE_APIS
LKL=lkl/vmlinux

APR_LIN_INCLUDE=-I/usr/include/apr-1.0/
APR_WIN_INCLUDE=-Iapr_win/include/

APR_LIN_LIB=-L/usr/lib/debug/usr/lib/libapr-1.so.0.2.7 -lapr-1
APR_WIN_LIB=apr_win/libapr-1.lib


CFLAGS=-g -D_LARGEFILE64_SOURCE $(LKL_DEFINES)
HERE=$(PWD)
LINUX=$(HERE)/../linux-2.6

PORTABLE_OBJS=listen.c main.c worker.c utils.c config.c cmdio.c cmdhandler.c sess.c fileops.c filestat.c dirops.c




all: daemon.exe daemon.out daemon-aio.out

include/asm:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/asm-lkl include/asm

include/asm-i386:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/asm-i386 include/asm-i386

include/asm-generic:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/asm-generic include/asm-generic

include/linux:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/linux include/linux

%.config: $(LINUX)/arch/lkl/defconfig
	-mkdir `dirname $@`
	cp $^ $@

INC=include/asm include/asm-generic include/asm-i386 include/linux

lkl-aio/vmlinux: .force lkl-aio/.config
	cd $(LINUX) && \
	$(MAKE) O=$(HERE)/`dirname $@` ARCH=lkl \
		LKL_DRIVERS="$(HERE)/drivers/posix-aio/lkl/ $(HERE)/drivers/stduser/lkl" \
		EXTRA_CFLAGS=-DNR_IRQS=2 \
		STDIO_CONSOLE=y FILE_DISK_MAJOR=42 \
		vmlinux

lkl/vmlinux: .force lkl/.config
	cd $(LINUX) && \
	$(MAKE) O=$(HERE)/`dirname $@` ARCH=lkl \
		LKL_DRIVERS=$(HERE)/drivers/stduser/lkl \
		STDIO_CONSOLE=y FILE_DISK=y FILE_DISK_MAJOR=42 \
		vmlinux

lkl-nt/vmlinux: .force lkl-nt/.config
	cd $(LINUX) && \
	$(MAKE) O=$(HERE)/`dirname $@` ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- \
		LKL_DRIVERS=$(HERE)/drivers/stduser/lkl \
		STDIO_CONSOLE=y FILE_DISK=y FILE_DISK_MAJOR=42 \
		vmlinux

lkl-ntk/vmlinux: .force lkl-ntk/.config
	cd $(LINUX) && \
	$(MAKE) O=$(HERE)/`dirname $@` ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- \
		LKL_DRIVERS="$(HERE)/drivers/ntk/lkl/ $(HERE)/drivers/stduser/lkl/" \
		FILE_DISK=y FILE_DISK_MAJOR=42 \
		vmlinux


DRV_STDUSER=drivers/stduser/disk.c drivers/stduser/console.c
DRV_AIO=drivers/stduser/console.c drivers/posix-aio/disk-async.c
DRV_NTK=drivers/ntk/disk.c

AOUT=$(PORTABLE_OBJS) posix.c $(DRV_STDUSER) lkl/vmlinux
AOUT-aio=$(PORTABLE_OBJS) $(DRV_AIO) posix.c lkl-aio/vmlinux
AEXE=$(PORTABLE_OBJS) $(DRV_STDUSER) nt.c lkl-nt/vmlinux

CFLAGS=-Wall -g -DFILE_DISK_MAJOR=42 -D_LARGEFILE64_SOURCE

clean:
	-rm -rf daemon-aio.out daemon.exe daemon.exe lkl lkl-nt lkl-aio include

daemon-aio.out: $(AOUT-aio) $(INC) include/asm .force
	gcc $(CFLAGS) $(APR_LIN_INCLUDE) $(AOUT-aio) $(APR_LIN_LIB) -lrt -o $@

daemon.out: $(AOUT) $(INC) include/asm .force
	gcc $(CFLAGS) $(APR_LIN_INCLUDE) $(AOUT) $(APR_LIN_LIB) -o $@

daemon.exe: $(AEXE) $(INC)
	i586-mingw32msvc-gcc $(CFLAGS) $(APR_WIN_INCLUDE) $(AEXE) $(APR_WIN_LIB) -o $@

.force:
