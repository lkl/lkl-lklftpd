DEPS=listen.o main.o worker.o

GCC=gcc
RMF=rm -f

CFLAGS=-g -Wall -Werror
LDFLAGS=-lapr-1 -lpthread

TARGET=daemon

.c.o:
	$(GCC) -c $(CFLAGS) $^

build:$(DEPS)
	$(GCC) -o $(TARGET) $(DEPS) $(LDFLAGS)

clean:
	$(RMF) $(TARGET) $(DEPS)

