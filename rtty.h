/* rtty.h - definitions for rtty package
 * vix 01nov91 [written]
 *
 * $Id: rtty.h,v 1.6 1994-05-16 22:36:07 vixie Exp $
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

#ifdef DEBUG
#define dprintf if (Debug) fprintf
#else
#define	fprintf (void)
#endif

#ifndef __P
# ifdef __STDC__
#  define __P(x) x
# else
#  define __P(x) ()
#  define const
# endif
#endif

#if (BSD >= 199103) || defined(ultrix) || defined(sun)
# define USE_STDLIB
#endif

#if (BSD >= 199103) || defined(ultrix) || defined(sun)
# define USE_UNISTD
#endif

/* something in ULTRIX that we want to use if it's there */
#ifndef TAUTOFLOW
#define TAUTOFLOW 0
#endif

#ifdef NEED_STRDUP
extern	char	*strdup __P((const char *));
#endif
