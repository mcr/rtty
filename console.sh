#! /bin/sh

# $Id: console.sh,v 1.2 1992-06-23 16:27:18 vixie Exp $

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
