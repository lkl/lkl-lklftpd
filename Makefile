LKL_SOURCE=../linux-2.6
LKL_DEFINES=$(DEFINE_PREFIX)_LARGEFILE64_SOURCE
#LKL_DEFINES+=$(DEFINE_PREFIX)_GNU_SOURCE
#uncoment the next 2 lines to get LKL's file APIs
LKL_DEFINES+=$(DEFINE_PREFIX)LKL_FILE_APIS
LKL=lkl/vmlinux

OBJS=listen.o main.o worker.o utils.o config.o cmdio.o cmdhandler.o sess.o fileops.o filestat.o dirops.o lkl_thread.o 
DEPS=$(patsubst %.o,.depends/%.d,$(1))

DEFINE_PREFIX=-D
INCLUDE_PREFIX=-I
RMF=rm -f
CFLAGS=-g -Wall -Werror -Wformat -Wstrict-prototypes -I/usr/include/apr-1.0/ $(LKL_DEFINES)
LDFLAGS=-lapr-1

TARGET=daemon



build: $(TARGET)

lkl/.config: $(LKL_SOURCE)
	-mkdir `dirname $@`
	cp $^/arch/lkl/defconfig $@

lkl/vmlinux: lkl/.config
	make -C $(LKL_SOURCE) O=$(PWD)/lkl ARCH=lkl 

$(TARGET):$(OBJS) $(LKL)
	$(CC) -o $(TARGET) $(OBJS) $(LKL) $(LDFLAGS) 

TAGS: 
	etags *.c *.h

.PHONY: clean TAGS 

clean:
	$(RMF) $(TARGET) $(OBJS) *~ 
	-rm -rf lkl
	-rm -rf .depends

.depends/%.d: %.c
	mkdir -p .depends/$(dir $<)
	$(CC) $(CFLAGS) -MM -MT $(patsubst %.c,%.o,$<) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

include $(call DEPS,$(OBJS))
