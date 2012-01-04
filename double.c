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
#include "xplot.h"

extern double atof();

#if defined(linux) || defined(ultrix)
#define remainder drem
#endif
extern double remainder();

char *
double_unparse(coord c)
{
  extern void panic();

  char *r;
  char buf[50];

  if ( fabs(c.d) > 1E-16 )	/* XXX kludge */
    (void) sprintf(buf, "%.10g", c.d);
  else
    (void) sprintf(buf, "0");

  r = malloc((unsigned) strlen(buf)+1);
  if (r == 0)
    panic("malloc returned 0");
  (void) strcpy(r, buf);
  return r;
}

coord
double_parse(char *s)
{
  coord r;
  r.d = atof(s);
  return r;
}

coord
double_zero(void)
{
  coord r;
  r.d = 0.0;
  return r;
}

int
double_cmp(coord c1, coord c2)
{
  if (c1.d > c2.d) return 1;
  else if (c1.d < c2.d) return -1;
  else return 0;
}

coord
double_add(coord c1, coord c2)
{
  coord r;
  r.d = c1.d + c2.d;
  return r;
}

coord
double_subtract(coord c1, coord c2)
{
  coord r;
  r.d = c1.d - c2.d;
  return r;
}

coord
double_round_up(coord c1, coord c2)
{
  coord r;

  double rem = remainder(c1.d, c2.d);
  if (rem > 0.0)
    r.d = c1.d + (c2.d - rem);
  else
    r.d = c1.d - rem;
  return r;
}

coord
double_round_down(coord c1, coord c2)
{
  coord r;

  double rem = remainder(c1.d, c2.d);
  if (rem > 0.0)
    r.d = c1.d - rem;
  else
    r.d = c1.d - (c2.d + rem);
  return r;
}


coord
double_tick(int level)
{
  coord r;

  r.d = 1.0;

#define MIDLEVEL 60

  while ( level < MIDLEVEL - 3 ) {
    r.d /= 10.0;
    level += 3;
  }

  while (level < MIDLEVEL) {
    r.d /= 2.0;			/* to 5 */
    level++;
    if (level < MIDLEVEL) {
      r.d /= 2.5;		/* to 2 */
      level++;
    }
    if (level < MIDLEVEL) {
      r.d /= 2.0;		/* to 1 */
      level++;
    }
  }
    
  while ( level > MIDLEVEL + 3 ) {
    r.d *= 10.0;
    level -= 3;
  }

  while (level > MIDLEVEL)  {
    r.d *= 2.0;			/* to 2 */
    level--;
    if (level > MIDLEVEL)  {
      r.d *= 2.5;		/* to 5 */
      level--;
    }
    if (level > MIDLEVEL)  {
      r.d *= 2.0;		/* to 1 */
      level--;
    }
  }
  return r;
}

int double_subtick(int level)
{
  int r = level;

  if (level < 2) r = 0;
  else
    switch (level%3) {
    case 2:
      r = level - 2;
      break;
    case 1:
      r = level - 1;
      break;
    case 0:
      r = level - 1;
      break;
    }

  return r;
}


double 
double_map(coord c1, coord c2, int n, coord c)
{
  double r;

  r = ((c.d - c1.d) / (c2.d - c1.d)) * (double) n;
  return r;
}

coord
double_unmap(coord c1,coord c2, int n, double x)
{
  coord r;

  r.d = ((double)x / (double)n) * (c2.d - c1.d) + c1.d;
  return r;
}

struct coord_impl double_impl = {
  double_unparse,
  double_parse,
  double_zero,
  double_cmp,
  double_add,
  double_subtract,
  double_round_up,
  double_round_down,
  double_tick,
  double_subtick,
  double_map,
  double_unmap
};
