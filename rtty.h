/* rtty.h - definitions for rtty package
 * vix 01nov91 [written]
 *
 * $Id: rtty.h,v 1.7 1996-08-23 21:39:14 vixie Exp $
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

extern	void	*safe_malloc __P((size_t)),
		*safe_calloc __P((size_t, size_t)),
		*safe_realloc __P((void *, size_t));
			
#ifdef NEED_STRDUP
extern	char	*strdup __P((const char *));
#endif
