#! /bin/sh

# $Id: agelogs.sh,v 1.1 1992-01-02 02:04:18 vixie Exp $

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
