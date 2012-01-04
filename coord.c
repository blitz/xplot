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
#include "xplot.h"

extern struct coord_impl unsigned_impl;
extern struct coord_impl signed_impl;
extern struct coord_impl timeval_impl;
extern struct coord_impl double_impl;
extern struct coord_impl dtime_impl;

/* kludge kludge.... but at least all this grossness is mostly 
 * confined to the next 16 or so lines */

struct coord_impl *impls[] = {
  &unsigned_impl,
  &signed_impl,
  &timeval_impl,
  &double_impl,
  &dtime_impl
  };

coord_type parse_coord_name(char *s)
{
  if (strcmp(s,"unsigned") == 0) return U_INT;
  else if (strcmp(s,"signed") == 0) return INT;
  else if (strcmp(s,"timeval") == 0) return TIMEVAL;
  else if (strcmp(s,"double") == 0) return DOUBLE;
  else if (strcmp(s,"dtime") == 0) return DTIME;
  else return ((coord_type) -1);
}

char *unparse_coord(coord_type ctype,coord c)
{
  return  impls[(int)ctype]->unparse(c);
}

coord parse_coord(coord_type ctype,char *s)
{
  return  impls[(int)ctype]->parse(s);
}


#ifndef cmp_coord
/* this is now a macro */
int cmp_coord(coord_type ctype, coord c1, coord c2)
{
  return  impls[(int)ctype]->cmp(c1, c2);
}
#endif

double map_coord(coord_type ctype, coord first, coord last, int n, coord c)
{
  return  impls[(int)ctype]->map(first, last, n, c);
}

coord unmap_coord(coord_type ctype,
		  coord first, coord last,
		  int n,
		  double i)
{
  return impls[(int)ctype]->unmap(first, last, n, i);
}

void cticks(coord_type ctype, coord first, coord last, int horizontal,
	    void (*pp)(coord c, int labelflag))
{
  int level;
  int sublevel;
  coord at;
  coord step;
  int count;
  int subcount;
  int maxextrasubticks;

#define MAXTICKS 6

  if (horizontal) {
    maxextrasubticks = MAXTICKS + 1;
  } else {
    maxextrasubticks = ( MAXTICKS + 1 ) * 2;
  }
  /* start with the smallest tick level */
  for (level = 0; ; level++) {
    step = impls[(int)ctype]->tick(level);
    at = impls[(int)ctype]->round_up(first, step);
    
    for (count=1; count <= MAXTICKS; count++) {
      at = impls[(int)ctype]->add(at, step);
      if (impls[(int)ctype]->cmp(at, last) > 0)
	break;
    } /* count is now number of ticks that fit */

    /* if <=  max allowed, use this level */
    if (count <= MAXTICKS) break;
  }

  step = impls[(int)ctype]->tick(level);
    
  at = impls[(int)ctype]->round_up(first, step);

  while (impls[(int)ctype]->cmp(at, last) <= 0) {
    pp(at,1);
    at = impls[(int)ctype]->add(at, step);
  }
  sublevel = impls[(int)ctype]->subtick(level);
  if (sublevel != level) {

    step = impls[(int)ctype]->tick(sublevel);
    at = impls[(int)ctype]->round_up(first, step);
    
    for (subcount = 1; subcount - count <= maxextrasubticks; subcount++) {
      at = impls[(int)ctype]->add(at, step);
      if (impls[(int)ctype]->cmp(at, last) > 0)
	break;
    }
    
    if (subcount - count <= maxextrasubticks) {
      at = impls[(int)ctype]->round_up(first, step);
      while (impls[(int)ctype]->cmp(at, last) <= 0) {
	pp(at,0);
	at = impls[(int)ctype]->add(at, step);
      }
    }
  }

      
}

coord bump_coord(coord_type ctype, coord c)
{
  int level = 0;
  coord r;
  do {
    r = impls[(int)ctype]->add(c, impls[(int)ctype]->tick(level++));
  } while (impls[(int)ctype]->cmp(c,r) == 0);
  return r;
}


void zoom_in_coord(coord_type ctype, coord first, coord last,
		   int x1, int x2, 
		   int n,
		   coord *newfirst, coord *newlast)
{
  int xf,xl;

#ifdef SDO
  if (x2 < 0) x2 = 0;
#endif

  if (x1 < x2) {
    xl = x2; xf = x1;
  } else {
    xl = x1; xf = x2;
  }

  *newfirst = impls[(int)ctype]->unmap(first, last, n, xf);
  *newlast  = impls[(int)ctype]->unmap(first, last, n, xl);

  if (impls[(int)ctype]->cmp(*newfirst, *newlast) == 0) {
    *newfirst = first;
    *newlast = last;
  }

  return;
}


void drag_coord(coord_type ctype, coord first, coord last,
		int x1, int x2,
		int n,
		coord *newfirst, coord *newlast)
{
  coord c1,c2;
  coord diff;

  c1 = impls[(int)ctype]->unmap(first, last, n, x2);
  c2 = impls[(int)ctype]->unmap(first, last, n, x1);
  
  switch(impls[(int)ctype]->cmp(c1,c2)) {
  case 1:
    diff = impls[(int)ctype]->subtract(c1, c2);
    *newfirst = impls[(int)ctype]->add(first, diff);
    *newlast  = impls[(int)ctype]->add(last, diff);
    break;
  case 0:
    *newfirst = first;
    *newlast  = last;
    break;
  case -1:
    diff = impls[(int)ctype]->subtract(c2, c1);
    *newfirst = impls[(int)ctype]->subtract(first, diff);
    *newlast  = impls[(int)ctype]->subtract(last, diff);
    break;
  }
    
  return;
}
