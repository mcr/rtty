# $Id: Makefile,v 1.10 1994-05-16 22:36:07 vixie Exp $

VERSION = 3.1.dev

VPATH = ../src

DESTROOT =
DESTPATH = $(DESTROOT)/usr/local/rtty
DESTBIN = $(DESTPATH)/bin

CC = cc
CDEBUG = -O -g
# use -U to undefine, -D to define
#	WANT_TCP	insecure network transparency
#	NEED_BITYPES_H	if you aren't on BSD/386 1.1, BSD 4.4, or SGI IRIX5
#	NEED_STRDUP	if your C library isn't POSIX compliant
CDEFS = -DDEBUG -UWANT_TCPIP -UNEED_BITYPES_H -UNEED_STRDUP
CFLAGS = $(CDEBUG) $(CDEFS) -I/usr/local/include
LIBS = 
#(if WANT_TCPIP defined and this isn't in your libc)
# -lresolv
#(if the resolver needs it, which BIND>=4.9's will on BSD>=4.4 systems)
# -l44bsd

BINARY = ttysrv rtty locbrok
SCRIPT = Startup console startsrv agelogs agelog
ALL = $(BINARY) $(SCRIPT)

all: $(ALL)

clean:; rm -rf $(ALL) *.o *.BAK *.CKP *~

kit:; cshar -o ../kit README Makefile *.c *.h *.sh

bin.tar:; tar cf bin.tar $(ALL)

install: $(ALL) Makefile
	-set -x; test -d $(DESTPATH) || mkdir $(DESTPATH)
	-set +e -x; for x in bin dev sock log pid opt; do \
		test -d $(DESTPATH)/$$x || mkdir $(DESTPATH)/$$x; \
	done
	set -x; for x in $(BINARY); do \
		install -c -m 111 $$x $(DESTBIN)/$$x; \
	done
	set -x; for x in $(SCRIPT); do \
		install -c -m 555 $$x $(DESTBIN)/$$x; \
	done

ttysrv: ttysrv.o ttyprot.o connutil.o misc.o version.o
	$(CC) -o ttysrv ttysrv.o ttyprot.o connutil.o misc.o version.o $(LIBS)

rtty: rtty.o ttyprot.o connutil.o misc.o version.o
	$(CC) -o rtty rtty.o ttyprot.o connutil.o misc.o version.o $(LIBS)

locbrok: locbrok.o misc.o version.o
	$(CC) -o locbrok locbrok.o misc.o version.o $(LIBS)

console: console.sh Makefile
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@
	chmod +x $@

startsrv: startsrv.sh Makefile
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@
	chmod +x $@

agelogs: agelogs.sh Makefile
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@
	chmod +x $@

Startup: Startup.sh Makefile
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@
	chmod +x $@

agelog: agelog.sh Makefile
	cp agelog.sh agelog
	chmod +x $@

ttysrv_saber:; #load $(CFLAGS) ttysrv.c ttyprot.c connutil.c
rtty_saber:; #load $(CFLAGS) rtty.c ttyprot.c connutil.c
locbrok_saber:; #load $(CFLAGS) locbrok.c

ttysrv.o: ttysrv.c ttyprot.h rtty.h
rtty.o: rtty.c ttyprot.h rtty.h
ttyprot.o: ttyprot.c ttyprot.h rtty.h
locbrok.o: locbrok.c locbrok.h rtty.h
connutil.o: connutil.c rtty.h
misc.o: misc.c rtty.h ttyprot.h
version.o: version.c

version.c: Makefile
	rm -f version.c
	( \
	  echo "#ifndef LINT"; \
	  echo "char Version[] ="; \
	  echo '"Version $(VERSION) ('`whoami`'@'`hostname`' '`date`')";'; \
	  echo "#endif"; \
	) >version.c
