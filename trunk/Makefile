SRCS= libzran.c
SRCS_exe= zran.c
SRCS_so= udf.c
OBJS= $(SRCS:.c=.o)
OBJS_exe= $(SRCS_exe:.c=.o)
OBJS_so= $(SRCS_so:.c=.o)
HDRS= libzran.h strnstr.h
CFLAGS += -Wall -pedantic -std=c99 -g
CFLAGS += -D_GNU_SOURCE
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS += -I/usr/include/mysql
LDLIBS += -lz

%.o: %.c Makefile .depend
	$(CC) $(CFLAGS) -c -o $@ $<

all: zran zran.so

zran: $(OBJS_exe) $(OBJS)
	$(CC) $(CFLAGS) -o $@ \
	$(OBJS_exe) $(OBJS)\
	$(LDLIBS)

zran.so: $(OBJS_so) $(OBJS)
	ld -shared -fPIC -o $@ $(OBJS_so) $(OBJS)

.depend: Makefile
	$(CC) -MM $(CFLAGS) $(SRCS) $(HDRS) 1> .depend

clean:
	rm -f $(OBJS) $(OBJS_exe) $(OBJS_so) zran zran.so .depend
