/* ttyproto.h - define protocol used by ttysrv and its clients
 * vix 29may91 [written]
 *
 * $Id: ttyprot.h,v 1.1 1992-01-02 02:04:18 vixie Exp $
 */

#define TP_TYPEMASK	0x00ff
#define	TP_DATA		0x0001
#define	TP_BAUD		0x0002
#define	TP_PARITY	0x0003
#define	TP_WORDSIZE	0x0004
#define	TP_BREAK	0x0005

#define	TP_OPTIONMASK	0xff00
#define TP_QUERY	0x0100

#define	TP_FIXED	(sizeof(unsigned short) + sizeof(unsigned short))
#define	TP_MAXVAR	64

typedef struct ttyprot {
	unsigned short	f;
	unsigned short	i;
	unsigned char	c[TP_MAXVAR];
} ttyprot;
