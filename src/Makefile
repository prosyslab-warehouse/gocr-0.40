#
# Makefile for ./src path, used by configure
#

EXEEXT = 
PROGRAM = gocr$(EXEEXT)
PGMASCLIB = Pgm2asc
LIBPGMASCLIB = lib$(PGMASCLIB).a
INCLUDEFILES = gocr.h

LIBOBJS=pgm2asc.o \
	box.o \
	database.o \
	detect.o \
	barcode.o \
	lines.o \
	list.o \
	ocr0.o \
	ocr0n.o \
	ocr1.o \
	otsu.o \
	output.o \
	pixel.o \
	unicode.o \
	remove.o \
	pnm.o \
	pcx.o \
	job.o

# these two lines are for cross-compiling, not tested
#srcdir = .
#VPATH = .
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
includedir = ${prefix}/include

CC=gcc
RANLIB=ranlib
INSTALL=/usr/bin/install -c

DEFS=-DHAVE_CONFIG_H
CPPFLAGS=
# to see the config.h
CFLAGS=-g -O2 $(CPPFLAGS) -I../include $(DEFS)
LDFLAGS= -lpnm  -L.
DESTDIR=/usr/local

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

default: all

all: $(PROGRAM) $(LIBPGMASCLIB)

$(PROGRAM): $(LIBPGMASCLIB) gocr.o
	$(CC) -o $@ gocr.o $(LDFLAGS) -l$(PGMASCLIB)
	if test -r $(PROGRAM); then cp $@ ../bin; fi


$(LIBPGMASCLIB): $(LIBOBJS)
	-rm -f $@
	$(AR) cru $@ $(LIBOBJS)
	$(RANLIB) $@

$(LIBOBJS): Makefile

# PHONY = don't look at file clean, -rm = start rm and ignore errors
.PHONY : clean proper install
install: all
	$(INSTALL) -d $(DESTDIR)$(bindir) $(DESTDIR)$(libdir) $(DESTDIR)$(includedir)
	$(INSTALL) ../bin/$(PROGRAM) $(DESTDIR)$(bindir)
	$(INSTALL) ../bin/gocr.tcl   $(DESTDIR)$(bindir)  # better X11/bin?
	$(INSTALL) $(LIBPGMASCLIB) $(DESTDIR)$(libdir)
	$(INSTALL) $(INCLUDEFILES) $(DESTDIR)$(includedir)

clean:
	-rm -f *.o *~

proper: clean
	-rm -f gocr libPgm2asc.a
	
