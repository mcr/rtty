/* misc.c - utility routines for the tty server
 * vixie 14may94 [cloned from ttyprot.c (12Sep91)]
 */

#ifndef LINT
static char RCSid[] = "$Id: misc.c,v 1.6 1996-08-23 22:25:25 vixie Exp $";
#endif

/* Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <sys/param.h>

#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "rtty.h"
#ifdef NEED_BITYPES_H
# include "bitypes.h"
#endif
#include "ttyprot.h"

#ifdef USE_STDLIB
# include <stdlib.h>
#else
extern	void		*calloc __P((size_t, size_t)),
			*malloc __P((size_t)),
			*realloc __P((void *, size_t));
#endif

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

void *
safe_malloc(size)
	size_t size;
{
	void *ret = malloc(size);

	if (!ret) {
		perror("malloc");
		exit(1);
	}
	return (ret);
}

void *
safe_calloc(n, size)
	size_t n, size;
{
	void *ret = calloc(n, size);

	if (!ret) {
		perror("calloc");
		exit(1);
	}
	return (ret);
}

void *
safe_realloc(ptr, size)
	void *ptr;
	size_t size;
{
	void *ret = realloc(ptr, size);

	if (!ret) {
		perror("realloc");
		exit(1);
	}
	return (ret);
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

#ifdef NEED_STRDUP
char *
strdup(s)
	const char *s;
{
	char *ret = (char *) safe_malloc(strlen(s) + 1);

	strcpy(ret, s);
	return (ret);
}
#endif

#ifdef NEED_INET_ATON
int inet_aton(cp, addr)
	const char *cp;
	struct in_addr *addr;
{
	u_int32_t v;

	if ((v = inet_addr(cp)) > 0) {
		addr->s_addr = v;
		return (1);
	}
	return (0);
}
#endif
