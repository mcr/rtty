/* ttyprot.c - utility routines to deal with the rtty protocol
 * vixie 12Sep91 [new]
 */

#ifndef LINT
static char RCSid[] = "$Id: ttyprot.c,v 1.6 1994-04-11 20:36:00 vixie Exp $";
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <termios.h>
#include <sys/param.h>
#include <sys/uio.h>

#include "rtty.h"
#ifdef NEED_BITYPES_H
# include "bitypes.h"
#endif
#include "ttyprot.h"

#ifdef USE_STDLIB
#include <stdlib.h>
#else
extern	void		*malloc __P((size_t));
#endif

#if DEBUG
extern	int		Debug;
#endif

int
tp_senddata(fd, buf, len, typ)
	int fd;
	register unsigned char *buf;
	register int len, typ;
{
	register int i;
	ttyprot t;
	struct iovec iov[2];

#if DEBUG
	if (Debug) {
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
	return i;
}

int
tp_sendctl(fd, f, i, c)
	int fd;
	unsigned int f;
	unsigned int i;
	unsigned char *c;
{
	ttyprot t;
	struct iovec iov[2];
	register int il = 0;
	int len = c ?min(strlen((char *)c), TP_MAXVAR) :0;

#if DEBUG
	if (Debug) {
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
	return writev(fd, iov, il);
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
		return 0;
	}
#ifdef DEBUG
	if (Debug) {
		fprintf(stderr, "tp_getdata(fd%d, len%d): got %d bytes",
			fd, len, nchars);
		if (Debug > 1) {
			fputs(": \"", stderr);
			cat_v(stderr, tp->c, nchars);
			fputs("\"", stderr);
		}
		fputc('\n', stderr);
	}
#endif
	return nchars;
}

void
cat_v(file, buf, nchars)
	FILE *file;
	u_char *buf;
	int nchars;
{
	while (nchars-- > 0) {
		int c = *buf++;

		if (isprint(c)) {
			fputc(c, file);
		} else if (iscntrl(c)) {
			fputc('^', file);
			fputc('@' + c, file);	/* XXX assumes ASCII */
		} else {
			fprintf(file, "\\%03o", c);
		}
	}
}

char *
strsave(s)
	char *s;
{
	char *x;

	x = malloc(strlen(s) + 1);
	strcpy(x, s);
	return x;
}

int
install_ttyios(tty, ios)
	int tty;
	struct termios *ios;
{
#ifdef DEBUG
	if (Debug) {
		fprintf(stderr,
			"install_termios(%d): C=0x%x L=0x%x I=0x%x O=0x%x\n",
			tty, ios->c_cflag, ios->c_lflag,
			ios->c_iflag, ios->c_oflag);
	}
#endif
	if (0 > tcsetattr(tty, TCSANOW, ios)) {
		perror("tcsetattr");
		return -1;
	}
	return 0;
}

void
prepare_term(ios)
	struct termios *ios;
{
	ios->c_cflag |= HUPCL|CLOCAL|CREAD|TAUTOFLOW|CS8;
	ios->c_lflag |= NOFLSH;
	ios->c_lflag &= ~(ICANON|TOSTOP|ECHO|ECHOE|ECHOK|ECHONL|IEXTEN|ISIG);
	ios->c_iflag &= ~(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK|ISTRIP
			  |INLCR|IGNCR|ICRNL|IXON|IXOFF);
	ios->c_oflag &= ~OPOST;
	ios->c_cc[VMIN] = 0;
	ios->c_cc[VTIME] = 0;
}
