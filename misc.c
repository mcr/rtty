/* misc.c - utility routines for the tty server
 * vixie 14may94 [cloned from ttyprot.c (12Sep91)]
 */

#ifndef LINT
static char RCSid[] = "$Id: misc.c,v 1.1 1994-05-16 21:26:48 vixie Exp $";
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <termios.h>
#include <sys/param.h>

#include "rtty.h"
#ifdef NEED_BITYPES_H
# include "bitypes.h"
#endif
#include "ttyprot.h"

#if DEBUG
extern	int		Debug;
#endif

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

int
install_ttyios(tty, ios)
	int tty;
	const struct termios *ios;
{
#ifdef DEBUG
	if (Debug) {
		fprintf(stderr,
		"install_termios(%d): C=0x%x L=0x%x I=0x%x O=0x%x m=%d t=%d\n",
			tty, ios->c_cflag, ios->c_lflag,
			ios->c_iflag, ios->c_oflag,
			ios->c_cc[VMIN], ios->c_cc[VTIME]);
	}
#endif
	if (0 > tcsetattr(tty, TCSANOW, ios)) {
		perror("tcsetattr");
		return (-1);
	}
	return (0);
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

#ifndef isnumber
/*
 * from libvixutil.a (14may94 version)
 */
int
isnumber(s)
	const char *s;
{
	char ch;
	int n;

	n = 0;
	while (ch = *s++) {
		n++;
		if (!isdigit(ch))
			return (0);
	}
	return (n != 0);
}
#endif
