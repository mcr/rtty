/* ttysrv - serve a tty to stdin/stdout, a named pipe, or a network socket
 * vix 28may91 [written]
 *
 * $Id: ttysrv.c,v 1.1 1992-01-02 02:04:18 vixie Exp $
 */

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
#include <netinet/in.h>

#include "ttyprot.h"
#include "locbrok.h"
#include "rtty.h"

#define USAGE_STR \
	"-s Serv -t Tty -l Log -b Baud -p Parity -w Wordsize -i Pidfile"

extern int optind, opterr;
extern char *optarg;

char *ProgName;
char *ServSpec = NULL;	int Serv;
char *TtySpec = NULL;	int Tty = -1;	struct termios Ttyios, Ttyios_orig;
int Ttyios_set = 0;
char *LogSpec = NULL;	int Log = -1;
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

int Sigpiped = 0;
sigpipe() {
	Sigpiped++;
}

sighup() {
	if (Log != -1) {
		close(Log);
		open_log();
	}
}

open_log() {
	if (0 > (Log = open(LogSpec, O_CREAT|O_WRONLY|O_APPEND, 0644))) {
		fprintf(stderr, "%s: can't open log file ", ProgName);
		perror(LogSpec);
	}
}

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
		Ttyios.c_cflag = HUPCL|CLOCAL|CREAD;
		Ttyios.c_lflag = 0;
		Ttyios.c_iflag = 0;
		Ttyios.c_oflag = 0;
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
		nfound = select(highest_fd+1, &readfds, NULL, NULL, NULL);
		if (nfound < 0 && errno == EINTR)
			continue;
		ASSERT(0<=nfound, "select");
		dprintf(stderr, "ttysrv.main_loop: select->%d\n", nfound);
		for (fd = 0; fd <= highest_fd; fd++) {
			if (!FD_ISSET(fd, &readfds)) {
				continue;
			}
			dprintf(stderr, "ttysrv.main_loop: fd%d readable\n",
				fd);
			if (fd == Tty) {
				tty_input(fd);
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

tty_input(fd) {
	int nchars;
	unsigned char buf[TP_MAXVAR];

	nchars = read(fd, buf, TP_MAXVAR);
	if (nchars <= 0) {
		return;
	}
	dprintf(stderr, "ttysrv.tty_input: %d bytes read on fd%d\n",
		nchars, fd);
	if (Log != -1) {
		write(Log, buf, nchars);
	}
	for (fd = 0;  fd <= highest_fd;  fd++) {
		int x;

		if (!FD_ISSET(fd, &Clients)) {
			continue;
		}

		Sigpiped = 0;
		x = tp_senddata(fd, buf, nchars);
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

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0)|FNBLOCK);
	FD_SET(fd, &Clients);
	if (fd > highest_fd) {
		highest_fd = fd;
	}
}

client_input(fd) {
	char buf[TP_MAXVAR];
	register int nchars;
	register int i, new;
	register unsigned int o;

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
		if (Log != -1) {
			write(Log, buf, nchars);
		}
		nchars = write(Tty, T.c, nchars);
		dprintf(stderr, "ttysrv.client_input: wrote %d bytes @fd%d\n",
			nchars, Tty);
		break;
	case TP_BREAK:
		dprintf(stderr, "ttysrv.client_input: sending break\n");
		tcsendbreak(Tty, 0);
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
	default:
		fprintf(stderr, "T.f was %04x\n", ntohs(T.f));
		break;
	}
}

close_client(fd) {
	dprintf(stderr, "close_client: fd%d\n", fd);
	close(fd);
	FD_CLR(fd, &Clients);
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
	Ttyios.c_cflag |= SysWordsize;
}

