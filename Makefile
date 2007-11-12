#uncoment the next 2 lines to get LKL's file APIs. This currently works on linux only
.PHONY=clean
LKL_DEFINES+=-DLKL_FILE_APIS
LKL=lkl/vmlinux

APR_LIN_INCLUDE=-I/usr/include/apr-1.0/
APR_WIN_INCLUDE=-Iapr_win/include/

APR_LIN_LIB=-lapr-1 -L/home/gringo/apr12x/.libs/libapr-1.so.0.2.12
APR_WIN_LIB=apr_win/Debug/libapr-1.lib


HERE=$(PWD)
LINUX=$(HERE)/../linux-2.6

SRCS=$(shell ls *.c)
OBJS=$(patsubst %.c,%.o,$(SRCS))
DEPS=$(patsubst %.c,.deps/%.d,$(SRCS))

MKDIR=mkdir -p

all: daemon.out 

include/asm:
	-$(MKDIR) `dirname $@`
	ln -s $(LINUX)/include/asm-lkl include/asm

include/asm-i386:
	-$(MKDIR) `dirname $@`
	ln -s $(LINUX)/include/asm-i386 include/asm-i386

include/asm-generic:
	-$(MKDIR) `dirname $@`
	ln -s $(LINUX)/include/asm-generic include/asm-generic

include/linux:
	-$(MKDIR) `dirname $@`
	ln -s $(LINUX)/include/linux include/linux

%.config: $(LINUX)/arch/lkl/defconfig
	-mkdir `dirname $@`
	cp $^ $@

INC=include/asm include/asm-generic include/asm-i386 include/linux

lkl/vmlinux: lkl/.config drivers/*.c drivers/Makefile
	cd $(LINUX) && \
	$(MAKE) O=$(HERE)/`dirname $@` ARCH=lkl \
		LKL_DRIVERS=$(HERE)/drivers \
		vmlinux

lkl-nt/vmlinux: lkl-nt/.config
	cd $(LINUX) && \
	$(MAKE) O=$(HERE)/`dirname $@` ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- \
		LKL_DRIVERS=$(HERE)/drivers \
		vmlinux

CFLAGS=-Wall -g -DFILE_DISK_MAJOR=42 -D_LARGEFILE64_SOURCE $(LKL_DEFINES)	\
	$(APR_LIN_INCLUDE) 


syscalls.o: syscalls.c $(INC)
	$(CC) -c $(CFLAGS) -Iinclude $< 


%.o: %.c $(INC)
	$(CC) -c $(CFLAGS) $< 


AOUT=$(OBJS) lkl/vmlinux
AEXE=$(OBJS) lkl-nt/vmlinux

clean:
	-rm -rf daemon.out daemon.exe include *.o drivers/*.o drivers/built-in* drivers/.*.cmd .deps/ *~

clean-all: clean
	-rm -rf lkl lkl-nt

TAGS: 
	etags *.c drivers/*.c

daemon.out: $(AOUT) $(INC) include/asm 
	gcc  $(AOUT) $(APR_LIN_LIB) -o $@

daemon.exe: $(AEXE) $(INC)
	i586-mingw32msvc-gcc $(CFLAGS) $(APR_WIN_INCLUDE) $(AEXE) $(APR_WIN_LIB) -o $@

.deps/%.d: %.c
	mkdir -p .deps/$(dir $<)
	$(CC) $(CFLAGS) -MM -MT $(patsubst %.c,%.o,$<) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

include $(DEPS)


