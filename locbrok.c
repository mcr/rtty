/* locbrok - location broker
 * vix 13sep91 [written]
 *
 * $Id: locbrok.c,v 1.1 1992-01-02 02:04:18 vixie Exp $
 */

#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rtty.h"
#include "locbrok.h"

#define USAGE_STR "[-s service] [-x debuglev]"
extern int optind, opterr;
extern char *optarg;
extern char *malloc();

typedef struct reg_db {
	char *name;
	u_short port;
	u_short client;
	struct reg_db *next;
} reg_db;

char *ProgName = "?";
char *Service = LB_SERVNAME;
int Port;
int Debug = 0;
fd_set Clients;
int MaxFD;
reg_db *RegDB = NULL;

main(argc, argv)
	int argc;
	char *argv[];
{
	char ch;
	struct servent *serv;

	ProgName = argv[0];
	while ((ch = getopt(argc, argv, "s:x:7")) != EOF) {
		switch (ch) {
		case 's':
			Service = optarg;
			break;
		case 'x':
			Debug = atoi(optarg);
			break;
		default:
			USAGE((stderr, "%s: getopt=%c ?\n", ProgName, ch));
		}
	}

	if (0 != (Port = atoi(Service))) {
		/* numeric service; we're ok */
		;
        } else if (NULL != (serv = getservbyname(Service, "tcp"))) {
		/* found the service name; we're ok */
		Port = serv->s_port;
        } else {
		/* nothing worked; use default */
		Port = LB_SERVPORT;
		fprintf(stderr, "%s: service `%s' not found, using port %d\n",
			ProgName, Service, Port);
	}

	server();
}

server() {
	int serv, on = 1;
	struct sockaddr_in name;

	serv = socket(PF_INET, SOCK_STREAM, 0);
	ASSERT(serv>=0, "socket")
	setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof on);

	name.sin_family = AF_INET;
	name.sin_addr.s_addr = INADDR_ANY;
	name.sin_port = Port;
	ASSERT(bind(serv, &name, sizeof name)>=0, "bind")

	FD_ZERO(&Clients);
	MaxFD = serv;
	listen(serv, 5);
	while (TRUE) {
		fd_set readfds;
		int nset;
		register int fd;
		extern int errno;

		readfds = Clients;
		FD_SET(serv, &readfds);
		nset = select(MaxFD+1, &readfds, NULL, NULL, NULL);
		if (nset < 0 && errno == EINTR)
			continue;
		ASSERT(nset>=0, "assert")
		for (fd = 0;  fd <= MaxFD;  fd++) {
			if (!FD_ISSET(fd, &readfds))
				continue;
			if (fd == serv) {
				int namesize = sizeof name;
				int a, b, c, d, local, cl;

				ASSERT((cl=accept(serv, &name, &namesize))>=0,
				       "accept")
				a = name.sin_addr.S_un.S_un_b.s_b1;
				b = name.sin_addr.S_un.S_un_b.s_b2;
				c = name.sin_addr.S_un.S_un_b.s_b3;
				d = name.sin_addr.S_un.S_un_b.s_b4;
				local = (	(!a && !b && !c && !d)
					 ||	(a==127 && !b && !c && d==1)
					 );
				fprintf(stderr,
					"accept from %d.%d.%d.%d (%slocal)\n",
					a, b, c, d, local?"":"not ");
				FD_SET(cl, &Clients);
				if (cl > MaxFD)
					MaxFD = cl;
				continue;
			}
			if (FD_ISSET(fd, &Clients)) {
				client_input(fd);
			}
		}
	}
}

client_input(fd) {
	locbrok	lb;
	reg_db *db;
	int keepalive = 0;
	reg_db *find_byname(), *find_byport();

	if (0 >= read(fd, &lb, sizeof lb)) {
		fputs("locbrok.client_input: ", stderr);
		perror("read");
		goto death;
	}
	lb.lb_port = ntohs(lb.lb_port);
	lb.lb_nlen = ntohs(lb.lb_nlen);
	if (lb.lb_nlen >= LB_MAXNAMELEN) {
		fprintf(stderr, "client_input: fd%d sent oversize req\n", fd);
		goto death;
	}
	lb.lb_name[lb.lb_nlen] = '\0';
	fprintf(stderr, "client_input(fd %d, port %d, %d:`%s')\n",
		fd, lb.lb_port, lb.lb_nlen, lb.lb_name);

	if (lb.lb_port && !lb.lb_nlen) {
		if (NULL != (db = find_byport(lb.lb_port))) {
			lb.lb_nlen = min(strlen(db->name), LB_MAXNAMELEN);
			strncpy(lb.lb_name, db->name, lb.lb_nlen);
		}
	} else if (!lb.lb_port && lb.lb_nlen) {
		if (NULL != (db = find_byname(lb.lb_name))) {
			lb.lb_port = db->port;
		}
	} else if (lb.lb_port && lb.lb_nlen) {
		if (add(lb.lb_name, lb.lb_port, fd) == -1) {
			lb.lb_nlen = 0;
		} else {
			keepalive++;
			print();
		}
	} else {
		fprintf(stderr, "bogus client_input (port,nlen both 0)\n");
		goto death;
	}
	lb.lb_port = htons(lb.lb_port);
	lb.lb_nlen = htons(lb.lb_nlen);
	write(fd, &lb, sizeof lb);
	if (keepalive)
		return;
death:
	close(fd);
	FD_CLR(fd, &Clients);
	rm_byclient(fd);
	print();
}

reg_db *
find_byname(name)
	char *name;
{
	reg_db *db;

	for (db = RegDB;  db;  db = db->next)
		if (!strcmp(name, db->name))
			return db;
	return NULL;
}

reg_db *
find_byport(port)
	u_short port;
{
	reg_db *db;

	for (db = RegDB;  db;  db = db->next)
		if (port == db->port)
			return db;
	return NULL;
}

int
add(name, port, client)
	char *name;
	u_short port;
	u_short client;
{
	reg_db *db;

	if (find_byname(name) || find_byport(port))
		return -1;
	db = (reg_db *) malloc(sizeof(reg_db));
	db->name = malloc(strlen(name)+1);
	strcpy(db->name, name);
	db->port = port;
	db->client = client;
	db->next = RegDB;
	RegDB = db;
	return 0;
}

rm_byclient(client)
	u_short client;
{
	register reg_db *cur = RegDB, *prev = NULL;

	while (cur) {
		if (cur->client == client) {
			register reg_db *tmp = cur;

			if (prev)
				prev->next = cur->next;
			else
				RegDB = cur->next;
			cur = cur->next;

			free(tmp->name);
			free(tmp);
		} else {
			prev = cur;
			cur = cur->next;
		}
	}
}

print() {
	reg_db *db;

	fprintf(stderr, "db:\n");
	for (db = RegDB;  db;  db = db->next)
		fprintf(stderr, "(%s %d %d)\n",
			db->name, db->port, db->client);
	fprintf(stderr, "---\n");
}
