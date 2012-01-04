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

char *signed_unparse(coord c)
{
  extern void panic();

  char *r;
  char buf[50];
  (void) sprintf(buf,"%d",c.i);
  r = malloc((unsigned) strlen(buf)+1);
  if (r == 0)
    panic("malloc returned 0");
  (void) strcpy(r, buf);
  return r;
}
	 
coord signed_parse(char *s)
{
  coord r;
  extern int atoi();

  r.i = atoi(s);
  return r;
}

coord signed_zero(void)
{
  coord r;
  r.i = 0;
  return r;
}

int signed_cmp(coord c1, coord c2)
{
  if (c1.i > c2.i) return 1;
  else if (c1.i < c2.i) return -1;
  else return 0;
}

coord signed_add(coord c1, coord c2)
{
  coord r;
  r.i = c1.i + c2.i;
  /******** should check for overflow ?*/
  return r;
}

coord signed_subtract(coord c1, coord c2)
{
  coord r;
  r.i = c1.i - c2.i;
  /******** should check for underflow ?*/
  return r;
}

coord signed_round_up(coord c1, coord c2)
{
  coord r;
  if (c1.i % c2.i == 0)
    r.i = c1.i;
  else
    {
      /* r.i = c1.i + (c2.i - (c1.i % c2.i));  except c1.i % c2.i might be - */
      r.i = c1.i % c2.i;
      if (r.i < 0) {
	r.i += c2.i;
      }
      r.i = c1.i + (c2.i - r.i);
    }
  return r;
}

coord signed_round_down(coord c1, coord c2)
{
  coord r;
  r.i = c1.i - (c1.i % c2.i);
  return r;

}

coord signed_tick(int level)
{
  coord r;

  r.i = 1;
  while (level >= 3) {
    r.i *= 10;
    level -= 3;
  }
  
  switch (level) {
  case 2:
    r.i *= 5;
    level -= 2;
    break;
  case 1:
    r.i *= 2;
    level -= 1;
    break;
  case 0:
    break;
  }

  return r;
}

int signed_subtick(int level)
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



double signed_map(coord c1, coord c2, int n, coord c)
{
  double r;

  r = (((double) ((double) c.i - (double) c1.i)) *
	(((double) n) / ((double) (c2.i - c1.i))));

  return r;
}

coord signed_unmap(coord c1, coord c2, int n, double x)
{
  coord r;

  r.i = rint(((double) c1.i) + (((double) x) *
				(((double) (c2.i - c1.i)) / (double) n)));
  return r;
}

struct coord_impl signed_impl  = {
  signed_unparse,
  signed_parse,
  signed_zero,
  signed_cmp,
  signed_add,
  signed_subtract,
  signed_round_up,
  signed_round_down,
  signed_tick,
  signed_subtick,
  signed_map,
  signed_unmap
  };
