/* rtty.h - definitions for rtty package
 * vix 01nov91 [written]
 *
 * $Id: rtty.h,v 1.1 1992-01-02 02:04:18 vixie Exp $
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


