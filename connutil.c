/* rconnect - connect to a service on a remote host
 * vix 13sep91 [written - again, dammit]
 *
 * $Id: connutil.c,v 1.1 1992-01-02 02:04:18 vixie Exp $
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rtty.h"

extern char *ProgName;

/* assume ip/tcp */

int
is_inetaddr(addr)
	register char *addr;
{
	/* wretched guess */
	while (*addr && (*addr == '.' || isdigit(*addr)))
		addr++;
	return (!*addr);
}

int
rconnect(host, service)
	char *host;
	char *service;
{
	long **hp;
	struct hostent *h;
	struct sockaddr_in n;
	int port, sock, done;

	if (!(port = atoi(service))) {
		struct servent *s = getservbyname(service, "tcp");
		if (!s) {
			fprintf(stderr, "rconnect: %s: unknown service\n",
				service);
			errno = ENOPROTOOPT;
			return -1;
		}
		port = s->s_port;
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT(sock>=0, "socket")

	n.sin_family = AF_INET;
	n.sin_port = port;

	if (is_inetaddr(host) && 0 != (n.sin_addr.s_addr = inet_addr(host))) {
		fprintf(stderr, "trying [%s]\n", inet_ntoa(n.sin_addr.s_addr));
		done = (connect(sock, &n, sizeof n) >= 0);
	} else {
		h = gethostbyname(host);
		for (hp = (long**)h->h_addr_list;  *hp;  hp++) {
			bcopy(*hp, (caddr_t)&n.sin_addr.s_addr, h->h_length);
			fprintf(stderr, "trying [%s]\n", inet_ntoa(**hp));
			if (connect(sock, &n, sizeof n) >= 0)
				break;
		}
		done = (*hp != NULL);
	}
	if (!done) {
		perror(host);
		close(sock);
		return -1;
	}
	return sock;
}
