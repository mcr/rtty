# $Id: Makefile,v 1.4 1992-09-10 23:32:43 vixie Exp $

DESTROOT =
DESTPATH = $(DESTROOT)/rtty
DESTBIN = $(DESTPATH)/bin

CC = gcc
CDEBUG = -O -g
CFLAGS = $(CDEBUG)

BINARY = ttysrv rtty locbrok
SCRIPT = Startup console startsrv agelogs agelog
ALL = $(BINARY) $(SCRIPT)

all: $(ALL)

clean:; rm -rf $(ALL) *.o *.BAK *.CKP *~

kit:; shar -o ../kit README Makefile *.c *.h *.sh

install: $(ALL) Makefile
	set -x; for x in $(BINARY); do \
		install -c -m 111 $$x $(DESTBIN)/$$x; \
	done
	set -x; for x in $(SCRIPT); do \
		install -c -m 555 $$x $(DESTBIN)/$$x; \
	done

ttysrv: ttysrv.o ttyprot.o connutil.o
	$(CC) -o ttysrv ttysrv.o ttyprot.o connutil.o

rtty: rtty.o ttyprot.o connutil.o
	$(CC) -o rtty rtty.o ttyprot.o connutil.o

locbrok: locbrok.o
	$(CC) -o locbrok locbrok.o

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

