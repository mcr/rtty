/* rconnect - connect to a service on a remote host
 * vix 13sep91 [written - again, dammit]
 */

#ifndef LINT
static char RCSid[] = "$Id: connutil.c,v 1.4 1993-12-28 01:15:10 vixie Exp $";
#endif

#ifdef WANT_TCPIP

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern	int		h_errno;
extern	char		*ProgName;

/* assume ip/tcp for now */

static	jmp_buf		jmpalrm;
static	void		sigalrm __P((int));

static
int doconnect(n, ns, to)
	struct sockaddr_in *n;
	int ns, to;
{
	int sock, ok, save;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return -1;
	}
		
	if (to) {
		if (!setjmp(jmpalrm)) {
			signal(SIGALRM, sigalrm);
			alarm(to);
		} else {
			errno = ETIMEDOUT;
			goto finito;
		}
	}
	ok = (connect(sock, (struct sockaddr *)n, ns) >= 0);
	save = errno;
finito:
	if (to) {
		alarm(0);
		signal(SIGALRM, SIG_DFL);
	}
	if (!ok) {
		close(sock);
		errno = save;
		return -1;
	}
	errno = save;
	return sock;
}

int
rconnect(host, service, verbose, errors, timeout)
	char *host;
	char *service;
	FILE *verbose;
	FILE *errors;
	int timeout;
{
	long **hp;
	struct hostent *h;
	struct sockaddr_in n;
	int port, sock, done;

	if (!(port = htons(atoi(service)))) {
		struct servent *s = getservbyname(service, "tcp");
		if (!s) {
			if (errors) {
				fprintf(errors,
					"%s: unknown service\n", service);
			}
			errno = ENOPROTOOPT;
			return -1;
		}
		port = s->s_port;
	}

	n.sin_family = AF_INET;
	n.sin_port = port;

	if (inet_aton(host, &n.sin_addr)) {
		if (verbose) {
			fprintf(verbose, "trying [%s]\n",
				inet_ntoa(n.sin_addr.s_addr));
		}
		done = ((sock = doconnect(&n, sizeof n, timeout)) >= 0);
	} else {
		h = gethostbyname(host);
		if (!h) {
			if (errors) {
				fprintf(errors,
					"%s: %s\n", host, hstrerror(h_errno));
			}
			return -1;
		}
		for (hp = (long**)h->h_addr_list;  *hp;  hp++) {
			bcopy(*hp, (caddr_t)&n.sin_addr.s_addr, h->h_length);
			if (verbose) {
				fprintf(verbose,
					"trying [%s]\n", inet_ntoa(**hp));
			}
			if ((sock = doconnect(&n, sizeof n, timeout)) >= 0) {
				break;
			}
		}
		done = (*hp != NULL);
	}
	if (!done) {
		if (errors) {
			fprintf(errors, "%s: %s\n", host, strerror(errno));
		}
		close(sock);
		return -1;
	}
	return sock;
}

static void
sigalrm(x)
{
	longjmp(jmpalrm, 1);
	/*NOTREACHED*/
}

#endif /*WANT_TCPIP*/
