#!/bin/sh

# $Id: startsrv.sh,v 1.3 1993-12-28 00:49:56 vixie Exp $

default_options='-b 9600 -w 8 -p none'
default_sock_prot='ug=rw,o='
default_log_prot='u=rw,g=r,o='

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

	if [ -s DESTPATH/prot/${host}.sock ]; then
		sock_prot=`cat DESTPATH/prot/${host}.sock`
	elif [ -s DESTPATH/prot/DEFAULT.sock ]; then
		sock_prot=`cat DESTPATH/prot/DEFAULT.sock`
	else
		sock_prot="$default_sock_prot"
	fi

	if [ -s DESTPATH/prot/${host}.log ]; then
		log_prot=`cat DESTPATH/prot/${host}.log`
	elif [ -s DESTPATH/prot/DEFAULT.log ]; then
		log_prot=`cat DESTPATH/prot/DEFAULT.log`
	else
		log_prot="$default_log_prot"
	fi

	rm -f DESTPATH/sock/$host
	# braces are needed due to obscure bug in ash
	# they won't hurt other systems
	{ DESTPATH/bin/ttysrv $options \
		-t DESTPATH/dev/$host \
		-s DESTPATH/sock/$host \
		-l DESTPATH/log/$host & }
	echo $! >DESTPATH/pid/$host
	echo -n " newpid=$!"
	sleep 1
	chmod $sock_prot DESTPATH/sock/$host
	chmod $log_prot DESTPATH/log/$host
	echo " done."
done

exit
