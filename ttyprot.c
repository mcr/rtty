/* ttyprot.c - utility routines to deal with the rtty protocol
 * vixie 12Sep91 [new]
 *
 * $Id: ttyprot.c,v 1.1 1992-01-02 02:04:18 vixie Exp $
 */

#include <sys/types.h>
#include <sys/uio.h>

#include "ttyprot.h"
#include "rtty.h"

tp_senddata(fd, buf, len)
	int fd;
	register unsigned char *buf;
	register int len;
{
	register int i;
	ttyprot t;
	struct iovec iov[2];

	t.f = htons(TP_DATA);
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
