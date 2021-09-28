CC      = gcc
CFLAGS  = -Wall -O2 -g -W -Wunused-result
ALL_CFLAGS = $(CFLAGS) -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

PROGS = iowatcher
INSTALL = install
prefix = /usr/local
bindir = $(prefix)/bin

export prefix INSTALL

ALL = $(PROGS)

$(PROGS): | depend

all: $(ALL)

%.o: %.c
	$(CC) -o $*.o -c $(ALL_CFLAGS) $<

iowatcher: blkparse.o plot.o main.o tracers.o mpstat.o fio.o
	$(CC) $(ALL_CFLAGS) -o $@ $(filter %.o,$^) -lm -lrt

depend:
	@$(CC) -MM $(ALL_CFLAGS) *.c 1> .depend

clean:
	-rm -f *.o $(PROGS) .depend

install: all
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 $(ALL) $(DESTDIR)$(bindir)

ifneq ($(wildcard .depend),)
include .depend
endif

