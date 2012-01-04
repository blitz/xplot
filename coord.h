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

#ifndef COORD_H
#define COORD_H

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
typedef enum {FALSE, TRUE} bool;

struct coord_impl {
  char  *((*unparse)(coord c));
  coord (*parse)(char *s);
  coord (*zero)(void);
  int   (*cmp)(coord c1, coord c2);
  coord (*add)(coord c1, coord c2);
  coord (*subtract)(coord c1, coord c2);
  coord (*round_up)(coord c1, coord c2);
  coord (*round_down)(coord c1, coord c2);
  coord (*tick)(int level);
  int   (*subtick)(int level);
  double (*map)(coord c1, coord c2, int n, coord c);
  coord (*unmap)(coord c1, coord c2, int n, double x);
};

#define cmp_coord(ctype, c1, c2) (impls[(int)ctype]->cmp(c1,c2))

#ifdef TCPTRACE
/* Mark Allman's triangulation distance tool */
#define sub_coord(ctype, c1, c2) (impls[(int)ctype]->subtract(c1,c2))
#endif /* TCPTRACE */

coord_type parse_coord_name(char *s);
char *unparse_coord(coord_type ctype, coord c);
coord parse_coord(coord_type ctype, char *s);
#ifndef cmp_coord
int cmp_coord(coord_type ctype, coord c1, coord c2);
#endif
double map_coord(coord_type ctype, coord first, coord last, int n, coord c);
coord unmap_coord(coord_type ctype, coord first, coord last, int n, double i);
coord bump_coord(coord_type ctype, coord c);
void cticks(coord_type ctype, coord first, coord last, int horizontal,
	    void (*pp)(coord c, int labelflag));

void zoom_in_coord(coord_type ctype, coord first, coord last,
		   int x1, int x2, 
		   int n,
		   coord *newfirst, coord *newlast);

void drag_coord(coord_type ctype, coord first, coord last,
		int x1, int x2,
		int n,
		coord *newfirst, coord *newlast);

#ifdef cmp_coord
extern struct coord_impl *impls[];
#endif

#endif /* COORD_H */
