/* ttyprot.c - utility routines to deal with the rtty protocol
 * vixie 12Sep91 [new]
 */

#ifndef LINT
static char RCSid[] = "$Id: ttyprot.c,v 1.4 1992-11-12 18:26:29 vixie Exp $";
#endif

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "ttyprot.h"
#include "rtty.h"

tp_senddata(fd, buf, len, typ)
	int fd;
	register unsigned char *buf;
	register int len, typ;
{
	register int i;
	ttyprot t;
	struct iovec iov[2];

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

tp_sendctl(fd, f, i, c)
	int fd;
	unsigned int f;
	unsigned int i;
	unsigned char *c;
{
	ttyprot t;
	struct iovec iov[2];
	register int il = 0;

	t.f = htons(f);
	t.i = htons(i);
	iov[il].iov_base = (caddr_t)&t;
	iov[il].iov_len = TP_FIXED;
	il++;
	if (c) {
		iov[il].iov_base = (caddr_t)c;
		iov[il].iov_len = min(strlen(c), TP_MAXVAR);
		il++;
	}
	return writev(fd, iov, il);
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
