/* rtty - client of ttysrv
 * vix 28may91 [written]
 *
 * $Id: rtty.c,v 1.2 1992-06-23 16:27:18 vixie Exp $
 */

#include <stdio.h>
#include <termio.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/param.h>

#include "ttyprot.h"
#include "locbrok.h"
#include "rtty.h"

#define USAGE_STR \
	"[-7] [-x DebugLevel] Serv"

#define Tty STDIN

ttyprot T;

extern int optind, opterr;
extern char *optarg;

char *ProgName;
char *ServSpec = NULL;		int Serv;
char LogSpec[MAXPATHLEN];	int Log = -1;
int Debug = 0;
int SevenBit = 0;

struct termios Ttyios, Ttyios_orig;
int Ttyios_set = 0;

void
quit() {
	fprintf(stderr, "\r\n[rtty exiting]\r\n");
	if (Ttyios_set) {
		tcsetattr(Tty, TCSANOW, &Ttyios_orig);
	}
	exit(0);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	char ch;

	ProgName = argv[0];
	while ((ch = getopt(argc, argv, "s:x:7")) != EOF) {
		switch (ch) {
		case 's':
			ServSpec = optarg;
			break;
		case 'x':
			Debug = atoi(optarg);
			break;
		case '7':
			SevenBit++;
			break;
		default:
			USAGE((stderr, "%s: getopt=%c ?\n", ProgName, ch));
		}
	}

	if (optind != argc-1) {
		USAGE((stderr, "must specify service"));
	}
	ServSpec = argv[optind++];

	if (ServSpec[0] == '/') {
		struct sockaddr_un n;

		Serv = socket(PF_UNIX, SOCK_STREAM, 0);
		ASSERT(Serv>=0, "socket");

		n.sun_family = AF_UNIX;
		(void) strcpy(n.sun_path, ServSpec);

		ASSERT(0<=connect(Serv, &n,
			strlen(n.sun_path) +sizeof n.sun_family),n.sun_path);
		dprintf(stderr, "rtty.main: connected on fd%d\n", Serv);
		fprintf(stderr, "connected\n");
	} else {
		int loc, len;
		locbrok lb;
		char buf[10];
		char *cp = strchr(ServSpec, '@');

		if (cp)
			*cp++ = '\0';
		else
			cp = "127.1";

		if ((loc = rconnect(cp, "locbrok")) == -1) {
			fprintf(stderr, "can't contact location broker\n");
			quit();
		}
		lb.lb_port = htons(0);
		len = min(LB_MAXNAMELEN, strlen(ServSpec));
		lb.lb_nlen = htons(len);
		strncpy(lb.lb_name, ServSpec, len);
		ASSERT(write(loc, &lb, sizeof lb)==sizeof lb, "write lb");
		ASSERT(read(loc, &lb, sizeof lb)==sizeof lb, "read lb");
		close(loc);
		lb.lb_port = ntohs(lb.lb_port);
		if (!lb.lb_port) {
			fprintf(stderr, "location broker can't find %s@%s\n",
				ServSpec, cp);
			quit();
		}
		sprintf(buf, "%d", lb.lb_port);
		Serv = rconnect(cp, buf);
		ASSERT(Serv >= 0, "rconnect rtty");
	}

	fprintf(stderr, "[connected]\n");

	{
		tcgetattr(Tty, &Ttyios);
		Ttyios_orig = Ttyios;
		Ttyios_set++;
		Ttyios.c_cflag |= INITIAL_CFLAG;
		Ttyios.c_lflag &= INITIAL_LFLAG;
		Ttyios.c_iflag &= INITIAL_IFLAG;
		Ttyios.c_oflag &= INITIAL_OFLAG;
		Ttyios.c_cc[VMIN] = 0;
		Ttyios.c_cc[VTIME] = 0;
		signal(SIGINT, quit);
		signal(SIGQUIT, quit);
		tcsetattr(Tty, TCSANOW, &Ttyios);
	}

	main_loop();
}

fd_set fds;
int highest_fd;

main_loop()
{
	FD_ZERO(&fds);
	FD_SET(Serv, &fds);
	FD_SET(STDIN, &fds);
	highest_fd = max(Serv, STDIN);

	for (;;) {
		fd_set readfds, exceptfds;
		register int nfound, fd;

		readfds = fds;
		exceptfds = fds;
		nfound = select(highest_fd+1, &readfds, NULL,
				&exceptfds, NULL);
		if (nfound < 0 && errno == EINTR)
			continue;
		ASSERT(0<=nfound, "select");
		for (fd = 0; fd <= highest_fd; fd++) {
			if (FD_ISSET(fd, &exceptfds)) {
				if (fd == Serv) {
					server_died();
				}
			}
			if (FD_ISSET(fd, &readfds)) {
				if (fd == STDIN) {
					tty_input(fd);
				} else if (fd == Serv) {
					serv_input(fd);
				}
			}
		}
	}
}

tty_input(fd) {
	unsigned char buf[1];
	static enum {base, need_cr, tilde} state = base;

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
			if (ch == '\r') {
				state = base;
			}
			break;
		case tilde:
#define HELP_STR "\r\n\
~~  - send one tilde (~)\r\n\
~.  - exit program\r\n\
~^Z - suspent program\r\n\
~^L - set logging\r\n\
~s  - set option\r\n\
~#  - send BREAK\r\n\
~?  - this message\r\n\
"
			state = base;
			switch (ch) {
			case '~': /* ~~ - write one ~, which is in buf[] */
				break;
			case '.': /* ~. - quitsville */
				quit();
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
			case 's': /* ~s - set option */
				set();
				continue;
			case '#': /* ~# - send break */
				tp_sendctl(Serv, TP_BREAK, 0, NULL);
				continue;
			case '?': /* ~? - help */
				fprintf(stderr, HELP_STR);
				continue;
			default: /* ~mumble - write; `mumble' is in buf[] */
				tp_senddata(Serv, (unsigned char *)"~", 1);
				if (Log != -1) {
					write(Log, "~", 1);
				}
				break;
			}
			break;
		}
		if (0 > tp_senddata(Serv, buf, 1)) {
			server_died();
		}
		if (Log != -1) {
			write(Log, buf, 1);
		}
	}
}

set() {
	char vmin = Ttyios.c_cc[VMIN];
	char buf[64];
	int new;

	fputs("~set ", stderr);
	Ttyios.c_cc[VMIN] = 1;
	tcsetattr(Tty, TCSANOW, &Ttyios);
	if (1 == read(Tty, buf, 1)) {
		switch (buf[0]) {
		case '\n':
		case '\r':
			fputs("(show all)\r\n", stderr);
			tp_sendctl(Serv, TP_BAUD|TP_QUERY, 0, NULL);
			tp_sendctl(Serv, TP_PARITY|TP_QUERY, 0, NULL);
			tp_sendctl(Serv, TP_WORDSIZE|TP_QUERY, 0, NULL);
			break;
		case 'b':
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
			break;
		case 'p':
			fputs("parity ", stderr);
			tcsetattr(Tty, TCSANOW, &Ttyios_orig);
			fgets(buf, sizeof buf, stdin);
			if (buf[strlen(buf)-1] == '\n') {
				buf[strlen(buf)-1] = '\0';
			}
			tp_sendctl(Serv, TP_PARITY, strlen(buf),
				   (unsigned char *)buf);
			break;
		case 'w':
			fputs("wordsize ", stderr);
			tcsetattr(Tty, TCSANOW, &Ttyios_orig);
			fgets(buf, sizeof buf, stdin);
			if (!(new = atoi(buf))) {
				break;
			}
			tp_sendctl(Serv, TP_WORDSIZE, new, NULL);
			break;
		default:
			fputs("[baud parity wordsize]\r\n", stderr);
			break;
		}
	}
	Ttyios.c_cc[VMIN] = vmin;
	tcsetattr(Tty, TCSANOW, &Ttyios);
}

logging() {
	if (Log == -1) {
		printf("\r\nLog file is: "); fflush(stdout);
		fgets(LogSpec, sizeof LogSpec, stdin);
		if (LogSpec[strlen(LogSpec)-1] == '\n') {
			LogSpec[strlen(LogSpec)-1] = '\0';
		}
		if (LogSpec[0]) {
			Log = open(LogSpec, O_CREAT|O_APPEND|O_WRONLY, 0644);
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

serv_input(fd) {
	register int nchars;
	register int i;
	register unsigned int o;

	if (0 >= (nchars = read(fd, &T, TP_FIXED))) {
		fprintf(stderr, "serv_input: read(%d) returns %d\n",
			fd, nchars);
		server_died();
	}

	i = ntohs(T.i);
	o = ntohs(T.f) & TP_OPTIONMASK;
	switch (ntohs(T.f) & TP_TYPEMASK) {
	case TP_DATA:
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
		write(STDOUT, T.c, nchars);
		if (Log != -1) {
			write(Log, T.c, nchars);
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
	}
}

server_replied(msg, i)
	char *msg;
	int i;
{
	fprintf(stderr, "[%s %s]\r\n",
		msg,
		i ?"accepted" :"rejected");
}

server_died() {
	fprintf(stderr, "\r\n[server disconnect]\r\n");
	quit();
}
