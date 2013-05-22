

DESTDIR = /usr/local
MYNAME=spcct
INCLUDEDIR = .
DEBUG	= -g -DNQDEBUG
CFLAGS  += -I$(INCLUDEDIR) -D_GNU_SOURCE
LIBS += -lpthread
CC	= gcc

objects = getdep.o pkgcontainer.o nqcommon.o pkgdts.o pkgtraversal.o pkgthreadpool.o

main :	 $(objects)
	$(CC) $(CFLAGS) $(LIBS) -o main $(objects)


getdep.o :	getdep.c pkgcontainer.h pkgcontainer.h pkgdts.h nqcommon.h
	$(CC) $(CFLAGS)  -DMYNAME=\"$(MYNAME)\" -c getdep.c

pkgcontainer.o : pkgdts.h nqcommon.h
pkgdts.o : pkgdts.h 
pkgtraversal.o : pkgdts.h nqcommon.h pkgcontainer.h pkgthreadpool.h

install: main
	-install -Dm755 main $(DESTDIR)/bin/$(MYNAME)

uninstall: 
	-rm $(DESTDIR)/bin/$(MYNAME)

.PHONY : clean install uninstall

clean :	
	-rm main *.o
