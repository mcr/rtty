#!/bin/sh

# $Id: startsrv.sh,v 1.2 1992-06-23 16:27:18 vixie Exp $

default_options='-b 9600 -w 8 -p none'

for host
do
	echo -n "startsrv($host):"
	#
	# kill any existing ttysrv on this port
	#
	[ -f DESTPATH/pid/$host ] && {
		pid=`cat DESTPATH/pid/$host`
		echo -n " oldpid=$pid"
		while ps w$pid >/tmp/startsrv$$ 2>&1
		do
			grep -s ttysrv /tmp/startsrv$$ && {
				echo -n " killed"
				kill $pid
				sleep 1
			} || {
				break
			}
		done
		rm DESTPATH/pid/$host /tmp/startsrv$$
	}
	#
	# start up a new one
	#
	if [ -s DESTPATH/opt/${host}.srv ]; then
		options=`cat DESTPATH/opt/${host}.srv`
	elif [ -s DESTPATH/opt/DEFAULT.srv ]; then
		options=`cat DESTPATH/opt/DEFAULT.srv`
	else
		options="$default_options"
	fi
	rm -f DESTPATH/sock/$host
	DESTPATH/bin/ttysrv $options \
		-t DESTPATH/dev/$host \
		-s DESTPATH/sock/$host \
		-l DESTPATH/log/$host &
	echo $! >DESTPATH/pid/$host
	echo -n " newpid=$!"
	sleep 1
	chmod ugo+rw DESTPATH/sock/$host
	chmod og-rwx DESTPATH/log/$host
	echo " done."
done

exit
