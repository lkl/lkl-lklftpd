DEPS=listen.o main.o worker.o utils.o config.o cmdio.o cmdhandler.o sess.o fileops.o

DEFINE_PREFIX=-D
INCLUDE_PREFIX=-I

GCC=gcc
RMF=rm -f

LKL_DEFINES=$(DEFINE_PREFIX)_LARGEFILE64_SOURCE
#uncoment the next line to get LKL's file APIs
LKL_DEFINES+=$(DEFINE_PREFIX)LKL_FILE_APIS


CFLAGS=-g -Wall -Werror -Wformat -Wstrict-prototypes -I/usr/include/apr-1.0/ $(LKL_DEFINES)
LDFLAGS=-lapr-1

TARGET=daemon

.c.o:
	$(GCC) -c $(CFLAGS) $^

$(TARGET) build:$(DEPS)
	$(GCC) -o $(TARGET) $(DEPS) $(LDFLAGS)

clean:
	$(RMF) $(TARGET) $(DEPS)

