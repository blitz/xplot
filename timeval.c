/* 
This software is being provided to you, the LICENSEE, by the
Massachusetts Institute of Technology (M.I.T.) under the following
license.  By obtaining, using and/or copying this software, you agree
that you have read, understood, and will comply with these terms and
conditions:

Permission to use, copy, modify and distribute, including the right to
grant others the right to distribute at any tier, this software and
its documentation for any purpose and without fee or royalty is hereby
granted, provided that you agree to comply with the following
copyright notice and statements, including the disclaimer, and that
the same appear on ALL copies of the software and documentation,
including modifications that you make for internal use or for
distribution:

Copyright 1992,1993 by the Massachusetts Institute of Technology.
                    All rights reserved.

THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS
OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF
MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE
OF THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD
PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.

The name of the Massachusetts Institute of Technology or M.I.T. may
NOT be used in advertising or publicity pertaining to distribution of
the software.  Title to copyright in this software and any associated
documentation shall at all times remain with M.I.T., and USER agrees
to preserve same.
*/
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "xplot.h"

#ifndef TM_GMTOFF
extern time_t timezone;
#ifdef HAS_ALTZONE
extern time_t altzone;
#else /* no altzone */
#define altzone (timezone - 3600)
#endif /* HAS_ALTZONE */
#endif /* TM_GMTOFF */

extern int daylight;
extern char *tzname[2];


inline static
void
timeval_fix(coord *r)
{
  while (r->t.tv_usec < 0) {
    r->t.tv_usec += 1000000;
    r->t.tv_sec  -= 1;
  }

  while (r->t.tv_usec >= 1000000) {
    r->t.tv_usec -= 1000000;
    r->t.tv_sec  += 1;
  }
}


char *timeval_unparse(coord c)
{
  char *cp;
  char *r;
  char buf[50];
  extern void panic();
  struct tm *tmp;
  
  tmp = localtime((time_t *) &(c.t.tv_sec));
  (void) sprintf(buf,"%s",asctime(tmp));

  if (c.t.tv_usec == 0 && tmp->tm_sec == 0 && tmp->tm_min == 0 && tmp->tm_hour == 0) {
    cp = buf+4;
    sprintf(cp+7,"midn");
  } else if (c.t.tv_usec == 0 && tmp->tm_sec == 0 && tmp->tm_min == 0 && tmp->tm_hour == 12) {
    cp = buf+4;
    sprintf(cp+7,"noon");
  } else {
    cp = buf+10;
    cp[10] = '\0';
    if (c.t.tv_usec != 0) {
      if (c.t.tv_usec % 100 == 0) {
	(void) sprintf(cp+9,".%04u",(unsigned) c.t.tv_usec/100);
	cp += 7;
      } else {
	(void) sprintf(cp+9,".%06u",(unsigned) c.t.tv_usec);
	cp += 9;
      }
    }
  }
  r = malloc((unsigned) strlen(cp)+1);
  if (r == 0)
    panic("malloc returned 0");
  (void) strcpy(r, cp);
  return r;
}
	 
coord timeval_parse(char *s)
{
  coord r;
  extern int atoi();

  r.t.tv_usec = 0;

  /* quick, dirty, and unsafe */
  r.t.tv_sec = atoi(s);
  while (isdigit(*s)) s++;

  if (*s == '.') {
    s++;
    r.t.tv_usec = atoi(s);
    
    {
      int len = 0;
      while (isdigit(*s++)) len++;
      while (len < 6) (len++, r.t.tv_usec *= 10);
      while (len > 6) (len--, r.t.tv_usec /= 10);
    }
  }
  
  timeval_fix(&r);
  return r;
}

coord timeval_zero(void)
{
  coord r;
  r.t.tv_sec = 0;
  r.t.tv_usec = 0;
  return r;
}

int timeval_cmp(coord c1, coord c2)
{
  int r;

  if (c1.t.tv_sec > c2.t.tv_sec) r = 1;
  else if (c1.t.tv_sec < c2.t.tv_sec) r = -1;
  else if (c1.t.tv_usec > c2.t.tv_usec) r = 1;
  else if (c1.t.tv_usec < c2.t.tv_usec) r = -1;
  else r = 0;

  return r;
}

coord timeval_add(coord c1, coord c2)
{
  coord r;

  r.t.tv_sec  = c1.t.tv_sec  + c2.t.tv_sec;
  r.t.tv_usec = c1.t.tv_usec + c2.t.tv_usec;

  timeval_fix(&r);
  return r;
}

coord timeval_subtract(coord c1,coord c2)
{
  coord r;

  timeval_fix(&r);

  if (c2.t.tv_usec > c1.t.tv_usec) {
    c1.t.tv_sec  -= 1;
    c1.t.tv_usec += 1000000;
  }

  r.t.tv_sec  = c1.t.tv_sec  - c2.t.tv_sec;
  r.t.tv_usec = c1.t.tv_usec - c2.t.tv_usec;

  return r;
}

coord timeval_round_down(coord c1, coord c2)
{
  coord r;
  struct tm *tmp;
  time_t gmtoff;

  tmp = localtime((time_t *) &(c1.t.tv_sec));
#ifdef TM_GMTOFF
  gmtoff = tmp->tm_gmtoff;
#else
  if (tmp->tm_isdst==0) {
	gmtoff = -timezone;
  } else if (tmp->tm_isdst==1) {
	gmtoff = -altzone;
  } else {
	gmtoff = 0;
  }
 #endif /* TM_GMTOFF */
  c1.t.tv_sec += gmtoff;
 
  if (c2.t.tv_sec == 0) {
    r.t.tv_sec = c1.t.tv_sec;
    r.t.tv_usec = (c1.t.tv_usec - (c1.t.tv_usec % c2.t.tv_usec));
  } else {
    r.t.tv_usec = 0;
    r.t.tv_sec = c1.t.tv_sec - (c1.t.tv_sec % c2.t.tv_sec);
  }

  r.t.tv_sec -= gmtoff;

  timeval_fix(&r);
  return r;
}


coord timeval_round_up(coord c1, coord c2)
{
  coord r;
  struct tm *tmp;
  time_t gmtoff;

  tmp = localtime((time_t *) &(c1.t.tv_sec));

#ifdef TM_GMTOFF
  gmtoff = tmp->tm_gmtoff;
#else
  if (tmp->tm_isdst==0) {
	gmtoff = -timezone;
  } else if (tmp->tm_isdst==1) {
	gmtoff = -altzone;
  } else {
	gmtoff = 0;
  }
 #endif /* TM_GMTOFF */
  c1.t.tv_sec += gmtoff;
 
  if (c2.t.tv_sec == 0) {
    r.t.tv_sec  = c1.t.tv_sec;
    if (c1.t.tv_usec % c2.t.tv_usec == 0)
      r.t.tv_usec = c1.t.tv_usec;
    else {
      r.t.tv_usec = c1.t.tv_usec + (c2.t.tv_usec -
				    (c1.t.tv_usec % c2.t.tv_usec));
      if (r.t.tv_usec >= 1000000) {
	r.t.tv_usec -= 1000000;
	r.t.tv_sec  += 1;
      }
    }
  } else {
    r.t.tv_usec = 0;
    if (c1.t.tv_sec % c2.t.tv_sec == 0)
      r.t.tv_sec = c1.t.tv_sec;
    else
      r.t.tv_sec = c1.t.tv_sec + (c2.t.tv_sec - (c1.t.tv_sec % c2.t.tv_sec));
  }

  r.t.tv_sec -= gmtoff;

  timeval_fix(&r);
  return r;
  
#if 0
  return timeval_round_down(timeval_add(c1,c2), c2);
#endif

}

static struct tick_table_s {
  struct timeval step;
  int subtick_offset;
} tick_table[] = {
  {{ 0, 1 }, 0 },
  {{ 0, 2 }, -1 },
  {{ 0, 5 }, -2 },

  {{ 0, 10 }, -2 },
  {{ 0, 20 }, -2 },
  {{ 0, 50 }, -2 },

  {{ 0, 100 }, -2 },
  {{ 0, 200 }, -2 },
  {{ 0, 500 }, -2 },

  {{ 0, 1000 }, -2 },
  {{ 0, 2000 }, -2 },
  {{ 0, 5000 }, -2 },

  {{ 0, 10000 }, -2 },
  {{ 0, 20000 }, -2 },
  {{ 0, 50000 }, -2 },

  {{ 0, 100000 }, -2 },
  {{ 0, 200000 }, -2 },
  {{ 0, 500000 }, -2 },

  {{ 1, 0 }, -2 },
  {{ 2, 0 }, -2 },
  {{ 5, 0 }, -2 },
  {{ 10, 0 }, -2 },
  {{ 20, 0 }, -2 },
  {{ 30, 0 }, -2 },

  {{ 1*60, 0 }, -2 },
  {{ 2*60, 0 }, -2 },
  {{ 5*60, 0 }, -2 },
  {{ 10*60, 0 }, -2 },
  {{ 20*60, 0 }, -2 },
  {{ 30*60, 0 }, -2 },

  {{ 1*60*60, 0 }, -2 },
  {{ 2*60*60, 0 }, -2 },
  {{ 6*60*60, 0 }, -2 },
  {{ 12*60*60, 0 }, -2 },
  {{ 24*60*60, 0 }, -2 },
  {{ 2*24*60*60, 0 }, -2 },
  {{ 5*24*60*60, 0 }, -2 },
  {{ 10*24*60*60, 0 }, -2 },
  {{ 20*24*60*60, 0 }, -2 },
  {{ 50*24*60*60, 0 }, -2 },
  {{ 100*24*60*60, 0 }, -2 },
  {{ 200*24*60*60, 0 }, -2 },
  {{ 500*24*60*60, 0 }, -2 },
  {{ 1000*24*60*60, 0 }, -2 },
  {{ 2000*24*60*60, 0 }, -2 },
  {{ 5000*24*60*60, 0 }, -2 },
  {{ 10000*24*60*60, 0 }, -2 },
};

coord timeval_tick(int level)
{
  coord r;
  extern void ianic();


  if (level < 0 || level >= (int)(sizeof(tick_table) / sizeof(struct tick_table_s)))
    panic("timeval_tick: level too large");
  
  r.t = tick_table[level].step;
  
  timeval_fix(&r);
  return r;
}

int timeval_subtick(int level)
{
  int r;
  extern void panic();

  if (level < 0 || level >= (int)(sizeof(tick_table) / sizeof(struct tick_table_s)))
    panic("timeval_subtick: level too large");
  
  r = level + tick_table[level].subtick_offset;

  return r;
}


double timeval_map(coord c1,coord c2, int n, coord c)
{
  double r;
  double d;
  double dc;

  d  = (((double) ((int) (c2.t.tv_sec  - c1.t.tv_sec))) * 1000000 +
        ((double) ((int) (c2.t.tv_usec - c1.t.tv_usec))));
  dc = (((double) ((int) (c.t.tv_sec   - c1.t.tv_sec))) * 1000000 +
	((double) ((int) (c.t.tv_usec  - c1.t.tv_usec))));
    
  r = dc/d * ((double) n);
  
  return r;
}

coord timeval_unmap(coord c1,coord c2, int n, double x)
{
  coord r;
  double d;

  d = (((double) (c2.t.tv_sec  - c1.t.tv_sec)) * 1000000 +
       ((double) (c2.t.tv_usec - c1.t.tv_usec)));
  d /= n;
  d *= x;
  d += ((double) c1.t.tv_sec * 1000000) + (double) c1.t.tv_usec;
  
  r.t.tv_sec = floor(d/1000000.);
  r.t.tv_usec = rint(d - (((double) floor(d/1000000.)) * 1000000.));

  return r;
}

struct coord_impl timeval_impl  = {
  timeval_unparse,
  timeval_parse,
  timeval_zero,
  timeval_cmp,
  timeval_add,
  timeval_subtract,
  timeval_round_up,
  timeval_round_down,
  timeval_tick,
  timeval_subtick,
  timeval_map,
  timeval_unmap
  };
