#########################################################################
#
# Makefile for jpeginfo for *nix environments
#
#
Version = 1.4
PKGNAME = jpeginfo

# Compile Options:
#  -DLINUX    for Linux
#  -DSUN      for Solaris
#  -DSGI      for Silicon Graphics
#  -DHPUX     for HP-UX
#
#  -DLONG_OPTIONS  if you have GNU getopt_long function
#
DEFINES = -DLINUX -DLONG_OPTIONS

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/man/man1
USER	= root
GROUP	= root

# if necessary define where jpeglib and it's headers are located
#LIBDIR  = -L/usr/local/lib
#INCDIR  = -I/usr/local/include


CC     = gcc
CFLAGS = -O2 $(DEFINES) $(INCDIR)  # -N
LIBS   = -ljpeg $(LIBDIR)
STRIP  = strip


# should be no reason to modify lines below this
#########################################################################

DIRNAME = $(shell basename `pwd`) 
DISTNAME  = $(PKGNAME)-$(Version)

OBJS = $(PKGNAME).o md5stuff.o md5/md5c.o

$(PKGNAME):	$(OBJS) 
	$(CC) $(CFLAGS) -o $(PKGNAME) $(OBJS) $(LIBS) 

all:	$(PKGNAME) 

strip:
	for i in $(PKGNAME) ; do [ -x $i ] && $(STRIP) $$i ; done

clean:
	rm -f *~ *.o core a.out make.log $(PKGNAME) $(OBJS)

dist:	clean
	(cd .. ; tar cvzf $(DISTNAME).tar.gz $(DIRNAME))

backup:	dist

zip:	clean	
	(cd .. ; zip -r9 $(DISTNAME).zip $(DIRNAME))

install: all  install.man
	install -m 755 -o $(USER) -g $(GROUP) $(PKGNAME) $(BINDIR)

printable.man:
	groff -Tps -mandoc ./$(PKGNAME).1 >$(PKGNAME).ps
	groff -Tascii -mandoc ./$(PKGNAME).1 | tee $(PKGNAME).prn | sed 's/.//g' >$(PKGNAME).txt

install.man:
	install -m 644 -o $(USER) -g $(GROUP) $(PKGNAME).1 $(MANDIR)

# a tradition !
love:	
	@echo "Not War - Eh?"
# eof

