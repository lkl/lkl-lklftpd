DEPS=listen.o main.o worker.o utils.o config.o

GCC=gcc
RMF=rm -f

CFLAGS=-g -Wall -Werror -I/usr/include/apr-1.0/ -D_LARGEFILE64_SOURCE
LDFLAGS=-lapr-1 -lpthread

TARGET=daemon

.c.o:
	$(GCC) -c $(CFLAGS) $^

build:$(DEPS)
	$(GCC) -o $(TARGET) $(DEPS) $(LDFLAGS)

clean:
	$(RMF) $(TARGET) $(DEPS)

