#! /bin/sh

# $Id: agelogs.sh,v 1.2 1993-12-28 00:49:56 vixie Exp $

agelog=/usr/etc/agelog

cd DESTPATH/dev
for tty in *
do
	$agelog	-m DESTPATH/log/$tty \
		-p `cat DESTPATH/pid/$tty` \
		7 \
		DESTPATH/log/.aged
done

exit
