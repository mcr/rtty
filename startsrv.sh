#!/bin/sh

# $Id: startsrv.sh,v 1.1 1992-01-02 02:04:18 vixie Exp $

for host
do
	echo -n "startsrv($host):"
	#
	# kill any existing ttysrv on this port
	#
	[ -f DESTPATH/pid/$host ] && {
		pid=`cat DESTPATH/pid/$host`
		echo -n " oldpid=$pid"
		while ps w$pid >/tmp/start1$$ 2>&1
		do
			grep -s ttysrv /tmp/start1$$ && {
				echo -n " killed"
				kill $pid
				sleep 1
			} || {
				break
			}
		done
		rm DESTPATH/pid/$host
	}
	#
	# start up a new one
	#
	rm -f DESTPATH/sock/$host
	DESTPATH/bin/ttysrv \
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
