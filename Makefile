DEPS=listen.o main.o worker.o utils.o config.o cmdio.o cmdhandler.o sess.o fileops.o filestat.o dirops.o lkl_thread.o

DEFINE_PREFIX=-D
INCLUDE_PREFIX=-I

GCC=gcc
RMF=rm -f

LKL_DEFINES=$(DEFINE_PREFIX)_LARGEFILE64_SOURCE
#LKL_DEFINES+=$(DEFINE_PREFIX)_GNU_SOURCE
#uncoment the next 2 lines to get LKL's file APIs
LKL_DEFINES+=$(DEFINE_PREFIX)LKL_FILE_APIS
LKL_FLAGS=../linux-2.6/vmlinux


CFLAGS=-g -Wall -Wformat -Wstrict-prototypes -I/usr/include/apr-1.0/ $(LKL_DEFINES)
LDFLAGS=$(LKL_FLAGS) -lapr-1

TARGET=daemon

.c.o:
	$(GCC) -c $(CFLAGS) $^

$(TARGET) build:$(DEPS)
	$(GCC) -o $(TARGET) $(DEPS) $(LDFLAGS)

clean:
	$(RMF) $(TARGET) $(DEPS)

