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

#ifndef _xplot_h_
#define _xplot_h_

#include <sys/time.h>
#include "config.h"

#ifdef HAVE_LIBX11
#include <X11/Xos.h>
#else
#error xplot requires X11
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

#ifdef HAVE_LIBM
#include <math.h>
#ifdef ultrix
double rint();     /* YMUL! some versions of ultrix omit this from math.h! */
#endif
#else
#error xplot requires libm/math.h
#endif

/* Coordinate type definitions */
typedef union coord_u {
  int i;
  unsigned int u;
  struct timeval t;
  double d;
} coord;

typedef enum { U_INT, INT, TIMEVAL, DOUBLE, DTIME} coord_type;

#include "coord.h"

#ifdef ultrix
double rint();     /* YMUL! some versions of ultrix omit this from math.h! */
#endif

/* allow error checking on all malloc() calls */
void *MALLOC(int nbytes);
#define malloc MALLOC

/* prototypes */
void panic(char *s);
void fatalerror(char *s);

#endif
