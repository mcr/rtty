#! /bin/sh

# $Id: console.sh,v 1.1 1992-01-02 02:04:18 vixie Exp $

host=$1

[ -z "$host" ] && {
	ls DESTPATH/sock
	exit
}

exec DESTPATH/bin/rtty DESTPATH/sock/$host
