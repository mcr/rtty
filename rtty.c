/* rtty - client of ttysrv
 * vix 28may91 [written]
 */

#ifndef LINT
static char RCSid[] = "$Id: rtty.c,v 1.9 1994-04-11 18:18:57 vixie Exp $";
#endif

#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/bitypes.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/param.h>

#include "rtty.h"
#include "ttyprot.h"
#ifdef WANT_TCPIP
# include "locbrok.h"
#endif

#define USAGE_STR \
	"[-7] [-x DebugLevel] Serv"

#ifdef USE_UNISTD
#include <unistd.h>
#else
#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
extern	char		*getlogin __P((void)),
			*ttyname __P((int));
#endif

extern	int		optind, opterr,
			getopt __P((int, char * const *, const char *));
extern	char		*optarg;

#define Tty STDIN_FILENO

ttyprot T;

extern	int		rconnect(char *host, char *service,
				 FILE *verbose, FILE *errors, int timeout);
extern	char		Version[];

static	char		*ProgName = "amnesia",
			WhoAmI[TP_MAXVAR],
			*ServSpec = NULL,
			*Login = NULL,
			*TtyName = NULL,
			LogSpec[MAXPATHLEN];
static	int		Serv = -1,
			Log = -1,
			Ttyios_set = 0,
			SevenBit = 0,
			highest_fd = -1;

static	struct termios	Ttyios, Ttyios_orig;
static	fd_set		fds;

static	void		main_loop __P((void)),
			tty_input __P((int)),
			query_or_set __P((int)),
			logging __P((void)),
			serv_input __P((int)),
			server_replied __P((char *, int)),
			server_died __P((void)),
			quit __P((int));

#ifdef DEBUG
int Debug = 0;
#endif

main(argc, argv)
	int argc;
	char *argv[];
{
	char ch;

	ProgName = argv[0];

	if (!(Login = getlogin())) {
		struct passwd *pw = getpwuid(getuid());

		if (pw) {
			Login = pw->pw_name;
		} else {
			Login = "nobody";
		}
	}
	if (!(TtyName = ttyname(STDIN_FILENO))) {
		TtyName = "/dev/null";
	}

	while ((ch = getopt(argc, argv, "s:x:l:7")) != EOF) {
		switch (ch) {
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
		case '7':
			SevenBit++;
			break;
		default:
			USAGE((stderr, "%s: getopt=%c ?\n", ProgName, ch));
		}
	}

	if (optind != argc-1) {
		USAGE((stderr, "must specify service\n"));
	}
	ServSpec = argv[optind++];
	sprintf(WhoAmI, "%s@%s", Login, TtyName);

	if (ServSpec[0] == '/') {
		struct sockaddr_un n;

		Serv = socket(PF_UNIX, SOCK_STREAM, 0);
		ASSERT(Serv>=0, "socket");

		n.sun_family = AF_UNIX;
		(void) strcpy(n.sun_path, ServSpec);

		ASSERT(0<=connect(Serv, (struct sockaddr *)&n, sizeof n),
		       n.sun_path);
		dprintf(stderr, "rtty.main: connected on fd%d\r\n", Serv);
		fprintf(stderr, "connected\n");
#ifdef WANT_TCPIP
	} else {
		int loc, len;
		locbrok lb;
		char buf[10];
		char *cp = strchr(ServSpec, '@');

		if (cp)
			*cp++ = '\0';
		else
			cp = "127.1";

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
#endif /*WANT_TCPIP*/
	}

	{
		tcgetattr(Tty, &Ttyios);
		Ttyios_orig = Ttyios;
		prepare_term(&Ttyios);
		signal(SIGINT, quit);
		signal(SIGQUIT, quit);
		install_ttyios(Tty, &Ttyios);
		Ttyios_set++;
	}

	fprintf(stderr,
		"(use (CR)~? for minimal help; also (CR)~q? and (CR)~s?)\r\n");
	tp_sendctl(Serv, TP_WHOSON, strlen(WhoAmI), (u_char*)WhoAmI);
	main_loop();
}

static void
main_loop()
{
	FD_ZERO(&fds);
	FD_SET(Serv, &fds);
	FD_SET(STDIN_FILENO, &fds);
	highest_fd = max(Serv, STDIN_FILENO);

	for (;;) {
		fd_set readfds, exceptfds;
		register int nfound, fd;

		readfds = fds;
		exceptfds = fds;
#if 0
		dprintf(stderr, "rtty.main_loop: select(%d,%08x)\r\n",
			highest_fd+1, readfds.fds_bits[0]);
#endif
		nfound = select(highest_fd+1, &readfds, NULL,
				&exceptfds, NULL);
		if (nfound < 0 && errno == EINTR)
			continue;
		ASSERT(0<=nfound, "select");
#if 0
		dprintf(stderr, "ttysrv.main_loop: select->%d\r\n", nfound);
#endif
		for (fd = 0; fd <= highest_fd; fd++) {
			if (FD_ISSET(fd, &exceptfds)) {
				dprintf(stderr,
					"rtty.main_loop: fd%d exceptional\r\n",
					fd);
				if (fd == Serv) {
					server_died();
				}
			}
			if (FD_ISSET(fd, &readfds)) {
				dprintf(stderr,
					"rtty.main_loop: fd%d readable\r\n",
					fd);
				if (fd == STDIN_FILENO) {
					tty_input(fd);
				} else if (fd == Serv) {
					serv_input(fd);
				}
			}
		}
	}
}

static void
tty_input(fd) {
	unsigned char buf[1];
	static enum {base, need_cr, tilde} state = base;

	fcntl(Tty, F_SETFL, fcntl(Tty, F_GETFL, 0)|O_NONBLOCK);
	while (1 == read(fd, buf, 1)) {
		register unsigned char ch = buf[0];

		switch (state) {
		case base:
			if (ch == '~') {
				state = tilde;
				continue;
			}
			if (ch != '\r') {
				state = need_cr;
			}
			break;
		case need_cr:
			/* \04 (^D) is a line terminator on some systems */
			if (ch == '\r' || ch == '\04') {
				state = base;
			}
			break;
		case tilde:
#define HELP_STR "\r\n\
~~  - send one tilde (~)\r\n\
~.  - exit program\r\n\
~^Z - suspent program\r\n\
~^L - set logging\r\n\
~q  - query server\r\n\
~s  - set option\r\n\
~#  - send BREAK\r\n\
~?  - this message\r\n\
"
			state = base;
			switch (ch) {
			case '~': /* ~~ - write one ~, which is in buf[] */
				break;
			case '.': /* ~. - quitsville */
				quit(0);
				/* FALLTHROUGH */
			case 'L'-'@': /* ~^L - start logging */
				tcsetattr(Tty, TCSANOW, &Ttyios_orig);
				logging();
				tcsetattr(Tty, TCSANOW, &Ttyios);
				continue;
			case 'Z'-'@': /* ~^Z - suspend yourself */
				tcsetattr(Tty, TCSANOW, &Ttyios_orig);
				kill(getpid(), SIGTSTP);
				tcsetattr(Tty, TCSANOW, &Ttyios);
				continue;
			case 'q': /* ~q - query server */
				/*FALLTHROUGH*/
			case 's': /* ~s - set option */
				query_or_set(ch);
				continue;
			case '#': /* ~# - send break */
				tp_sendctl(Serv, TP_BREAK, 0, NULL);
				continue;
			case '?': /* ~? - help */
				fprintf(stderr, HELP_STR);
				continue;
			default: /* ~mumble - write; `mumble' is in buf[] */
				tp_senddata(Serv, (u_char *)"~", 1,
					    TP_DATA);
				if (Log != -1) {
					write(Log, "~", 1);
				}
				break;
			}
			break;
		}
		if (0 > tp_senddata(Serv, buf, 1, TP_DATA)) {
			server_died();
		}
		if (Log != -1) {
			write(Log, buf, 1);
		}
	}
	fcntl(Tty, F_SETFL, fcntl(Tty, F_GETFL, 0)&~O_NONBLOCK);
}

static void
query_or_set(ch)
	int ch;
{
	char vmin = Ttyios.c_cc[VMIN];
	char buf[64];
	int set;
	int new;

	if (ch == 'q')
		set = 0;
	else if (ch == 's')
		set = 1;
	else
		return;

	fputs(set ?"~set " :"~query ", stderr);
	Ttyios.c_cc[VMIN] = 1;
	tcsetattr(Tty, TCSANOW, &Ttyios);
	if (1 == read(Tty, buf, 1)) {
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
				tcsetattr(Tty, TCSANOW, &Ttyios_orig);
				fgets(buf, sizeof buf, stdin);
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
				tcsetattr(Tty, TCSANOW, &Ttyios_orig);
				fgets(buf, sizeof buf, stdin);
				if (buf[strlen(buf)-1] == '\n') {
					buf[strlen(buf)-1] = '\0';
				}
				tp_sendctl(Serv, TP_PARITY, strlen(buf),
					   (unsigned char *)buf);
			}
			break;
		case 'w':
			if (!set) {
				fputs("\07\r\n", stderr);
			} else {
				fputs("wordsize ", stderr);
				tcsetattr(Tty, TCSANOW, &Ttyios_orig);
				fgets(buf, sizeof buf, stdin);
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
				fputs("[all baud parity wordsize]\r\n",
				      stderr);
			else
				fputs("[all Whoson Tail Version]\r\n", stderr);
			break;
		}
	}
	Ttyios.c_cc[VMIN] = vmin;
	tcsetattr(Tty, TCSANOW, &Ttyios);
}

static void
logging() {
	if (Log == -1) {
		printf("\r\nLog file is: "); fflush(stdout);
		fgets(LogSpec, sizeof LogSpec, stdin);
		if (LogSpec[strlen(LogSpec)-1] == '\n') {
			LogSpec[strlen(LogSpec)-1] = '\0';
		}
		if (LogSpec[0]) {
			Log = open(LogSpec, O_CREAT|O_APPEND|O_WRONLY, 0640);
			if (Log == -1) {
				perror(LogSpec);
			}
		}
	} else {
		if (0 > close(Log)) {
			perror(LogSpec);
		} else {
			printf("\n[%s closed]\n", LogSpec);
		}
		Log = -1;
	}
}

static void
serv_input(fd) {
	register int nchars;
	register int i;
	register unsigned int f, o, t;
	char passwd[TP_MAXVAR], s[3], *c, *crypt();

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
			register int i;

			for (i = 0;  i < nchars;  i++) {
				T.c[i] &= 0x7f;
			}
		}
		switch (t) {
		case TP_DATA:
			write(STDOUT_FILENO, T.c, nchars);
			if (Log != -1) {
				write(Log, T.c, nchars);
			}
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
		if (o & TP_QUERY) {
			fprintf(stderr, "[baud %d]\r\n", i);
		} else {
			server_replied("baud rate change", i);
		}
		break;
	case TP_PARITY:
		if (o & TP_QUERY) {
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
		if (o & TP_QUERY) {
			fprintf(stderr, "[wordsize %d]\r\n", i);
		} else {
			server_replied("wordsize change", i);
		}
		break;
	case TP_LOGIN:
		if (!(o & TP_QUERY)) {
			break;
		}
		tp_sendctl(Serv, TP_LOGIN, strlen(Login), (u_char*)Login);
		break;
	case TP_PASSWD:
		if (!(o & TP_QUERY)) {
			break;
		}
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
server_replied(msg, i)
	char *msg;
	int i;
{
	fprintf(stderr, "[%s %s]\r\n",
		msg,
		i ?"accepted" :"rejected");
}

static void
server_died() {
	fprintf(stderr, "\r\n[server disconnect]\r\n");
	quit(0);
}

static void
quit(x) {
	fprintf(stderr, "\r\n[rtty exiting]\r\n");
	if (Ttyios_set) {
		tcsetattr(Tty, TCSANOW, &Ttyios_orig);
	}
	exit(0);
}
