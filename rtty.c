/* rtty - client of ttysrv
 * vix 28may91 [written]
 */

#ifndef LINT
static char RCSid[] = "$Id: rtty.c,v 1.24 2003-02-14 19:38:50 vixie Exp $";
#endif

/*
 * Copyright (c) 1996,2001 by Internet Software Consortium.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/un.h>
#include <sys/param.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <termios.h>
#include <unistd.h>

#include "rtty.h"
#include "misc.h"
#include "ttyprot.h"
#ifdef WANT_TCPIP
# include "locbrok.h"
#endif

#define LOCAL_DEBUG 0

#define USAGE_STR \
	"[-e c] [-s ServSpec] [-l LoginName] [-7] [-r] [-x DebugLevel] Serv"

#define Tty STDIN_FILENO

ttyprot T;

extern	int		rconnect(char *host, char *service,
				 FILE *verbose, FILE *errors,
				 int timeout);
extern	char		Version[];

static	const char	*ProgName = "amnesia",
			*ServSpec = NULL,
			*Login = NULL,
			*TtyName = NULL;

static	char		WhoAmI[TP_MAXVAR],
			LogSpec[MAXPATHLEN];
static	int		Serv = -1,
			Log = -1,
			Ttyios_set = 0,
			SevenBit = 0,
			Restricted = 0,
			highest_fd = -1,
			EscChar = '~';

static	struct termios	Ttyios, Ttyios_orig;
static	fd_set		fds;

static	void		main_loop(void),
			tty_input(int),
			query_or_set(int, int),
			logging(int),
			serv_input(int),
			server_replied(const char *, int),
			server_died(void),
			quit(int),
			restricted_help(void),
			unrestricted_help(void);

#ifdef DEBUG
int Debug = 0;
#endif

int
main(int argc, char *argv[]) {
	int ch;

	ProgName = argv[0];

	if ((Login = getlogin()) == NULL) {
		struct passwd *pw = getpwuid(getuid());

		if (pw)
			Login = pw->pw_name;
		else
			Login = "nobody";
	}
	if ((TtyName = ttyname(STDIN_FILENO)) == NULL)
		TtyName = "/dev/null";

	while ((ch = getopt(argc, argv, "e:s:x:l:7r")) != -1) {
		switch (ch) {
		case 'e':
			EscChar = optarg[0];
			fprintf(stderr, "escape character set to ");
			if (isprint(EscChar)) {
				fprintf(stderr, "%c\n", EscChar);
			} else {
				fprintf(stderr, "0x%02x\n", EscChar);
				USAGE((stderr,
				       "must specify a printable character\n"
				       ));
			}
			break;

		case 's':
			ServSpec = optarg;
			break;
#ifdef DEBUG
		case 'x':
			Debug = atoi(optarg);
			break;
#endif
		case 'l':
			Login = optarg;
			break;
		case 'r':
			Restricted++;
			break;
		case '7':
			SevenBit++;
			break;
		default:
			USAGE((stderr, "%s: getopt=%c ?\n", ProgName, ch));
		}
	}

	if (optind != (argc - 1)) {
		USAGE((stderr, "must specify service\n"));
	}
	ServSpec = argv[optind++];
	sprintf(WhoAmI, "%s@%s", Login, TtyName);

	if (ServSpec[0] == '/') {
		struct sockaddr_un n;
		int cc;

		Serv = socket(PF_UNIX, SOCK_STREAM, 0);
		ASSERT(Serv>=0, "socket");

		n.sun_family = AF_UNIX;
		(void) strcpy(n.sun_path, ServSpec);

		cc = connect(Serv, (struct sockaddr *)&n, sizeof n);
		ASSERT(cc >= 0, n.sun_path);
		dprintf(stderr, "rtty.main: connected on fd%d\r\n", Serv);
		fprintf(stderr, "connected\n");
	} else {
#ifdef WANT_TCPIP
		int loc, len;
		locbrok lb;
		char buf[10];
		char *cp = strchr(ServSpec, '@');

		if (cp)
			*cp++ = '\0';
		else
			cp = "127.0.0.1";

		if ((loc = rconnect(cp, "locbrok", NULL,stderr,30)) == -1) {
			fprintf(stderr, "can't contact location broker\n");
			quit(0);
		}
		lb.lb_port = htons(0);
		len = min(LB_MAXNAMELEN, strlen(ServSpec));
		lb.lb_nlen = htons(len);
		strncpy(lb.lb_name, ServSpec, len);
		ASSERT(write(loc, &lb, sizeof lb)==sizeof lb, "write lb");
		ASSERT(read(loc, &lb, sizeof lb)==sizeof lb, "read lb");
		close(loc);
		lb.lb_port = ntohs(lb.lb_port);
		dprintf(stderr, "(locbrok: port %d)\n", lb.lb_port);
		if (!lb.lb_port) {
			fprintf(stderr, "location broker can't find %s@%s\n",
				ServSpec, cp);
			quit(0);
		}
		sprintf(buf, "%d", lb.lb_port);
		Serv = rconnect(cp, buf, NULL,stderr,30);
		ASSERT(Serv >= 0, "rconnect rtty");
#else
		USAGE((stderr, "service must begin with a '/'\n"));
#endif /*WANT_TCPIP*/
	}

	tcgetattr(Tty, &Ttyios);
	Ttyios_orig = Ttyios;
	prepare_term(&Ttyios, 1);
	signal(SIGINT, quit);
	signal(SIGQUIT, quit);
	install_ttyios(Tty, &Ttyios);
	Ttyios_set++;

	fprintf(stderr,
		"(use (CR)%c? for minimal help; "
		"also (CR)%cq? and (CR)%cs?)\r\n",
		EscChar, EscChar, EscChar);
	tp_sendctl(Serv, TP_WHOSON, strlen(WhoAmI), (u_char*)WhoAmI);
	main_loop();
	exit(0);
}

static void
main_loop(void) {
	FD_ZERO(&fds);
	FD_SET(Serv, &fds);
	FD_SET(STDIN_FILENO, &fds);
	highest_fd = max(Serv, STDIN_FILENO);

	for (;;) {
		fd_set readfds, exceptfds;
		int nfound, fd;

		readfds = fds;
		exceptfds = fds;
		nfound = select(highest_fd+1, &readfds, NULL,
				&exceptfds, NULL);
		if (nfound < 0 && errno == EINTR)
			continue;
		ASSERT(0 <= nfound, "select");
		for (fd = 0; fd <= highest_fd; fd++) {
			if (FD_ISSET(fd, &exceptfds)) {
				if (fd == Serv)
					server_died();
			}
			if (FD_ISSET(fd, &readfds)) {
				if (fd == STDIN_FILENO)
					tty_input(fd);
				else if (fd == Serv)
					serv_input(fd);
			}
		}
	}
	/*NOTREACHED*/
}

static void
tty_input(int fd) {
	static enum {base, need_cr, tilde} state = base;
	u_char buf[1];
	int n, save_nonblock;

	save_nonblock = tty_nonblock(fd, 1);
	while ((n = read(fd, buf, 1)) == 1) {
		u_char ch = buf[0];

		switch (state) {
		case base:
			if (ch == EscChar) {
				state = tilde;
				continue;
			}
			if (ch != '\r')
				state = need_cr;
			break;
		case need_cr:
			/* \04 (^D) is a line terminator on some systems */
			if (ch == '\r' || ch == '\04')
				state = base;
			break;
		case tilde:
			state = base;
			if (ch == EscChar) {
				/* duplicated esc sends just one, in buf[] */
				break;
			}
			switch (ch) {
			case '.': /* ~. - quitsville */
				(void) tty_nonblock(fd, 0);
				quit(0);
				/* FALLTHROUGH */
			case 'L'-'@': /* ~^L - start logging */
				if (Restricted)
					goto passthrough;
				install_ttyios(fd, &Ttyios_orig);
				logging(fd);
				install_ttyios(fd, &Ttyios);
				continue;
			case 'Z'-'@': /* ~^Z - suspend yourself */
				if (Restricted)
					goto passthrough;
				install_ttyios(fd, &Ttyios_orig);
				(void) tty_nonblock(fd, 0);
				kill(getpid(), SIGTSTP);
				install_ttyios(fd, &Ttyios);
				(void) tty_nonblock(fd, 1);
				continue;
			case 's': /* ~s - set option */
				if (Restricted)
					goto passthrough;
				/*FALLTHROUGH*/
			case 'q': /* ~q - query server */
				query_or_set(fd, ch);
				continue;
			case '#': /* ~# - send break */
				tp_sendctl(Serv, TP_BREAK, 0, NULL);
				continue;
			case '?': /* ~? - help */
				if (!Restricted)
					restricted_help();
				unrestricted_help();
				continue;
 passthrough:
			default: /* ~mumble - write; `mumble' is in buf[] */
				tp_senddata(Serv, (u_char *)&EscChar, 1,
					    TP_DATA);
				if (Log != -1)
					write(Log, &EscChar, 1);
				break;
			}
			break;
		}
		if (0 > tp_senddata(Serv, buf, 1, TP_DATA))
			server_died();
		if (Log != -1)
			write(Log, buf, 1);
	}
	(void) tty_nonblock(fd, save_nonblock);
	if (n == 0)
		quit(0);
}

static void
query_or_set(int fd, int ch) {
	int set, new, save_nonblock;
	char buf[64];

	switch (ch) {
	case 'q':
		set = 0;
		break;
	case 's':
		set = 1;
		break;
	default:
		fputc('G'-'@', stderr);
		return;
	}

	fputs(set ?"~set " :"~query ", stderr);
	save_nonblock = tty_nonblock(fd, 0);

	switch (read(fd, buf, 1)) {
	case -1:
		perror("read");
		goto done;
	case 0:
		fprintf(stderr, "!read");
		fflush(stderr);
		goto done;
	default:
		break;
	}

	switch (buf[0]) {
	case '\n':
	case '\r':
	case 'a':
		fputs("(show all)\r\n", stderr);
		tp_sendctl(Serv, TP_BAUD|TP_QUERY, 0, NULL);
		tp_sendctl(Serv, TP_PARITY|TP_QUERY, 0, NULL);
		tp_sendctl(Serv, TP_WORDSIZE|TP_QUERY, 0, NULL);
		break;
	case 'b':
		if (!set) {
			fputs("\07\r\n", stderr);
		} else {
			fputs("baud ", stderr);
			install_ttyios(fd, &Ttyios_orig);
			fgets(buf, sizeof buf, stdin);
			install_ttyios(fd, &Ttyios);
			if (buf[strlen(buf)-1] == '\n') {
				buf[strlen(buf)-1] = '\0';
			}
			if (!(new = atoi(buf))) {
				break;
			}
			tp_sendctl(Serv, TP_BAUD, new, NULL);
		}
		break;
	case 'p':
		if (!set) {
			fputs("\07\r\n", stderr);
		} else {
			fputs("parity ", stderr);
			install_ttyios(fd, &Ttyios_orig);
			fgets(buf, sizeof buf, stdin);
			install_ttyios(fd, &Ttyios);
			if (buf[strlen(buf)-1] == '\n') {
				buf[strlen(buf)-1] = '\0';
			}
			tp_sendctl(Serv, TP_PARITY, strlen(buf),
				   (u_char *)buf);
		}
		break;
	case 'w':
		if (!set) {
			fputs("\07\r\n", stderr);
		} else {
			fputs("wordsize ", stderr);
			install_ttyios(fd, &Ttyios_orig);
			fgets(buf, sizeof buf, stdin);
			install_ttyios(fd, &Ttyios);
			if (!(new = atoi(buf))) {
				break;
			}
			tp_sendctl(Serv, TP_WORDSIZE, new, NULL);
		}
		break;
	case 'T':
		if (set) {
			fputs("\07\r\n", stderr);
		} else {
			fputs("Tail\r\n", stderr);
			tp_sendctl(Serv, TP_TAIL|TP_QUERY, 0, NULL);
		}
		break;
	case 'W':
		if (set) {
			fputs("\07\r\n", stderr);
		} else {
			fputs("Whoson\r\n", stderr);
			tp_sendctl(Serv, TP_WHOSON|TP_QUERY, 0, NULL);
		}
		break;
	case 'V':
		if (set) {
			fputs("\07\r\n", stderr);
		} else {
			fputs("Version\r\n", stderr);
			tp_sendctl(Serv, TP_VERSION|TP_QUERY, 0, NULL);
			fprintf(stderr, "[%s (client)]\r\n", Version);
		}
		break;
	default:
		if (set)
			fputs("[all baud parity wordsize]\r\n", stderr);
		else
			fputs("[all Whoson Tail Version]\r\n", stderr);
		break;
	}
 done:
	(void) tty_nonblock(fd, save_nonblock);
	return;
}

static void
logging(int fd) {
	if (Log == -1) {
		int save_nonblock = tty_nonblock(fd, 0);

		printf("\r\nLog file is: "); fflush(stdout);
		fgets(LogSpec, sizeof LogSpec, stdin);
		if (LogSpec[strlen(LogSpec) - 1] == '\n')
			LogSpec[strlen(LogSpec)-1] = '\0';
		if (LogSpec[0]) {
			Log = open(LogSpec, O_CREAT|O_APPEND|O_WRONLY, 0640);
			if (Log == -1)
				perror(LogSpec);
		}
		(void) tty_nonblock(fd, save_nonblock);
	} else {
		if (0 > close(Log))
			perror(LogSpec);
		else
			printf("\n[%s closed]\n", LogSpec);
		Log = -1;
	}
}

static void
serv_input(int fd) {
	char passwd[TP_MAXVAR], s[3], *c, *crypt();
	int nchars, i;
	u_int f, o, t;

	if (0 >= (nchars = read(fd, &T, TP_FIXED))) {
		fprintf(stderr, "serv_input: read(%d) returns %d\n",
			fd, nchars);
		server_died();
	}

	i = ntohs(T.i);
	f = ntohs(T.f);
	o = f & TP_OPTIONMASK;
	t = f & TP_TYPEMASK;
	switch (t) {
	case TP_DATA:	/* FALLTHROUGH */
	case TP_NOTICE:
		if (i != (nchars = read(fd, T.c, i))) {
			fprintf(stderr, "serv_input: read@%d need %d got %d\n",
				fd, i, nchars);
			server_died();
		}
		if (SevenBit) {
			int x;

			for (x = 0; x < nchars; x++)
				T.c[x] &= 0x7f;
		}
		switch (t) {
		case TP_DATA:
			write(STDOUT_FILENO, T.c, nchars);
			if (Log != -1)
				write(Log, T.c, nchars);
			break;
		case TP_NOTICE:
			write(STDOUT_FILENO, "[", 1);
			write(STDOUT_FILENO, T.c, nchars);
			write(STDOUT_FILENO, "]\r\n", 3);
			if (Log != -1) {
				write(Log, "[", 1);
				write(Log, T.c, nchars);
				write(Log, "]\r\n", 3);
			}
			break;
		default:
			break;
		}
		break;
	case TP_BAUD:
		if ((o & TP_QUERY) != 0)
			fprintf(stderr, "[baud %d]\r\n", i);
		else
			server_replied("baud rate change", i);
		break;
	case TP_PARITY:
		if ((o & TP_QUERY) != 0) {
			if (i != (nchars = read(fd, T.c, i))) {
				server_died();
			}
			T.c[i] = '\0';
			fprintf(stderr, "[parity %s]\r\n", T.c);
		} else {
			server_replied("parity change", i);
		}
		break;
	case TP_WORDSIZE:
		if ((o & TP_QUERY) != 0)
			fprintf(stderr, "[wordsize %d]\r\n", i);
		else
			server_replied("wordsize change", i);
		break;
	case TP_LOGIN:
		if ((o & TP_QUERY) == 0)
			break;
		tp_sendctl(Serv, TP_LOGIN, strlen(Login), (u_char*)Login);
		break;
	case TP_PASSWD:
		if ((o & TP_QUERY) == 0)
			break;
		fputs("Password:", stderr); fflush(stderr);
		for (c = passwd;  c < &passwd[sizeof passwd];  c++) {
			fd_set infd;

			FD_ZERO(&infd);
			FD_SET(Tty, &infd);
			if (1 != select(Tty+1, &infd, NULL, NULL, NULL))
				break;
			if (1 != read(Tty, c, 1))
				break;
			if (*c == '\r')
				break;
		}
		*c = '\0';
		fputs("\r\n", stderr); fflush(stderr);
		s[0] = 0xff & (i>>8);
		s[1] = 0xff & i;
		s[2] = '\0';
		c = crypt(passwd, s);
		tp_sendctl(Serv, TP_PASSWD, strlen(c), (u_char*)c);
		break;
	}
}

static void
server_replied(const char *msg, int i) {
	fprintf(stderr, "[%s %s]\r\n", msg, i ? "accepted" : "rejected");
}

static void
server_died(void) {
	fprintf(stderr, "\r\n[server disconnect]\r\n");
	quit(0);
}

static void
quit(int x) {
	fprintf(stderr, "\r\n[rtty exiting]\r\n");
	if (Ttyios_set)
		install_ttyios(Tty, &Ttyios_orig);
	exit(0);
}

const char *r_help_strs[] = {
	"^Z - suspend program",
	"^L - set logging",
	"q - query server",
	"s - set option",
	NULL
};

const char *help_strs[] = {
	". - exit program",
	"# - send BREAK",
	"? - this message",
	NULL
};

static void
restricted_help(void) {
	const char * const *cp;

	fprintf(stderr, "\r\n");
	for (cp = r_help_strs; *cp; cp++)
		fprintf(stderr, "%c%s\r\n", EscChar, *cp);
}

static void
unrestricted_help(void) {
	const char * const *cp;

	fprintf(stderr, "\r\n");
	fprintf(stderr, "%c%c  - send one escape character (%c)\r\n",
		EscChar, EscChar, EscChar);
	for (cp = help_strs; *cp; cp++)
		fprintf(stderr, "%c%s\r\n", EscChar, *cp);
}
