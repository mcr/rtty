/* ttyprot.c - utility routines to deal with the rtty protocol
 * vixie 12Sep91 [new]
 */

#ifndef LINT
static char RCSid[] = "$Id: ttyprot.c,v 1.8 1996-08-23 22:09:30 vixie Exp $";
#endif

#include <sys/param.h>
#include <sys/uio.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "rtty.h"
#ifdef NEED_BITYPES_H
# include "bitypes.h"
#endif
#include "ttyprot.h"

#if DEBUG
extern	int		Debug;
#endif

int
tp_senddata(fd, buf, len, typ)
	int len, typ, fd;
	u_char *buf;
{
	struct iovec iov[2];
	ttyprot t;
	int i;

#if DEBUG
	if (Debug >= 5) {
		fprintf(stderr, "tp_senddata(fd=%d, buf=\"", fd);
		cat_v(stderr, buf, len);
		fprintf(stderr, "\", len=%d, typ=%d)\r\n", len, typ);
	}
#endif
	t.f = htons(typ);
	iov[0].iov_base = (caddr_t)&t;
	iov[0].iov_len = TP_FIXED;
	while (len > 0) {
		i = min(len, TP_MAXVAR);
		t.i = htons(i);
		iov[1].iov_base = (caddr_t)buf;
		iov[1].iov_len = i;
		buf += i;
		len -= i;
		i = writev(fd, iov, 2);
		if (i < 0)
			break;
	}
	return (i);
}

int
tp_sendctl(fd, f, i, c)
	int fd;
	u_int f, i;
	u_char *c;
{
	struct iovec iov[2];
	ttyprot t;
	int len = c ?min(strlen((char *)c), TP_MAXVAR) :0;
	int il = 0;

#if DEBUG
	if (Debug >= 5) {
		fprintf(stderr, "tp_sendctl(fd=%d, f=%04x, i=%d, c=\"",
			fd, f, i, c);
		cat_v(stderr, c ?c :(u_char*)"", len);
		fprintf(stderr, "\")\r\n");
	}
#endif
	t.f = htons(f);
	t.i = htons(i);
	iov[il].iov_base = (caddr_t)&t;
	iov[il].iov_len = TP_FIXED;
	il++;
	if (c) {
		iov[il].iov_base = (caddr_t)c;
		iov[il].iov_len = len;
		il++;
	}
	return (writev(fd, iov, il));
}

int
tp_getdata(fd, tp)
	int fd;
	ttyprot *tp;
{
	int len = ntohs(tp->i);
	int nchars;

	if (len != (nchars = read(fd, tp->c, len))) {
		dprintf(stderr, "tp_getdata: read=%d(%d) fd%d: ",
			nchars, len, fd);
		if (nchars < 0)
			perror("read#2");
		else
			fputc('\n', stderr);
		return (0);
	}
#ifdef DEBUG
	if (Debug >= 5) {
		fprintf(stderr, "tp_getdata(fd%d, len%d): got %d bytes",
			fd, len, nchars);
		if (Debug >= 6) {
			fputs(": \"", stderr);
			cat_v(stderr, tp->c, nchars);
			fputs("\"", stderr);
		}
		fputc('\n', stderr);
	}
#endif
	return (nchars);
}
