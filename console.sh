#! /bin/sh

# $Id: console.sh,v 1.3 1993-12-28 00:49:56 vixie Exp $

default_options=''

host=$1

[ -z "$host" ] && {
	ls DESTPATH/sock
	exit
}

if [ -s DESTPATH/opt/${host}.cons ]; then
	options=`cat DESTPATH/opt/${host}.cons`
elif [ -s DESTPATH/opt/DEFAULT.cons ]; then
	options=`cat DESTPATH/opt/DEFAULT.cons`
else
	options="$default_options"
fi

exec DESTPATH/bin/rtty $options DESTPATH/sock/$host
