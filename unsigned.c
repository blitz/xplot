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
#include "xplot.h"
#include <stdio.h>

#ifdef ultrix
#define LIBC_ATOI_IS_BROKEN
#endif

#ifdef linux
#define LIBC_ATOI_IS_BROKEN
#endif

#ifdef LIBC_ATOI_IS_BROKEN
#include <ctype.h>
#endif


char *unsigned_unparse(coord c)
{

  extern void panic();

  char *r;
  char buf[50];
  (void) sprintf(buf,"%u",c.u);
  r = malloc((unsigned) strlen(buf)+1);
  if (r == 0)
    panic("malloc returned 0");
  (void) strcpy(r, buf);
  return r;
}
	 
coord unsigned_parse(char *s)
{
  coord r;
#ifndef LIBC_ATOI_IS_BROKEN
  extern int atoi();

  r.u = atoi(s);
#else
  char *p;
  r.u = 0;
  p = s;

  while (isdigit(*p)) {
    r.u *= 10;
    r.u += (*p - '0');
    p++;
  }
  if (*p != '\0')
    fprintf(stderr,"warning: unsigned_parse format error in string: %s\n", s);
#endif  

  return r;
}

coord unsigned_zero(void)
{
  coord r;
  r.u = 0;
  return r;
}

int unsigned_cmp(coord c1, coord c2)
{
#ifdef SDO
  /* SDO's way, consider wrap around at edges of picture */
  int diff;
  
  diff = c1.u - c2.u;
  if (diff < 0)
    return(-1);
  else if (diff > 0)
    return(1);
  else
    return(0);
#else
  if (c1.u > c2.u) return 1;
  else if (c1.u < c2.u) return -1;
  else return 0;
#endif
}

coord unsigned_add(coord c1, coord c2)
{
  coord r;
  r.u = c1.u + c2.u;
  /****** need to check for overflow ? */
  return r;
}

coord unsigned_subtract(coord c1, coord c2)
{
  coord r;
  r.u = c1.u - c2.u;
  /****** need to check for overflow ? */
  return r;
}

coord unsigned_round_down(coord c1, coord c2)
{
  coord r;
  r.u = c1.u - (c1.u % c2.u);
  /****** need to check for overflow ? */
  return r;

}

coord unsigned_round_up(coord c1, coord c2)
{
  coord r;

  if (c1.u % c2.u == 0)
    r.u = c1.u;
  else
    r.u = c1.u + (c2.u - (c1.u % c2.u));

  return r;
}

coord unsigned_tick(int level)
{
  coord r;

  r.u = 1;
  while (level >= 3) {
    r.u *= 10;
    level -= 3;
  }

  switch (level) {
  case 2:
    r.u *= 5;
    level -= 2;
    break;
  case 1:
    r.u *= 2;
    level -= 1;
    break;
  case 0:
    break;
  }

  return r;
}

int unsigned_subtick(int level)
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



double unsigned_map(coord c1,coord c2, int n, coord c)
{
  double r;

  r = (((double) ((double) c.u - (double) c1.u)) *
	(((double) n) / ((double) (c2.u - c1.u))));

  return r;
}

coord unsigned_unmap(coord c1, coord c2, int n, double x)
{
  coord r;

  r.u = rint(((double) c1.u) + (((double) x) *
				(((double) (c2.u - c1.u)) / (double) n)));
  return r;
}

struct coord_impl unsigned_impl  = {
  unsigned_unparse,
  unsigned_parse,
  unsigned_zero,
  unsigned_cmp,
  unsigned_add,
  unsigned_subtract,
  unsigned_round_up,
  unsigned_round_down,
  unsigned_tick,
  unsigned_subtick,
  unsigned_map,
  unsigned_unmap
  };
