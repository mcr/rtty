/* ttysrv - serve a tty to stdin/stdout, a named pipe, or a network socket
 * vix 28may91 [written]
 */

#ifndef LINT
static char RCSid[] = "$Id: ttysrv.c,v 1.6 1992-09-10 23:30:52 vixie Exp $";
#endif

#include <stdio.h>
#include <termio.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "ttyprot.h"
#include "locbrok.h"
#include "rtty.h"

#define USAGE_STR \
	"-s Serv -t Tty -l Log -b Baud -p Parity -w Wordsize -i Pidfile"

extern char *calloc(), *malloc(), *realloc();
extern void free();

extern int optind, opterr;
extern char *optarg;

char *ProgName;
char *ServSpec = NULL;	int Serv;
char *TtySpec = NULL;	int Tty = -1;	struct termios Ttyios, Ttyios_orig;
int Ttyios_set = 0;
char *LogSpec = NULL;	FILE *LogF = NULL;	int LogDirty = FALSE;
int Baud = 9600;	speed_t SysBaud;
char *Parity = "none";	unsigned int SysParity;
char ParityBuf[TP_MAXVAR];
char *PidFile = NULL;
int Wordsize = 8;	unsigned int SysWordsize;
fd_set Clients;		int highest_fd;
int Debug = 0;
ttyprot T;
unsigned short Port;
int LocBrok = -1;
time_t Now;

struct whoson {
	char *who;
	time_t lastInput;
} **WhosOn;

struct timeval TOinput = {0, 250000};	/* 0.25 second */
struct timeval TOflush = {1, 0};	/* 1 second */

int Sigpiped = 0;

void
sigpipe() {
	Sigpiped++;
}

void
sighup() {
	if (LogF) {
		fclose(LogF);
		LogF = NULL;
		LogDirty = FALSE;
		open_log();
	}
}

open_log() {
	if (!(LogF = fopen(LogSpec, "a+"))) {
		perror(LogSpec);
		fprintf(stderr, "%s: can't open log file\n", ProgName);
	}
}

void
quit() {
	fprintf(stderr, "\r\nttysrv exiting\r\n");
	if (Ttyios_set && (Tty != -1)) {
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
	while ((ch = getopt(argc, argv, "s:t:l:b:p:w:x:i:")) != EOF) {
		switch (ch) {
		case 's':
			ServSpec = optarg;
			break;
		case 't':
			TtySpec = optarg;
			break;
		case 'l':
			LogSpec = optarg;
			break;
		case 'b':
			Baud = atoi(optarg);
			break;
		case 'p':
			Parity = optarg;
			break;
		case 'w':
			Wordsize = atoi(optarg);
			break;
		case 'x':
			Debug = atoi(optarg);
			break;
		case 'i':
			PidFile = optarg;
			break;
		default:
			USAGE((stderr, "%s: getopt=%c ?\n", ProgName, ch));
		}
	}

	if ((SysBaud = find_baud(Baud)) == B0) {
		USAGE((stderr, "%s: baudrate %d ?\n", ProgName, Baud));
	}
	if ((SysParity = find_parity(Parity)) == -1) {
		USAGE((stderr, "%s: parity %s ?\n", ProgName, Parity));
	}
	if ((SysWordsize = find_wordsize(Wordsize)) == 0) {
		USAGE((stderr, "%s: wordsize %d ?\n", ProgName, Wordsize));
	}

	if (!TtySpec) {
		USAGE((stderr, "%s: must specify -t ttyspec ?\n", ProgName));
	} else if (0 > (Tty = open(TtySpec, O_NONBLOCK|O_RDWR))) {
		fprintf(stderr, "%s: can't open tty ", ProgName);
		perror(TtySpec);
		exit(2);
	} else {
		dprintf(stderr, "ttysrv.main: tty open on fd%d\n", Tty);
		tcgetattr(Tty, &Ttyios);
		Ttyios_orig = Ttyios;
		Ttyios_set++;
		Ttyios.c_cflag |= INITIAL_CFLAG;
		Ttyios.c_lflag &= INITIAL_LFLAG;
		Ttyios.c_iflag &= INITIAL_IFLAG;
		Ttyios.c_oflag &= INITIAL_OFLAG;
		Ttyios.c_cc[VMIN] = 0;
		Ttyios.c_cc[VTIME] = 0;
		set_baud();
		set_parity();
		set_wordsize();
		signal(SIGINT, quit);
		signal(SIGQUIT, quit);
		install_ttyios();
	}

	if (!ServSpec) {
		USAGE((stderr, "%s: must specify -s servspec ?\n", ProgName));
	} else if (ServSpec[0] == '/') {
		struct sockaddr_un n;

		Serv = socket(PF_UNIX, SOCK_STREAM, 0);
		ASSERT(Serv>=0, "socket");

		n.sun_family = AF_UNIX;
		(void) strcpy(n.sun_path, ServSpec);

#ifdef unsafe
		(void) unlink(ServSpec);
#endif
		ASSERT(0<=bind(Serv, &n,
			       strlen(n.sun_path) +sizeof n.sun_family),
		       n.sun_path);
	} else {
		struct sockaddr_in n;
		int nlen = sizeof n;
			
		Serv = socket(PF_INET, SOCK_STREAM, 0);
		ASSERT(Serv>=0, "socket");

		n.sin_family = AF_INET;
		n.sin_port = 0;
		n.sin_addr.s_addr = INADDR_ANY;
		ASSERT(0<=bind(Serv, &n, sizeof n), "bind");

		ASSERT(0<=getsockname(Serv, &n, &nlen), "getsockname");
		Port = n.sin_port;
		fprintf(stderr, "serving internet port %d\n", Port);

		/* register with the location broker, or die */
		{
			int len = min(LB_MAXNAMELEN, strlen(ServSpec));
			locbrok lb;

			LocBrok = rconnect("127.1", "locbrok");
			ASSERT(LocBrok>0, "rconnect locbrok");
			lb.lb_port = htons(Port);
			lb.lb_nlen = htons(len);
			strncpy(lb.lb_name, ServSpec, len);
			ASSERT(0<write(LocBrok, &lb, sizeof lb),
			       "write locbrok")
		}
	}

	if (LogSpec) {
		open_log();
	}

	if (PidFile) {
		FILE *f = fopen(PidFile, "w");
		if (!f) {
			perror(PidFile);
		} else {
			fprintf(f, "%d\n", getpid());
			fclose(f);
		}
	}

	WhosOn = (struct whoson **) calloc(getdtablesize(),
					   sizeof(struct whoson **));

	main_loop();
}

install_ttyios() {
	if (0 > tcsetattr(Tty, TCSANOW, &Ttyios)) {
		perror("tcsetattr");
	}
}

main_loop()
{
	listen(Serv, 5);
	dprintf(stderr, "ttysrv.main: listening on fd%d\n", Serv);
	signal(SIGPIPE, sigpipe);
	signal(SIGHUP, sighup);
	FD_ZERO(&Clients);
	highest_fd = max(Serv, Tty);

	for (;;) {
		fd_set readfds;
		register int nfound, fd;

		readfds = Clients;
		FD_SET(Serv, &readfds);
		FD_SET(Tty, &readfds);
		dprintf(stderr, "ttysrv.main_loop: select(%d,%08x)\n",
			highest_fd+1, readfds.fds_bits[0]);
		nfound = select(highest_fd+1, &readfds, NULL, NULL,
				(LogDirty ?&TOflush :NULL));
		if (nfound < 0 && errno == EINTR)
			continue;
		if (nfound == 0 && LogDirty && LogF) {
			fflush(LogF);
			LogDirty = FALSE;
		}
		Now = time(0);
		dprintf(stderr, "ttysrv.main_loop: select->%d\n", nfound);
		for (fd = 0; fd <= highest_fd; fd++) {
			if (!FD_ISSET(fd, &readfds)) {
				continue;
			}
			dprintf(stderr, "ttysrv.main_loop: fd%d readable\n",
				fd);
			if (fd == Tty) {
				tty_input(fd, FALSE);
				tty_input(fd, TRUE);
				continue;
			}
			if (fd == Serv) {
				serv_input(fd);
				continue;
			}
			client_input(fd);
		}
	}
}

tty_input(fd, aggregate) {
	int nchars, x;
	unsigned char buf[TP_MAXVAR];

	nchars = 0;
	do {
		x = read(fd, buf+nchars, TP_MAXVAR-nchars);
	} while (
		 (x > 0) &&
		 ((nchars += x) < TP_MAXVAR) &&
		 (aggregate && (select(0, NULL, NULL, NULL, &TOinput) || TRUE))
		 );
	if (nchars == 0) {
		return;
	}
	dprintf(stderr, "ttysrv.tty_input: %d bytes read on fd%d\n",
		nchars, fd);
	if (LogF) {
		if (nchars != fwrite(buf, sizeof(char), nchars, LogF)) {
			perror("fwrite(LogF)");
		} else {
			LogDirty = TRUE;
		}
	}
	broadcast(buf, nchars, TP_DATA);
}

broadcast(buf, nchars, typ)
	unsigned char *buf;
	int nchars;
	unsigned int typ;
{
	register int fd, x;

	for (fd = 0;  fd <= highest_fd;  fd++) {
		if (!FD_ISSET(fd, &Clients)) {
			continue;
		}

		Sigpiped = 0;
		x = tp_senddata(fd, buf, nchars, typ);
		dprintf(stderr, "ttysrv.tty_input: %d bytes written on fd%d\n",
			x, fd);
		if (Sigpiped) {
			dprintf(stderr, "ttysrv.tty_input: sigpipe on fd%d\n",
				fd);
			close_client(fd);
		}
	}
}

serv_input(fd) {
	struct sockaddr_un in;
	int fromlen = sizeof(in);

	dprintf(stderr, "ttysrv.serv_input: accepting on fd%d\n", fd);

	if ((fd = accept(fd, &in, &fromlen)) == -1) {
		perror("accept");
		return;
	}

	dprintf(stderr, "ttysrv.serv_input: accepted fd%d\n", fd);

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0)|O_NONBLOCK);
	FD_SET(fd, &Clients);
	if (fd > highest_fd) {
		highest_fd = fd;
	}
	if (!WhosOn[fd]) {
		WhosOn[fd] = (struct whoson *) malloc(sizeof(struct whoson));
		WhosOn[fd]->who = NULL;
		WhosOn[fd]->lastInput = Now;
	}
}

client_input(fd) {
	register int nchars;
	register int i, new;
	register unsigned int o;

	if (WhosOn[fd]) {
		WhosOn[fd]->lastInput = Now;
	}

	/* read the fixed part of the ttyprot (everything but the array)
	 */
	if (TP_FIXED != (nchars = read(fd, &T, TP_FIXED))) {
		dprintf(stderr, "client_input: read=%d on fd%d: ", nchars, fd);
		if (Debug) perror("read");
		close_client(fd);
		return;
	}
	i = ntohs(T.i);
	o = ntohs(T.f) & TP_OPTIONMASK;
	switch (ntohs(T.f) & TP_TYPEMASK) {
	case TP_DATA:
		if (i != (nchars = read(fd, T.c, i))) {
			dprintf(stderr, "client_input: read#2=%d(%d) fd%d: ",
				nchars, i, fd);
			if (Debug) perror("read#2");
			close_client(fd);
			break;
		}
		dprintf(stderr, "ttysrv.client_input: %d bytes read on fd%d\n",
			nchars, fd);
		if (LogF) {
			if (nchars != fwrite(T.c, sizeof(char), nchars, LogF)){
				perror("fwrite(LogF)");
			} else {
				LogDirty = TRUE;
			}
		}
		nchars = write(Tty, T.c, nchars);
		dprintf(stderr, "ttysrv.client_input: wrote %d bytes @fd%d\n",
			nchars, Tty);
		break;
	case TP_BREAK:
		dprintf(stderr, "ttysrv.client_input: sending break\n");
		tcsendbreak(Tty, 0);
		tp_senddata(fd, "BREAK", 5, TP_NOTICE);
		dprintf(stderr, "ttysrv.client_input: done sending break\n");
		break;
	case TP_BAUD:
		if (o & TP_QUERY) {
			tp_sendctl(fd, TP_BAUD|TP_QUERY, Baud, NULL);
			break;
		}
		if (B0 == (new = find_baud(i))) {
			tp_sendctl(fd, TP_BAUD, 0, NULL);
		} else {
			Baud = i;
			SysBaud = new;
			set_baud();
			install_ttyios();
			tp_sendctl(fd, TP_BAUD, 1, NULL);
		}
		break;
	case TP_PARITY:
		if (o & TP_QUERY) {
			tp_sendctl(fd, TP_PARITY|TP_QUERY,
				   strlen(Parity), (unsigned char *)Parity);
			break;
		}
		if (i != (nchars = read(fd, T.c, i))) {
			dprintf(stderr, "client_input: read#2=%d(%d) fd%d: ",
				nchars, i, fd);
			if (Debug) perror("read#2");
			close_client(fd);
			break;
		}
		T.c[i] = '\0';
		if (-1 == (new = find_parity((char *)T.c))) {
			tp_sendctl(fd, TP_PARITY, 0, NULL);
		} else {
			strcpy(ParityBuf, (char *)T.c);
			Parity = ParityBuf;
			SysParity = new;
			set_parity();
			install_ttyios();
			tp_sendctl(fd, TP_PARITY, 1, NULL);
		}
		break;
	case TP_WORDSIZE:
		if (o & TP_QUERY) {
			tp_sendctl(fd, TP_WORDSIZE|TP_QUERY, Wordsize, NULL);
			break;
		}
		if (0 == (new = find_wordsize(i))) {
			tp_sendctl(fd, TP_WORDSIZE, 0, NULL);
		} else {
			Wordsize = i;
			SysWordsize = new;
			set_wordsize();
			install_ttyios();
			tp_sendctl(fd, TP_WORDSIZE, 1, NULL);
		}
		break;
	case TP_WHOSON:
		if (o & TP_QUERY) {
			int iwho;

			for (iwho = getdtablesize()-1;  iwho >= 0;  iwho--) {
				struct whoson *who = WhosOn[iwho];
				char data[TP_MAXVAR];
				int idle;

				if (!who)
					continue;
				idle = Now - who->lastInput;
				sprintf(data, "%s (idle %d sec%s)",
					who->who ?who->who :"undeclared",
					idle, (idle==1) ?"" :"s");
				tp_senddata(fd, data, strlen(data), TP_NOTICE);
			}
			break;
		}
		if (i != (nchars = read(fd, T.c, i))) {
			dprintf(stderr, "client_input: read#2=%d(%d) fd%d: ",
				nchars, i, fd);
			if (Debug) perror("read#2");
			close_client(fd);
			break;
		}
		dprintf(stderr, "ttysrv.client_input: %d bytes read on fd%d\n",
			nchars, fd);
		if (WhosOn[fd]) {
			if (!WhosOn[fd]->who) {
				WhosOn[fd]->who = malloc(i+1);
			} else {
				WhosOn[fd]->who = realloc(i+1);
			}
			strncpy(WhosOn[fd]->who, T.c, i);
			WhosOn[fd]->who[i] = '\0';
		}
		{ /*local*/
			char buf[TP_MAXVAR];

			sprintf(buf, "%-*.*s connected\07", i, i, T.c);
			broadcast(buf, strlen(buf), TP_NOTICE);
		}
		break;
	case TP_TAIL:
		if (!(o & TP_QUERY))
			break;
		if (!LogF)
			break;
		fflush(LogF);
		LogDirty = FALSE;
		if (0 > fseek(LogF, -1024, SEEK_END))
			if (0 > fseek(LogF, 0, SEEK_SET))
				break;
		{ /*local*/
			char buf[TP_MAXVAR];
			int len, something = FALSE;

			while (0 < (len = fread(buf, sizeof(char), sizeof buf,
						LogF))) {
				if (!something) {
					tp_senddata(fd, "tail+", 5, TP_NOTICE);
					something = TRUE;
				}
				tp_senddata(fd, buf, len, TP_DATA);
			}
			if (something) {
				tp_senddata(fd, "tail-", 5, TP_NOTICE);
			}
		}
		break;
	default:
		fprintf(stderr, "T.f was %04x\n", ntohs(T.f));
		break;
	}
}

close_client(fd) {
	dprintf(stderr, "close_client: fd%d\n", fd);
	close(fd);
	FD_CLR(fd, &Clients);
	if (WhosOn[fd]) {
		if (WhosOn[fd]->who) {
			char buf[TP_MAXVAR];

			sprintf(buf, "%s disconnected\07", WhosOn[fd]->who);
			broadcast(buf, strlen(buf), TP_NOTICE);
			free((char *) WhosOn[fd]->who);
			WhosOn[fd]->who = (char *) NULL;
		}
		free((char *) WhosOn[fd]);
		WhosOn[fd] = (struct whoson *) NULL;
	}
}

struct baudtab { int baud, sysbaud; } baudtab[] = {
	{ 300, B300 },
	{ 1200, B1200 },
	{ 2400, B2400 },
	{ 4800, B4800 },
	{ 9600, B9600 },
	{ 19200, B19200 },
	{ 38400, B38400 },
	{ 0, 0 }
};

struct partab { char *parity; unsigned int sysparity; } partab[] = {
	{ "even", PARENB },
	{ "odd", PARENB|PARODD },
	{ "none", 0 },
	{ NULL, 0 }
};

struct cstab { int wordsize, syswordsize; } cstab[] = {
	{ 5, CS5 },
	{ 6, CS6 },
	{ 7, CS7 },
	{ 8, CS8 },
	{ 0, 0 }
};

int
find_baud(baud)
	int baud;
{
	struct baudtab *baudp;
	int sysbaud = B0;

	for (baudp = baudtab;  baudp->baud;  baudp++) {
		if (baudp->baud == baud)
			sysbaud = baudp->sysbaud;
	}
	return sysbaud;
}

set_baud() {
	cfsetispeed(&Ttyios, SysBaud);
	cfsetospeed(&Ttyios, SysBaud);
}

int
find_parity(parity)
	char *parity;
{
	struct partab *parp;
	int sysparity = -1;

	for (parp = partab;  parp->parity;  parp++) {
		if (!strcmp(parp->parity, parity)) {
			sysparity = parp->sysparity;
		}
	}
	return sysparity;
}

set_parity() {
	Ttyios.c_cflag &= ~(PARENB|PARODD);
	Ttyios.c_cflag |= SysParity;
}

int
find_wordsize(wordsize)
	int wordsize;
{
	struct cstab *csp;
	int syswordsize = 0;

	for (csp = cstab;  csp->wordsize;  csp++) {
		if (csp->wordsize == wordsize)
			syswordsize = csp->syswordsize;
	}
	return syswordsize;
}

set_wordsize() {
	Ttyios.c_cflag &= ~CSIZE;
	Ttyios.c_cflag |= SysWordsize;
}
