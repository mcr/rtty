# $Id: Makefile,v 1.2 1992-04-18 04:16:37 vixie Exp $

DESTROOT =
DESTPATH = $(DESTROOT)/rtty
DESTBIN = $(DESTPATH)/bin

DEBUG = -O
CFLAGS = $(DEBUG)

BINARY = ttysrv rtty locbrok
SCRIPT = Startup console startsrv agelogs agelog
ALL = $(BINARY) $(SCRIPT)

all: $(ALL)

clean:; rm -rf $(ALL) *.o *.BAK *.CKP *~

install: $(ALL)
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

console: console.sh
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@

startsrv: startsrv.sh
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@

agelogs: agelogs.sh
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@

Startup: Startup.sh
	sed -e 's:DESTPATH:$(DESTPATH):g' <$@.sh >$@

agelog: agelog.sh
	cp agelog.sh agelog

ttysrv_saber:; #load $(CFLAGS) ttysrv.c ttyprot.c connutil.c
rtty_saber:; #load $(CFLAGS) rtty.c ttyprot.c connutil.c
locbrok_saber:; #load $(CFLAGS) locbrok.c

ttysrv.o: ttysrv.c ttyprot.h rtty.h
rtty.o: rtty.c ttyprot.h rtty.h
ttyprot.o: ttyprot.c ttyprot.h rtty.h
locbrok.o: locbrok.c locbrok.h rtty.h
connutil.o: connutil.c rtty.h

