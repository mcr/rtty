/* locbrok.h - defs for location broker
 * vix 13sep91 [written]
 *
 * $Id: locbrok.h,v 1.2 1993-12-28 00:49:56 vixie Exp $
 */

#define	LB_SERVNAME	"locbrok"
#define LB_SERVPORT	160
#define	LB_MAXNAMELEN	64

typedef struct locbrok {
	unsigned short	lb_port;
	unsigned short	lb_nlen;
	char		lb_name[LB_MAXNAMELEN];
} locbrok;
