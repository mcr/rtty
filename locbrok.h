/* locbrok.h - defs for location broker
 * vix 13sep91 [written]
 *
 * $Id: locbrok.h,v 1.3 1996-08-23 22:09:30 vixie Exp $
 */

#define	LB_SERVNAME	"locbrok"
#define LB_SERVPORT	160
#define	LB_MAXNAMELEN	64

typedef struct locbrok {
	u_short	lb_port;
	u_short	lb_nlen;
	char	lb_name[LB_MAXNAMELEN];
} locbrok;
