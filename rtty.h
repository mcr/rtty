/* rtty.h - definitions for rtty package
 * vix 01nov91 [written]
 *
 * $Id: rtty.h,v 1.2 1992-06-23 16:27:18 vixie Exp $
 */

#define ASSERT2(e, m1, m2)	if (!(e)) {int save_errno=errno;\
					   fprintf(stderr, "%s: %s: ",\
					           ProgName, m2);\
					   errno=save_errno;\
					   perror(m1); exit(1);}
#define ASSERT(e, m)		ASSERT2(e, m, "")
#define USAGE(x)		{ fprintf x;\
				  fprintf(stderr, "usage:  %s %s\n",\
					  ProgName, USAGE_STR);\
				  exit(1); }
#define TRUE 1
#define FALSE 0
#define min(a,b) ((a>b)?b:a)
#define max(a,b) ((a>b)?a:b)
#define dprintf if (Debug) fprintf
#define STDIN 0
#define STDOUT 1

#ifndef TAUTOFLOW
#define TAUTOFLOW 0
#endif

#define	INITIAL_CFLAG \
	(HUPCL|CLOCAL|CREAD|TAUTOFLOW)

#define INITIAL_LFLAG \
	~(ISIG|ICANON|NOFLSH|TOSTOP|ECHO|ECHOE|ECHOK|ECHONL|IEXTEN)

#define INITIAL_IFLAG \
    ~(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF)

#define	INITIAL_OFLAG \
	~(OPOST)
