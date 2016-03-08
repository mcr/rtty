##
## Our Automake template for Makefile.in (and eventually, Makefile)
##
## Process this with automake (or better yet, autoreconf) to generate
## Makefile.in
##

SUFFIXES = .sh
AM_CFLAGS = -DDEBUG -DWANT_TCPIP

bin_PROGRAMS = rtty ttysrv
bin_SCRIPTS = Startup agelog agelogs console startsrv

rtty_SOURCES = rtty.c connutil.c ttyprot.c misc.c rtty.h misc.h ttyprot.h
nodist_rtty_SOURCES = version.c

ttysrv_SOURCES = ttysrv.c ttyprot.c connutil.c misc.c rtty.h misc.h ttyprot.h
nodist_ttysrv_SOURCES = version.c

EXTRA_DIST = Startup.sh agelog.sh agelogs.sh console.sh startsrv.sh

CLEANFILES = $(bin_SCRIPTS) version.c
$(bin_SCRIPTS): Makefile

.sh:
	$(SED) -e 's,DESTPATH,$(exec_prefix),g' < $< > $@

version.c: configure.ac
	( \
	  echo "#ifndef LINT"; \
	  echo "char Copyright[] ="; \
	  echo '  "Copyright 1996,1997,2001,2016 by Internet Software Consortium, inc.";'; \
	  echo "char Version[] ="; \
	  if [ -d .git ]; then \
	  	VERSION="$$(git describe)"; \
	  else \
	  	VERSION="$(VERSION)"; \
	  fi; \
	  echo '  "Version '"$${VERSION}"' ('$$(whoami)'@'$$(hostname)' '$$(date)')";'; \
	  echo "#endif"; \
	) > version.c
