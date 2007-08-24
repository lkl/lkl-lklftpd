default: build


LKL_SOURCE=../linux-2.6
LKL_DEFINES=$(DEFINE_PREFIX)_LARGEFILE64_SOURCE
#LKL_DEFINES+=$(DEFINE_PREFIX)_GNU_SOURCE
#uncoment the next 2 lines to get LKL's file APIs
LKL_DEFINES+=$(DEFINE_PREFIX)LKL_FILE_APIS
LKL=lkl/vmlinux

DEPS=listen.o main.o worker.o utils.o config.o cmdio.o cmdhandler.o sess.o fileops.o filestat.o dirops.o lkl_thread.o $(LKL)

DEFINE_PREFIX=-D
INCLUDE_PREFIX=-I

RMF=rm -f


lkl/.config: $(LKL_SOURCE)
	-mkdir `dirname $@`
	cp $^/arch/lkl/defconfig $@

lkl/vmlinux: lkl/.config
	make -C $(LKL_SOURCE) O=$(PWD)/lkl ARCH=lkl 

CFLAGS=-g -Wall -Werror -Wformat -Wstrict-prototypes -I/usr/include/apr-1.0/ $(LKL_DEFINES)
LDFLAGS=-lapr-1

TARGET=daemon

.c.o:
	$(CC) -c $(CFLAGS) $^

$(TARGET) build:$(DEPS)
	$(CC) -o $(TARGET) $(DEPS) $(LDFLAGS)

.PHONY: clean

clean:
	$(RMF) $(TARGET) $(DEPS) *~ 
	-rm -rf lkl

