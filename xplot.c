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
/*
 xplot -- Written by Timothy J Shepard.
          Some features added by Andrew Heybey.
          Some features added by Greg Troxel.

 Some improved color support and a few other tweaks were contributed
 by Shawn D. Ostermann (@cs.ohiou.edu).   
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "xplot.h"
#include "coord.h"

#ifdef HAVE_LIBX11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#else
#error xplot requires x11
#endif


#include <ctype.h>

void panic(char *s)
{
  fprintf(stderr,"fatal error: %s\n",s);
  (void) fflush(stderr);
  abort();
  exit(1);
}


void fatalerror(char *s)
{
  fprintf(stderr,"fatal error: %s\n",s);
  (void) fflush(stderr);
  exit(1);
}

int get_input();
void emit_PS();

#define min(x,y) (((x)<(y))?(x):(y))
#define max(x,y) (((x)>(y))?(x):(y))
#define abs(x)   (((x)>0)?(x):(-(x)))

typedef enum {CENTERED, ABOVE, BELOW, TO_THE_LEFT, TO_THE_RIGHT} position; 

/* dXPoint is just like Xpoint but uses doubles instead instead of shorts.
*/
typedef struct { double x,y; } dXPoint;

/* lXPoint is just like Xpoint but uses longs instead instead of shorts.
   We need the extra bits to do Postscript stuff in emit_PS() below. 
*/
typedef struct { int x,y; } lXPoint;

static dXPoint dXPoint_from_lXPoint(lXPoint lxp)
{
  dXPoint r;
  r.x = (double) lxp.x;
  r.y = (double) lxp.y;
  return r;
}

static lXPoint lXPoint_from_dXPoint(dXPoint dxp)
{
  lXPoint r;
  r.x = (int) rint(dxp.x);
  r.y = (int) rint(dxp.y);
  return r;
}

/* xpcolor_t is a short so it fits in the command struct (60 bytes =
 64 - malloc overhead (on a mips)
*/

#define NCOLORS 10
char *ColorNames[NCOLORS] =
{
"white", "green", "red", "blue", "yellow", "purple", "orange", "magenta", "pink", "gray20"
};
char *GrayPSrep[NCOLORS] =
{
  "0 setgray",			/* white */
  ".3 setgray",			/* green */
  ".5 setgray",			/* red */
  ".7 setgray",			/* blue */
  ".9 setgray",			/* yellow infreq */
  ".6 setgray",			/* purple infreq */
  ".8 setgray",
  ".4 setgray",
  ".95 setgray",
  ".2 setgray"			/* 20% gray */
};
char *ColorPSrep[NCOLORS] =
{
"0 setgray", "0 1 0 setrgbcolor", "1 0 0 setrgbcolor", "0 0 1 setrgbcolor", "1 1 0 setrgbcolor", "0 1 1 setrgbcolor", "1 .5 0 setrgbcolor", "0 .5 1 setrgbcolor", "1 .5 .5 setrgbcolor", "0.2 0.2 0.2 setrgbcolor"
};

int	NColors = NCOLORS;

typedef short xpcolor_t;

typedef struct command_struct {
  struct command_struct *next;
  enum plot_command_type { X, DOT, PLUS, BOX, DIAMOND,
			   UTICK, DTICK, LTICK, RTICK, HTICK, VTICK,
			   UARROW, DARROW, LARROW, RARROW,
			   INVISIBLE, LINE, DLINE,
			   TEXT, TITLE, XLABEL, YLABEL } type:5;
  position position:3;
  bool needs_redraw:1;
  bool mapped:1;
  bool decoration:1;
  xpcolor_t color;
#ifdef WINDOW_COORDS_IN_COMMAND_STRUCT
  dXPoint a,b;
#endif
  coord xa, ya;
  coord xb, yb;
  char *text;
} command;

#define NUMVIEWS 30

#define pl_x_left   pl->x_left[pl->viewno]
#define pl_x_right  pl->x_right[pl->viewno]
#define pl_y_top    pl->y_top[pl->viewno]
#define pl_y_bottom pl->y_bottom[pl->viewno]

#ifndef WINDOW_COORDS_IN_COMMAND_STRUCT
#define pspl_x_left   pspl.x_left[pspl.viewno]
#define pspl_x_right  pspl.x_right[pspl.viewno]
#define pspl_y_top    pspl.y_top[pspl.viewno]
#define pspl_y_bottom pspl.y_bottom[pspl.viewno]
#endif

typedef struct plotter {
  struct plotter *next;
  command *commands;
  coord_type x_type;
  coord_type y_type;
  char *x_units;
  char *y_units;
  double aspect_ratio; /* 0.0 unless specified */
  int viewno;
  coord x_left[NUMVIEWS];
  coord y_bottom[NUMVIEWS];
  coord x_right[NUMVIEWS];
  coord y_top[NUMVIEWS];
  dXPoint origin;
  dXPoint size;
  dXPoint mainsize;
  Display *dpy;
  Screen *screen;
  int numtiles;
  int tileno;
  Window win;
  XSizeHints xsh;
  int visibility;
  int size_changed;
  int new_expose;
  int clean;
  GC gcs[NCOLORS];
  GC decgc;
  GC xorgc;
  GC bacgc;
  XFontStruct *font_struct;
  XGCValues gcv;
  enum plstate {NORMAL, SLAVE,
		  ZOOM, HZOOM, VZOOM,
		  DRAG, HDRAG, VDRAG,
		  EXITING, PRINTING, FIGING, THINFIGING,
		  ADVANCING, BACKINGUP,
		  WEDGED} state;
  struct plotter *master; /* pointer to master when in SLAVE state */
  enum plstate master_state; /* state of master when in SLAVE state */
  int buttonsdown;
  lXPoint raw_dragstart;
  lXPoint dragstart;
  lXPoint dragend;
  lXPoint pointer;
  lXPoint pointer_marks;
  bool pointer_marks_on_screen;
  Atom xplot_nagle_atom;
  bool slave_draw_in_progress;
  bool slave_motion_pending;
  lXPoint master_pointer;
  xpcolor_t default_color;
  xpcolor_t current_color;
  XColor foreground_color;
  XColor background_color;
  bool thick; 
} *PLOTTER;

PLOTTER the_plotter_list;

int option_thick;
int option_mono;
int global_argc;
char **global_argv;

#ifdef TCPTRACE
int show_dist = 0;
#endif /* TCPTRACE */


command *new_command(struct plotter *pl)
{
  command *c;
  
  c = (command *) malloc(sizeof(command));
  if (c == 0) fatalerror("malloc returned null");
  c->decoration = FALSE;
#ifdef WINDOW_COORDS_IN_COMMAND_STRUCT
  c->a.x = 0;
  c->a.y = 0;
  c->b.x = 0;
  c->b.y = 0;
#endif
  c->color = pl->current_color;
  
  c->xa.d = 0.0;
  c->ya.d = 0.0;
  c->xb.d = 0.0;
  c->yb.d = 0.0;

  c->next = pl->commands;
  pl->commands = c;

  return c;
}

void free_command(command *c)
{
  switch(c->type) {
  case TEXT:
    free(c->text);
    break;
  default:
    break;
  }
  free((char *)c);
}

dXPoint tomain(struct plotter *pl, dXPoint xp)
{
  dXPoint r;
  r.x = xp.x + pl->origin.x;
  r.y = xp.y + pl->origin.y;
  return r;
}

dXPoint tosub(struct plotter *pl, dXPoint xp)
{
  dXPoint r;
  r.x = xp.x - pl->origin.x;
  r.y = xp.y - pl->origin.y;
  return r;
}



/*
  0 1 2
  3 4 5
  6 7 8
*/

enum in_p { NO, YES, MAYBE } in_rect_table[9][9] = {
  { NO,    NO,    NO,    NO,    YES,   MAYBE, NO,    MAYBE, MAYBE },
  { NO,    NO,    NO,    MAYBE, YES,   MAYBE, MAYBE, YES,   MAYBE },
  { NO,    NO,    NO,    MAYBE, YES,   NO,    MAYBE, MAYBE, NO    },
  { NO,    MAYBE, MAYBE, NO,    YES,   YES,   NO,    MAYBE, MAYBE },
  { YES,   YES,   YES,   YES,   YES,   YES,   YES,   YES,   YES   },
  { MAYBE, MAYBE, NO,    YES,   YES,   NO,    MAYBE, MAYBE, NO    },
  { NO,    MAYBE, MAYBE, NO,    YES,   MAYBE, NO,    NO,    NO    },
  { MAYBE, YES,   MAYBE, MAYBE, YES,   MAYBE, NO,    NO,    NO    },
  { MAYBE, MAYBE, NO,    MAYBE, YES,   NO,    NO,    NO,    NO    }
};

static void compute_window_coords(struct plotter *pl, command *com)
{
  int loc1;
  int loc2;

  if (com->type == TITLE
      || com->type == XLABEL
      || com->type == YLABEL
      ) {
    /* complete special case */
    com->mapped = TRUE;
    com->needs_redraw = TRUE;
    return;
  }

  if (com->type == INVISIBLE) {
    com->mapped = FALSE;
    com->needs_redraw = FALSE;
    return;
  }


#define ccmp(ctype, c1, c2, op) (cmp_coord(ctype, c1, c2) op 0)
#define xcmp(xa, xb, op) ccmp(pl->x_type, xa, xb, op)
#define ycmp(ya, yb, op) ccmp(pl->y_type, ya, yb, op)

  if (xcmp(com->xa, pl_x_left, <))
    if (ycmp(com->ya, pl_y_top, >)) loc1 = 0;
    else
      if (ycmp(com->ya, pl_y_bottom, <)) loc1 = 6;
      else loc1 = 3;
  else
    if (xcmp(com->xa, pl_x_right, >))
      if (ycmp(com->ya, pl_y_top, >)) loc1 = 2;
      else
	if (ycmp(com->ya, pl_y_bottom, <)) loc1 = 8;
	else loc1 = 5;
    else
      if (ycmp(com->ya, pl_y_top, >)) loc1 = 1;
      else
	if (ycmp(com->ya, pl_y_bottom, <)) loc1 = 7;
	else loc1 = 4;

  switch(com->type) {
  default:
    panic("compute_window_coords: unknown command type");
  case X:
  case DOT:
  case PLUS:
  case BOX:
  case DIAMOND:
  case UTICK:
  case DTICK:
  case LTICK:
  case RTICK:
  case HTICK:
  case VTICK:
  case UARROW:
  case DARROW:
  case LARROW:
  case RARROW:
  case TEXT:
    loc2 = loc1;
    break;
  case DLINE:
  case LINE:
    if (xcmp(com->xb, pl_x_left, <))
      if (ycmp(com->yb, pl_y_top, >)) loc2 = 0;
      else
	if (ycmp(com->yb, pl_y_bottom, <)) loc2 = 6;
	else loc2 = 3;
    else
      if (xcmp(com->xb, pl_x_right, >))
	if (ycmp(com->yb, pl_y_top, >)) loc2 = 2;
	else
	  if (ycmp(com->yb, pl_y_bottom, <)) loc2 = 8;
	  else loc2 = 5;
      else
	if (ycmp(com->yb, pl_y_top, >)) loc2 = 1;
	else
	  if (ycmp(com->yb, pl_y_bottom, <)) loc2 = 7;
	  else loc2 = 4;
    break;
  }

  com->mapped = FALSE;
  switch(in_rect_table[loc1][loc2]) {
  case NO:
    return;
  case MAYBE:
  case YES:
    break;
  default:
    panic("compute_window_coords: unknown value from table");
  }
  
#ifdef WINDOW_COORDS_IN_COMMAND_STRUCT
  com->a.x = map_coord(pl->x_type, pl_x_left, pl_x_right, pl->size.x,
		       com->xa);
  com->a.y = (pl->size.y - 1) -
    map_coord(pl->y_type, pl_y_bottom, pl_y_top, pl->size.y,
	      com->ya);

  switch(com->type) {
  case X:
  case DOT:
  case PLUS:
  case BOX:
  case DIAMOND:
  case UTICK:
  case DTICK:
  case LTICK:
  case RTICK:
  case HTICK:
  case VTICK:
  case RARROW:
  case UARROW:
  case DARROW:
  case LARROW:
  case TEXT:
    break;
  case DLINE:
  case LINE:
    com->b.x = map_coord(pl->x_type, pl_x_left, pl_x_right, pl->size.x,
			 com->xb);
    com->b.y = (pl->size.y - 1) -
      map_coord(pl->y_type, pl_y_bottom, pl_y_top, pl->size.y,
		com->yb);
    break;
  default:
    panic("compute_window_coords: unknown command type");
  }

  com->a = tomain(pl,com->a);
  com->b = tomain(pl,com->b);
#endif
  com->mapped = TRUE;
  com->needs_redraw = TRUE;

}


static struct plotter *the_plotter_we_are_working_on;    /* C really looses */

char *append_strings_with_space_freeing_first(char *s1, char *s2)
{
  int len2,len;
  char *r;

  len2 = strlen(s2);
  if (len2 == 0)
    return s1;
  len = strlen(s1) + 1 + len2 + 1;
  r = (char *) malloc(len);
  if (r == 0) fatalerror("malloc returned null");
  strcpy(r, s1);
  strcat(r, " ");
  strcat(r, s2);
  free(s1);
  return r;
}

void doxtick(coord c,int labelflag)
{
  struct plotter *pl = the_plotter_we_are_working_on;
  command *com = new_command(pl);
  com->decoration = TRUE;
  com->type = DTICK;
  com->xa = c;
  com->ya = pl_y_bottom;
  if (labelflag) {
    com = new_command(pl);
    com->decoration = TRUE;
    com->type = TEXT;
    com->position = BELOW;
    com->xa = c;
    com->ya = pl_y_bottom;
    com->text = unparse_coord(pl->x_type, c);
    com->text = append_strings_with_space_freeing_first(com->text,
							pl->x_units);
  }
}
void doytick(coord c,int labelflag)
{
  struct plotter *pl = the_plotter_we_are_working_on;
  command *com = new_command(pl);
  com->decoration = TRUE;
  com->type = LTICK;
  com->xa = pl_x_left;
  com->ya = c;
  if (labelflag) {
    com = new_command(pl);
    com->decoration = TRUE;
    com->type = TEXT;
    com->position = TO_THE_LEFT;
    com->xa = pl_x_left;
    com->ya = c;
    com->text = unparse_coord(pl->y_type, c);
    com->text = append_strings_with_space_freeing_first(com->text,
							pl->y_units);
  }
}
void axis(struct plotter *pl)
{
  command *com;

  com = new_command(pl);
  com->decoration = TRUE;
  com->type = LINE;
  com->xa = pl_x_left;
  com->ya = pl_y_top;
  com->xb = pl_x_left;
  com->yb = pl_y_bottom;

  com = new_command(pl);
  com->decoration = TRUE;
  com->type = LINE;
  com->xa = pl_x_left;
  com->ya = pl_y_bottom;
  com->xb = pl_x_right;
  com->yb = pl_y_bottom;

  the_plotter_we_are_working_on = pl;
  cticks(pl->x_type, pl_x_left, pl_x_right, 1, doxtick);
  cticks(pl->y_type, pl_y_bottom, pl_y_top, 0, doytick);

}

void size_window(struct plotter *pl)
{
  command *c;

  pl->origin.x = 70;
  pl->origin.y = 30;
  pl->size.x = pl->mainsize.x - pl->origin.x - 10;
  pl->size.y = pl->mainsize.y - pl->origin.y - 30;
  
  /*************** CAVEAT abstraction violation in emit_PS() code below,
    caused mostly by hardwired constants above. */
  while (pl->commands && pl->commands->decoration) {
    c = pl->commands;
    pl->commands = pl->commands->next;
    free_command(c);
  }

  axis(pl);
  
  for (c = pl->commands; c != NULL; c = c->next)
    compute_window_coords(pl, c);
  
  
}

lXPoint detent(struct plotter *pl, lXPoint xp)
{

  dXPoint dr;
  dXPoint dxp;
  
  dxp = dXPoint_from_lXPoint(xp);

  dxp = tosub(pl, dxp);

  dr.x = map_coord(pl->x_type, pl_x_left, pl_x_right, pl->size.x,
		   unmap_coord(pl->x_type, pl_x_left, pl_x_right,
			       pl->size.x, dxp.x));
  dr.y = map_coord(pl->y_type, pl_y_bottom, pl_y_top, pl->size.y,
		   unmap_coord(pl->y_type, pl_y_bottom, pl_y_top,
			       pl->size.y, dxp.y));
  dr = tomain(pl,dr);

  return lXPoint_from_dXPoint(dr);
}  

lXPoint map_pl_pl(struct plotter *pl_from, struct plotter *pl_to, lXPoint xp)
{

  dXPoint dr;
  dXPoint dxp;
  coord x;
  coord y;

#define pl pl_from
  dxp = dXPoint_from_lXPoint(xp);

  dxp = tosub(pl, dxp);

  x = unmap_coord(pl->x_type, pl_x_left, pl_x_right, pl->size.x, dxp.x);
  y = unmap_coord(pl->y_type, pl_y_bottom, pl_y_top, pl->size.y, dxp.y);
    
#undef pl
#define pl pl_to

  dr.x = map_coord(pl->x_type, pl_x_left, pl_x_right, pl->size.x, x);

  dr.y = map_coord(pl->y_type, pl_y_bottom, pl_y_top, pl->size.y, y);

  dr = tomain(pl,dr);
#undef pl

  return lXPoint_from_dXPoint(dr);
}

#ifndef ScreenNumberOfScreen
/* X.V11R3 is/was missing this functionality.
   X Consortium people promissed that this will be 
   done right and defined as a macro eventually.
   
   For now, kludge in a hopefully forward compatible way.
   */

#define ScreenNumberOfScreen X_ScreenNumberOfScreen
int X_ScreenNumberOfScreen(Screen *scr)
{
  Display *dpy;
  int i;
  Screen *iscr;

  dpy = DisplayOfScreen(scr);

  for (i=0;i<XScreenCount(dpy);i++) {
    iscr = ScreenOfDisplay(dpy,i);
    if (iscr == scr)
      return i;
  }
  /* oops */
  panic("XScreenNumberOfScreen: could not find screen number");
  return 0;
}
#endif


/*
 * numwins, nth: specifies how many windows and which one (zero-based)
 * are desired.  This is a hint to new_plotter, which could check resources.
 * If tiling, we stack multiple windows vertically and make them smaller.
 */

void new_plotter(FILE *fp, Display *dpy, int numtiles, int tileno, int lineno)
{
  int r = 0;
  PLOTTER pl;

  do {
  
    pl = (PLOTTER) malloc(sizeof(*pl));
    if (pl == 0) fatalerror("malloc returned null");
    pl->next = the_plotter_list;
    the_plotter_list = pl;
  
    pl->dpy = dpy;
    pl->screen = XDefaultScreenOfDisplay(pl->dpy);

    pl->numtiles = numtiles;
    pl->tileno = tileno;

    pl->win = 0;

    pl->aspect_ratio = 0.0;
    pl->viewno = 0;
    pl->commands = NULL;
    pl->x_type = INT;
    pl->y_type = INT;
    pl->x_units = "";
    pl->y_units = "";
    pl->mainsize.x = 0;
    pl->mainsize.y = 0;
    pl->size_changed = 0;
    pl->size.x = 0;
    pl->size.y = 0;
    pl->origin.x = 0;
    pl->origin.y = 0;
    pl->state = NORMAL;
    pl->raw_dragstart.x = 0;
    pl->raw_dragstart.y = 0;
    pl->dragstart.x = 0;
    pl->dragstart.y = 0;
    pl->dragend.x = 0;
    pl->dragend.y = 0;
    pl->pointer.x = 0;
    pl->pointer.y = 0;
    pl->pointer_marks.x = 0;
    pl->pointer_marks.y = 0;
    pl->pointer_marks_on_screen = FALSE;
    pl->xplot_nagle_atom = None;
    pl->slave_draw_in_progress = FALSE;
    pl->slave_motion_pending = FALSE;
    pl->master_pointer.x = 0;
    pl->master_pointer.y = 0;
    pl->buttonsdown = 0;
    pl->new_expose = 0;
    pl->clean = 0;
    pl->default_color = -1;
    pl->current_color = -1;
    pl->thick = option_thick? TRUE: FALSE; 

    r = get_input(fp, dpy, lineno, pl);
    lineno = r;
  } while (r > 0);
  
}

void display_plotter(PLOTTER pl)
{
  XSetWindowAttributes attr;
  Window rootwindow;

  if (pl->win != 0) {
    fprintf(stderr,
	    "display_plotter called for already-displayed plotter\n");
    return;
  }

  rootwindow = XRootWindowOfScreen(pl->screen);

  /* set up foreground/background colors */

  {
    Colormap default_cmap = DefaultColormap(pl->dpy, DefaultScreen(pl->dpy));
    char *foreground_color_name;
    char *background_color_name;
    XColor exact_return;
    int i;

    foreground_color_name = XGetDefault(pl->dpy, global_argv[0], "foreground");
    if (!foreground_color_name) foreground_color_name = "white";
    i = XAllocNamedColor(pl->dpy, default_cmap, foreground_color_name,
			 &exact_return, &pl->foreground_color);
    if (i < 0)
    {
      fprintf(stderr, "XAllocNamedColor failed for %s: %d\n",
	      foreground_color_name, i);
      return;
    }

#if 0
    ColorNames[0] = strdup(foreground_color_name);
#else
    ColorNames[0] = (char *) malloc(strlen(foreground_color_name) + 1);
    if (ColorNames[0] == NULL) fatalerror("malloc returned null");
    strcpy(ColorNames[0], foreground_color_name);
#endif

    background_color_name = XGetDefault(pl->dpy, global_argv[0], "background");
    if (!background_color_name) background_color_name = "black";
    i = XAllocNamedColor(pl->dpy, default_cmap, background_color_name,
			 &exact_return, &pl->background_color);
    if (i < 0)
    {
      fprintf(stderr, "XAllocNamedColor failed for %s: %d\n",
	      background_color_name, i);
      return;
    }
  }      
        
  attr.background_pixel = pl->background_color.pixel;
  attr.border_pixel     = pl->foreground_color.pixel;
  attr.event_mask       = ButtonReleaseMask|ButtonPressMask|ExposureMask|
    EnterWindowMask|LeaveWindowMask|PointerMotionMask|PointerMotionHintMask|
    StructureNotifyMask|VisibilityChangeMask;
#if 1
  attr.cursor           = XCreateFontCursor(pl->dpy, XC_crosshair);
#else
  attr.cursor           = XCreateFontCursor(pl->dpy, XC_tcross);
#endif
  
  {
    XColor blk,wht;
    
    blk.red = blk.green = blk.blue = 0;
    wht.red = wht.green = wht.blue = -1;
    wht.flags = blk.flags = DoRed|DoGreen|DoBlue;
    
    XRecolorCursor(pl->dpy, attr.cursor, &wht, &blk);
  }
  
  {
    char *geom = XGetDefault (pl->dpy, global_argv[0], "geometry");
    int x = 10, y = 70, borderw = 1;
    unsigned int width = 400, height = 400;
    int flags = 0;
    int i;

    for (i = 1 ; i < global_argc ; i++) {
      if (strcmp("-geometry", global_argv[i]) == 0) {
	if (i+1 < global_argc) {
	  geom = global_argv[i+1];
	  break;
	} else {
	  static int virgin = 1;
	  if (virgin) {
	    fprintf(stderr, "-geometry on command line without any following argument\n");
	    virgin = 0;
	  }
	}
      }
    }
    if (geom)
     {
	flags = XParseGeometry (geom, &x, &y, &width, &height);
	if (flags & XValue)
	  {
	    if (flags & XNegative)
	      x += DisplayWidth (pl->dpy, ScreenNumberOfScreen(pl->screen))
		- width - borderw*2;
	    else
	      x += borderw;
	  }
	if (flags & YValue)
	  {
	    if (flags & YNegative)
	      y += DisplayHeight (pl->dpy, ScreenNumberOfScreen(pl->screen))
		- height - borderw*2;
	    else
	      y += borderw;
	  }
      }
    
    if (pl->numtiles > 0) {
      /* Now we have geometry in x, y, height, width for total xplot area */
      y += (height * pl->tileno) / pl->numtiles;
      height = height / pl->numtiles;
      /* we have now selected the nth 1/numtiles chunk */
    }

    pl->win = XCreateWindow(pl->dpy, rootwindow, x, y, width, height, borderw,
			    DefaultDepth(pl->dpy, ScreenNumberOfScreen(pl->screen)),
			    CopyFromParent, CopyFromParent,
			    CWBackPixel|CWBorderPixel|CWEventMask|CWCursor,
			    &attr);
    
    pl->xsh.flags = 0;
    if (flags & (WidthValue | HeightValue))
      pl->xsh.flags |= USSize;
    else
      pl->xsh.flags |= PSize;
    if (flags & (XValue | YValue))
      pl->xsh.flags |= USPosition;
    else
      pl->xsh.flags |= PPosition;
    
    pl->xsh.width = width;
    pl->xsh.height = height;
    pl->xsh.x = x;
    pl->xsh.y = y;
    XSetStandardProperties(pl->dpy, pl->win, "xplot", "xplot", None,
			   global_argv, global_argc, &pl->xsh);
  }
  
  XMapRaised(pl->dpy, pl->win);
  
  pl->visibility = VisibilityFullyObscured; /* initially true */
  {
    char *use_font = XGetDefault (pl->dpy, global_argv[0], "font");
    pl->font_struct = XLoadQueryFont(pl->dpy, use_font ? use_font : "fixed");
  }
  
  pl->gcv.foreground = pl->foreground_color.pixel;
  pl->gcv.background = pl->background_color.pixel;
  pl->gcv.font = pl->font_struct->fid;
  pl->gcv.line_width = 0;
  pl->gcv.cap_style = CapProjecting;
  if (pl->thick) {
    pl->gcv.line_width = 3;
    pl->gcv.cap_style = CapRound;
  }
  
  {
#define N_DPY_S 2

    static struct dpy_info {
      unsigned long line_plane_mask;
      Colormap clr_map;
      int depth;
      XColor clr;
      unsigned long pixel[NCOLORS];
      int Colors[NCOLORS];
      int virgin;
      int warned_color_alloc_failed;
      Atom xplot_nagle_atom;
      Display *saved_dpy;
    } d_i[N_DPY_S];

    int i;
    int d;
    
    for (d = 0; d < N_DPY_S; d++) {
      if (pl->dpy == d_i[d].saved_dpy)
	break;
      if (d_i[d].saved_dpy == 0) {
	d_i[d].saved_dpy = pl->dpy;
	d_i[d].virgin = 1;
	break;
      }
    }
    if (d >= N_DPY_S) {
      fprintf(stderr, "%s:%d:%s: bug -- pl->dpy != saved_dpy\n",
	      __FILE__, __LINE__, __FUNCTION__);
      panic("bug");
    }

    /* Allocate some color cells */
      
    if (d_i[d].virgin ) {
      int ci;

      d_i[d].virgin = 0;
      
      d_i[d].xplot_nagle_atom = XInternAtom(pl->dpy, "XPLOT_NAGLE", False);

      d_i[d].clr_map = DefaultColormap(pl->dpy, DefaultScreen(pl->dpy));
      
      d_i[d].depth = DisplayPlanes(pl->dpy, DefaultScreen(pl->dpy));

      ci = 0;

      /* if display/screen has only 1 bit of depth, don't try to
       * allocate any colors - just use BlackPixel and WhitePixel
       */
      if ( ! option_mono && d_i[d].depth > 1
	   && XAllocColorCells(pl->dpy, 
			       d_i[d].clr_map, 0,
			       &d_i[d].line_plane_mask, 1,
			       d_i[d].pixel, NColors)
	   )
	{
	  for ( ; ci < NColors; ci++)  {
	    XParseColor(pl->dpy, d_i[d].clr_map, ColorNames[ci], &d_i[d].clr);
	    d_i[d].clr.pixel = d_i[d].pixel[ci];
	    XStoreColor (pl->dpy, d_i[d].clr_map, &d_i[d].clr);
	    d_i[d].clr.pixel |= d_i[d].line_plane_mask;
	    XStoreColor (pl->dpy, d_i[d].clr_map, &d_i[d].clr);
	    d_i[d].Colors[ci] = d_i[d].clr.pixel;
	  }

	} else if (! option_mono && d_i[d].depth > 1 ) {
	  /* some visual types (e.g. TrueColor) do not support XAllocColorCells */

	  for ( ; ci < NColors; ci++) {
	    XColor exact_return;

	    i = XAllocNamedColor(pl->dpy, d_i[d].clr_map, ColorNames[ci],
				 &exact_return, &d_i[d].clr);
	    if ( i < 0 )
	      {
		fprintf(stderr, "XAllocNamedColor failed for %s: %d\n",
			ColorNames[i], i);
		break;
	      }

	    /* Here, we should check if the color is close enough. */
	    d_i[d].Colors[ci] = d_i[d].clr.pixel;

#if 0
	    /***** and on any failure, you should break out of this
	      loop leaving ci pointing at the first color (perhaps
	      zero) of the first color you've failed to allocate. */

	    /***** need to take care that this does the correct thing
	      on one-bit-plane displays.    There are two ways that this might occur:
	    
	      1. All pixel values obtained in this loop on a
	      one-bit-plane display are the same as WhitePixelOfScreen()

	      or
	      
	      2. On a one-bitplane display we break out of this loop
	      and leave the loop below to fill in WhitePixelOfScreen()
	      for each plot color.
	      */
#endif
	  }
	}
      for ( ; ci < NColors; ci++) {

	/* probably only one bit plane, or all the color cells are taken
	   (or option_mono)*/

	if (!d_i[d].warned_color_alloc_failed && !option_mono) {
	  fputs("unable to get all desired colors, will substitute white for some or all colors\n",
		stderr);
	  d_i[d].warned_color_alloc_failed = 1;
	}
	d_i[d].Colors[ci] = WhitePixelOfScreen(pl->screen);
      }
    }
    
    for (i = 0; i < NColors; i++) {

      pl->gcs[i] = XCreateGC(pl->dpy, pl->win,
			     GCForeground|GCFont|GCLineWidth|GCCapStyle,
			     &(pl->gcv));
    
      XSetForeground(pl->dpy, pl->gcs[i], d_i[d].Colors[i]);

    }

    pl->xplot_nagle_atom = d_i[d].xplot_nagle_atom;
#if 0
    printf("DEBUG: nagle_atom is %ld\n", pl->xplot_nagle_atom);
#endif
  }


  pl->decgc = XCreateGC(pl->dpy, pl->win,
			GCForeground|GCFont|GCLineWidth|GCCapStyle,
			&(pl->gcv));
  
  pl->gcv.foreground ^= pl->gcv.background;
  pl->gcv.function = GXxor;
  
  pl->xorgc = XCreateGC(pl->dpy, pl->win,
			GCForeground|GCFunction|GCFont|GCLineWidth|GCCapStyle,
			&(pl->gcv));
  
  pl->gcv.foreground = pl->gcv.background;

  /* just like dec gc but draws in background color */
  pl->bacgc = XCreateGC(pl->dpy, pl->win,
			GCForeground|GCFont|GCLineWidth|GCCapStyle,
			&(pl->gcv));

}


int undisplay_plotter(PLOTTER pl, int direction)
{
  int i;
  PLOTTER pll;
  
  if (direction == 1) {
    pll = pl->next;
    if (pll == 0) {
      pll = the_plotter_list;
    }
  } else if (direction == -1) {
    pll = the_plotter_list; 
    if (pl != the_plotter_list) {
      while (pll) {
	if (pll->next == pl) {
	  break;
	}
	pll = pll->next;
      }
    } else {
      /* go to last plotter on list */
      while (pll->next)
	pll = pll->next;
    }
  } else {
    pll = 0;
  }

  /* if previous (next) plotter on list doesn't have a window yet
     give it our window, GCs, and font_struct and set up all the
     other stuff that display_plotter() would set up . */ 

  if (pll && pll->win == 0) {
    pll->win = pl->win;
    for (i = 0; i < NColors; i++)
      pll->gcs[i] = pl->gcs[i];
    pll->decgc = pl->decgc;
    pll->xorgc = pl->xorgc;
    pll->font_struct = pl->font_struct;

    pll->xplot_nagle_atom = pl->xplot_nagle_atom;

    pll->xsh = pl->xsh;  /* unnecessary? */

    pll->visibility = pl->visibility;

    pll->gcv = pl->gcv; /* unnecessary? */

    pll->mainsize = pl->mainsize;

    pll->size_changed = 1; /* will trigger clearing and redisplay */

    /* take window away from this plotter */
    pl->win = 0;
    
    return 1;
  } else {
    XDestroyWindow(pl->dpy, pl->win);
    for (i = 0; i < NColors; i++)
      XFreeGC(pl->dpy, pl->gcs[i]);
    XFreeGC(pl->dpy, pl->decgc);
    XFreeGC(pl->dpy, pl->xorgc);
    XFreeFont(pl->dpy, pl->font_struct);
    pl->win = 0;
    return 0;
  }
    
#if 0
  while (pl->commands) {
    c = pl->commands;
    pl->commands = pl->commands->next;
    free_command(c);
  }
  
  free(pl);
#endif
  
}

/*
 * compute the bounding box of the current view
 */
void shrink_to_bbox(struct plotter *pl, int x, int y)
{
  command *c;
  
  int nmapped = 0;
  int ndots = 0;
  int virgin = 1;
  
  coord saved_x_left = pl_x_left;
  coord saved_x_right = pl_x_right;
  coord saved_y_bottom = pl_y_bottom;
  coord saved_y_top = pl_y_top;

  /* get mapped indicator set correctly for what new window size
   * before we scale  the other axis */
  for (c = pl->commands; c != NULL; c = c->next)
    compute_window_coords(pl, c);

#ifdef LOTS_OF_DEBUGGING_PRINTS
  fprintf(stderr, "C_S_P: view %d OLD %s %s %s %s\n",
	  pl->viewno,
	  unparse_coord(pl->x_type, pl_x_left),
	  unparse_coord(pl->x_type, pl_x_right),
	  unparse_coord(pl->y_type, pl_y_bottom),
	  unparse_coord(pl->y_type, pl_y_top));
#endif

  for (c = pl->commands; c != NULL; c = c->next)
    if ( c->mapped && ! c->decoration)
      {
	nmapped++;

	switch(c->type) {
	case LINE:
	case DLINE:
	  if ( x )
	    {
	      if (virgin || ccmp(pl->x_type, c->xb, pl_x_left, <))
		pl_x_left = c->xb;
	      if (virgin || ccmp(pl->x_type, c->xb, pl_x_right, >))
		pl_x_right = c->xb;
	    }
	  if ( y )
	    {
	      if (virgin || ccmp(pl->y_type, c->yb, pl_y_bottom, <))
		pl_y_bottom = c->yb;
	      if (virgin || ccmp(pl->y_type, c->yb, pl_y_top, >))
		pl_y_top = c->yb;
	    }
	  virgin = 0;
	  
	case X:
	case DOT:
	case PLUS:
	case BOX:
	case DIAMOND:
	case UTICK:
	case DTICK:
	case LTICK:
	case RTICK:
	case HTICK:
	case VTICK:
	case UARROW:
	case DARROW:
	case LARROW:
	case RARROW:
	case INVISIBLE:
	case TEXT:
	  ndots++;
	  if ( x )
	    {
	      if (virgin || ccmp(pl->x_type, c->xa, pl_x_left, <))
		pl_x_left = c->xa;
	      if (virgin || ccmp(pl->x_type, c->xa, pl_x_right, >))
		pl_x_right = c->xa;
	    }
	  if ( y )
	    {
	      if (virgin || ccmp(pl->y_type, c->ya, pl_y_bottom, <))
		pl_y_bottom = c->ya;
	      if (virgin || ccmp(pl->y_type, c->ya, pl_y_top, >))
		pl_y_top = c->ya;
	    }
	  virgin = 0;
	case TITLE:
	case XLABEL:
	case YLABEL:
	  break;
	}
	
      }

  /* make sure top/bottom are not equal, somehow  */
  if ( ccmp(pl->x_type, pl_x_left, pl_x_right, ==) )
    {
      pl_x_left = saved_x_left;
      pl_x_right = saved_x_right;
    }
  if ( ccmp(pl->y_type, pl_y_bottom, pl_y_top, ==) )
    {
      pl_y_bottom = saved_y_bottom;
      pl_y_top = saved_y_top;
    }

#if 0  /* this code is broken, round_{down,up} take more than one argument */
  /* make points 5/95 % rather than extreme - settle for rounding */
  if ( y )
    {
      pl_y_bottom = (impls[(int)pl->y_type]->round_down)(pl_y_bottom);
      pl_y_top = (impls[(int)pl->y_type]->round_up)(pl_y_top);
    }
#endif


#ifdef LOTS_OF_DEBUGGING_PRINTS
  fprintf(stderr, "C_S_P: nmapped=%d ndots=%d NEW %s %s %s %s\n",
	  nmapped, ndots,
	  unparse_coord(pl->x_type, pl_x_left),
	  unparse_coord(pl->x_type, pl_x_right),
	  unparse_coord(pl->y_type, pl_y_top),
	  unparse_coord(pl->y_type, pl_y_top));
#endif
}

void draw_pointer_marks(PLOTTER pl, GC gc)
{
  const int x_pm_l = 6;

#if 0
  printf("draw_pointer_marks, pl->state is %d\n", pl->state);
#endif

  if (pl->win == 0) return;

  XDrawLine(pl->dpy, pl->win, gc,
	    pl->pointer_marks.x, pl->origin.y + pl->size.y - 1,
	    pl->pointer_marks.x, pl->origin.y + pl->size.y - 1 - x_pm_l);
  XDrawLine(pl->dpy, pl->win, gc,
	    pl->origin.x, pl->pointer_marks.y,
	    pl->origin.x + x_pm_l, pl->pointer_marks.y);

  XDrawLine(pl->dpy, pl->win, gc,
	    pl->pointer_marks.x - x_pm_l, pl->pointer_marks.y,
	    pl->pointer_marks.x + x_pm_l, pl->pointer_marks.y);
  XDrawLine(pl->dpy, pl->win, gc,
	    pl->pointer_marks.x, pl->pointer_marks.y - x_pm_l,
	    pl->pointer_marks.x, pl->pointer_marks.y + x_pm_l);

  switch (pl->state == SLAVE? pl->master_state : pl->state) {
  case ZOOM:
  case HZOOM:
  case VZOOM:
    XDrawRectangle(pl->dpy, pl->win, gc,
		   min(pl->dragstart.x, pl->dragend.x), 
		   min(pl->dragstart.y, pl->dragend.y),
		   abs(pl->dragend.x - pl->dragstart.x),
		   abs(pl->dragend.y - pl->dragstart.y));
    break;
  case HDRAG:
    XDrawLine(pl->dpy, pl->win, gc,
	      pl->dragstart.x, pl->dragstart.y,
	      pl->dragend.x, pl->dragstart.y);
    break;
  case VDRAG:
    XDrawLine(pl->dpy, pl->win, gc,
	      pl->dragstart.x, pl->dragstart.y,
	      pl->dragstart.x, pl->dragend.y);
    break;
  case DRAG:
    XDrawLine(pl->dpy, pl->win, gc,
	      pl->dragstart.x, pl->dragstart.y, pl->dragend.x, pl->dragend.y);
#ifdef TCPTRACE
    if (show_dist)
    {
	lXPoint p;
	float pwidth = pl->size.x, pheight = pl->size.y;
	long pxdist, pydist;
	coord axdist, aydist;
	float xscale, yscale;
	char tmp [100], *s;
	double xdist, ydist, slope, hyp_dist;

	XDrawLine(pl->dpy, pl->win, gc,
		  pl->dragstart.x, pl->dragstart.y, pl->dragstart.x, 
		  pl->dragend.y);
	XDrawLine(pl->dpy, pl->win, gc,
		  pl->dragstart.x, pl->dragend.y, pl->dragend.x, 
		  pl->dragend.y);

	pxdist = pl->dragend.x - pl->dragstart.x;
	pydist = pl->dragstart.y - pl->dragend.y;
	xscale = pxdist / pwidth;
	yscale = pydist / pheight;

	axdist = sub_coord (pl->x_type,pl_x_right,pl_x_left);
	aydist = sub_coord (pl->y_type,pl_y_top,pl_y_bottom);

	if (pl->x_type != TIMEVAL)
	{
	    s = unparse_coord (pl->x_type,axdist);
	    xdist = atof (s);
	    free(s);
	}
	else
	    xdist = axdist.t.tv_sec + axdist.t.tv_usec / 1000000.0;
	xdist *= xscale;

	if (pl->y_type != TIMEVAL)
	{
	    s = unparse_coord (pl->y_type,aydist);
	    ydist = atof (s);
	    free(s);
	}
	else
	    ydist = aydist.t.tv_sec + aydist.t.tv_usec / 1000000.0;
	ydist *= yscale;

	slope = ydist / xdist;
	hyp_dist = sqrt ((xdist * xdist) + (ydist * ydist));

	if (xdist > 0)
	    p.x = pl->dragstart.x - 65;
	else
	    p.x = pl->dragstart.x + 10;
	p.y = pl->dragstart.y - (pydist / 2);
	sprintf (tmp,"%.3f %s", ydist,
		 (pl->y_units&&*pl->y_units)?pl->y_units:
		 pl->y_type == TIMEVAL?"sec":
		 "");
	XDrawString(pl->dpy, pl->win, gc, p.x, p.y, tmp, strlen(tmp));

	p.x = pl->dragstart.x + (pxdist / 2);
	if (ydist > 0)
	    p.y = pl->dragend.y - 5;
	else
	    p.y = pl->dragend.y + 15;
	sprintf (tmp,"%.3f %s", xdist,
		 (pl->x_units&&*pl->x_units)?pl->x_units:
		 pl->x_type == TIMEVAL?"sec":
		 "");
	XDrawString(pl->dpy, pl->win, gc, p.x, p.y, tmp, strlen(tmp));

	p.x = ((pl->dragend.x - pl->dragstart.x) / 2) + pl->dragstart.x;
	p.y = ((pl->dragend.y - pl->dragstart.y) / 2) + pl->dragstart.y;
	if (xdist > 0)
	    p.x += 15;
	else
	    p.x -= 85;
	if (ydist > 0)
	    p.y += 10;
	else
	    p.y -= 20;
	sprintf (tmp,"s = %.3f %s/%s", slope,
		 ((pl->y_units&&*pl->y_units)?pl->y_units:pl->y_type == TIMEVAL?"sec":"units"),
                 ((pl->x_units&&*pl->x_units)?pl->x_units:pl->x_type == TIMEVAL?"sec":"units")
		 );
//	sprintf (tmp,"s = %.3f", slope);
	XDrawString(pl->dpy, pl->win, gc, p.x, p.y, tmp, strlen(tmp));
/* Removed the diagonal distance, it is wrong. The value was being calculated
 * without taking into account the units of the x & y axis. Since it is not 
 * always possible to convert the different axis types from one form to the other,
 * it is safe to remove this value all together.
        p.y += 15;
	sprintf (tmp,"d = %.3f", hyp_dist);
	XDrawString(pl->dpy, pl->win, gc, p.x, p.y, tmp, strlen(tmp));
 */ 
    }
#endif /* TCPTRACE */
    break;

  default:
    /* we are called in motion notify */
    break;
  }
  return;
}

int main(int argc, char *argv[])
{

  command *c;
  int dummy_int;
  int option_tile = FALSE;
  int option_one_at_a_time = FALSE;
  unsigned int dummy_unsigned_int;
  Window dummy_window;
  Display *dpy = 0;
  Display *dpy2 = 0;
  Display *dpy_of_event = 0;
  PLOTTER pl;
  coord x_synch_bb_left;
  coord y_synch_bb_bottom;
  coord x_synch_bb_right;
  coord y_synch_bb_top;

  XEvent event;
  int i;
  /* Are x and/or y axis of the various windows locked together? */
  bool x_synch = FALSE, y_synch = FALSE;

  global_argc = argc;
  global_argv = argv;

#ifdef TCPTRACE
  {
      extern char* version_string;
      printf("Based on Tim Shepard's version 0.90.7 xplot\n");
      printf("Tcptrace-hosted version: %s\n", version_string);
  }
#endif /* TCPTRACE */

  /* Look for -v or -version argument */
  for (i = 1; i < argc && *argv[i] == '-'; i++) {
    if (strcmp ("-v", argv[i]) == 0
#ifdef TCPTRACE
	|| (strcmp ("--version", argv[i]) == 0)
#endif /* TCPTRACE */
	|| strcmp ("-version", argv[i]) == 0) {
      extern char* version_string;
      printf("xplot version %s\n", version_string);
      exit(0);
    }
#ifdef TCPTRACE
    if ((strcmp ("-h", argv[i]) == 0)
	|| (strcmp ("-help", argv[i]) == 0)
	|| (strcmp ("--help", argv[i]) == 0)) {
        fprintf(stderr,"\n");
	fprintf(stderr,"usage: %s [options] file [files]\n", argv[0]);
       	fprintf(stderr,"------\n\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "--------\n");
	fprintf(stderr, " -x               synchronize the x axis of all displayed files\n");
	fprintf(stderr, " -y               synchronize the y axis of all displayed files\n");
	fprintf(stderr, " -tile            adjust initial sizes to fit multiple files on screen\n");
	fprintf(stderr, " -mono            monochrome output\n");
	fprintf(stderr, " -1               show each file one at a time, rather than all at once\n");
        fprintf(stderr, " -d               specify display\n");
	fprintf(stderr, " -d2              specify second display (for group viewing)\n");
	fprintf(stderr, " -geometry        WxH[+X+Y] (understands standard X11 geometry)\n");
	fprintf(stderr, " -display         same as -d\n");
        fprintf(stderr, " -thick           draw the plots with a thick stroke\n");
	fprintf(stderr, " -version         print version information\n");
	fprintf(stderr, " -help            show this help screen\n");
	fprintf(stderr, "\n");       
	fprintf(stderr, "Mouse Bindings:\n");
	fprintf(stderr, "---------------\n");       
	fprintf(stderr, " left             draw rectangle to zoom in, click to zoom out\n");
	fprintf(stderr, " middle           drag to scroll window\n");
	fprintf(stderr, " right            quit\n");
	fprintf(stderr, " SHIFT + left     drop postscript file\n");
	fprintf(stderr, " SHIFT + middle   drop postscript file, smaller image\n");
	fprintf(stderr, " SHIFT + right    drop postscript file, less verticle space\n");
	fprintf(stderr, " CTRL  + middle   drag out a box showing dimensions\n");
	fprintf(stderr, "\n");
	exit(0);
    }
#endif /* TCPTRACE */
  }

  i = 1;

  /* Look for -x and/or -y options, and look for -t option*/
  for (; i < argc && *argv[i] == '-'; i++)
    {
      if (strcmp ("-x", argv[i]) == 0)
	x_synch = TRUE;
      else if (strcmp ("-y", argv[i]) == 0)
	y_synch = TRUE;
      else if (strcmp ("-thick", argv[i]) == 0)
	option_thick = TRUE;
      else if (strcmp ("-tile", argv[i]) == 0)
	option_tile = TRUE;
      else if (strcmp ("-mono", argv[i]) == 0)
	option_mono = TRUE;
      else if (strcmp ("-1", argv[i]) == 0)
	option_one_at_a_time = TRUE;
      else if (strcmp ("-d", argv[i]) == 0
	       || strcmp ("-display", argv[i]) == 0) {
	i++;
	if (dpy == 0) {
	  dpy = XOpenDisplay(argv[i]);
#ifdef TCPTRACE
	  if (dpy == NULL) fatalerror("could not open display");
#else /* TCPTRACE */
	  if (dpy == NULL) panic("could not open display");
#endif /* TCPTRACE */
	} else {
	  if (dpy2 == 0) {
	    dpy2 = XOpenDisplay(argv[i]);
#ifdef TCPTRACE
	    if (dpy2 == NULL) fatalerror("could not open display");
#else /* TCPTRACE */
	    if (dpy2 == NULL) panic("could not open display");
#endif /* TCPTRACE */
	  } else {
	    panic("too many displays");
	  }
	}
      }
      else if (strcmp("-d2", argv[i]) == 0) {
	i++;
	if (dpy2 == 0) {
	  dpy2 = XOpenDisplay(argv[i]);
#ifdef TCPTRACE
	  if (dpy2 == NULL) fatalerror("could not open display");
#else /* TCPTRACE */
	  if (dpy2 == NULL) panic("could not open display");
#endif /* TCPTRACE */
	} else {
	  panic("too many d2's");
	}
      }
      else
	/* Give the user the benefit of the doubt and assume that
	   they want a file that starts with '-' */
	break;
    }
	
	       
  if (dpy == 0) {
    dpy = XOpenDisplay("");
#ifdef TCPTRACE
	  if (dpy == NULL) fatalerror("could not open display");
#else /* TCPTRACE */
	  if (dpy == NULL) panic("could not open display");
#endif /* TCPTRACE */
  }

  {
    int numbase = i;
    int numwins = argc - i;
    int k;

    if (i < argc)
      for (k = i; k < argc; k++) {
	/*      for (k = argc-1; k>=i; k--) { */
	FILE *fp = 0;
	int len;
	len = strlen(argv[k]);
	if (strcmp(&argv[k][len-3],".gz") == 0) {
	  char *command;
	  command = (char *) malloc(50 + len);
	  if (command != 0) {
	    sprintf(command, "zcat %s", argv[k]);
	    fp = popen(command, "r");
	    free(command);
	  }
	} else {
	  fp = fopen(argv[k],"r");
	}
	if (fp) {
	  if (option_tile) {
	    new_plotter(fp, dpy, numwins, k-numbase, 0);
	  } else {
	    new_plotter(fp, dpy, 0, 0, 0);
	  }
	  fclose(fp);
	}
      }
    else
      /* 1 window, 0th */
      new_plotter(stdin, dpy, 1, 0, 0);

    if (dpy2 != 0) {

      if (i < argc)
	for (k = i; k < argc; k++) {
	  /*      for (k = argc-1; k>=i; k--) { */
	  FILE *fp = 0 ;
	  int len;
	  len = strlen(argv[k]);
	  if (strcmp(&argv[k][len-3],".gz") == 0) {
	    char *command;
	    command = (char *) malloc(50 + len);
	    if (command != 0) {
	      sprintf(command, "zcat %s", argv[k]);
	      fp = popen(command, "r");
	      free(command);
	    }
	  } else {
	    fp = fopen(argv[k],"r");
	  }
	  if (fp) {
	    if (option_tile) {
	      new_plotter(fp, dpy2, numwins, k-numbase, 0);
	    } else {
	      new_plotter(fp, dpy2, 0, 0, 0);
	    }
	    fclose(fp);
	  }
	}
      else {
	/* 1 window, 0th */
	panic("can't do dpy2 with stdin (yet)");
	new_plotter(stdin, dpy2, 1, 0, 0);
      }


    }

  }

  if ( ! the_plotter_list )
    {
      fprintf(stderr, "NO PLOTTERS\n");
      goto doexit;
    }

  /* Check that we are dealing with the same coordinate types. */
  if (the_plotter_list)
    {
#define BUTFIRSTPLOTTERS pl = the_plotter_list->next; pl != NULL; pl = pl->next
      if (x_synch)
	{
	  coord_type t = the_plotter_list->x_type;
	  for (BUTFIRSTPLOTTERS)
	    if (pl->x_type != t)
	      panic ("Attempt to synchronize different coordinate types");
	}
      if (y_synch)
	{
	  coord_type t = the_plotter_list->y_type;
	  for (BUTFIRSTPLOTTERS)
	    if (pl->y_type != t)
	      panic ("Attempt to synchronize different coordinate types");
	}
    }

#define ALLPLOTTERS pl = the_plotter_list ; pl != NULL; pl = pl->next
  for (ALLPLOTTERS) {
    int virgin = 1;
    for (c = pl->commands; c != NULL; c = c->next)
      switch(c->type) {
      case LINE:
      case DLINE:
	if (virgin || ccmp(pl->x_type, c->xb, pl_x_left, <))
	  pl_x_left = c->xb;
	if (virgin || ccmp(pl->x_type, c->xb, pl_x_right, >))
	  pl_x_right = c->xb;
	if (virgin || ccmp(pl->y_type, c->yb, pl_y_bottom, <))
	  pl_y_bottom = c->yb;
	if (virgin || ccmp(pl->y_type, c->yb, pl_y_top, >))
	  pl_y_top = c->yb;
	virgin = 0;
      case X:
      case DOT:
      case PLUS:
      case BOX:
      case DIAMOND:
      case UTICK:
      case DTICK:
      case LTICK:
      case RTICK:
      case HTICK:
      case VTICK:
      case UARROW:
      case DARROW:
      case LARROW:
      case RARROW:
      case INVISIBLE:
      case TEXT:
	if (virgin || ccmp(pl->x_type, c->xa, pl_x_left, <))
	  pl_x_left = c->xa;
	if (virgin || ccmp(pl->x_type, c->xa, pl_x_right, >))
	  pl_x_right = c->xa;
	if (virgin || ccmp(pl->y_type, c->ya, pl_y_bottom, <))
	  pl_y_bottom = c->ya;
	if (virgin || ccmp(pl->y_type, c->ya, pl_y_top, >))
	  pl_y_top = c->ya;
	virgin = 0;
      case TITLE:
      case XLABEL:
      case YLABEL:
	break;
      }

    pl_x_right = bump_coord(pl->x_type, pl_x_right);
    pl_y_top   = bump_coord(pl->y_type, pl_y_top);

    pl->viewno += 1;
    pl_x_left   = pl->x_left[0];
    pl_x_right  = pl->x_right[0];
    pl_y_top    = pl->y_top[0];
    pl_y_bottom = pl->y_bottom[0];
  }

  if (x_synch) {
    int virgin = 1;
    for (ALLPLOTTERS) {
      if (virgin || ccmp(pl->x_type, pl_x_left, x_synch_bb_left, <))
	x_synch_bb_left = pl_x_left;
      if (virgin || ccmp(pl->x_type, pl_x_right, x_synch_bb_right, >))
	x_synch_bb_right = pl_x_right;
      virgin = 0;
    }
    for (ALLPLOTTERS) {
      pl_x_left = x_synch_bb_left;
      pl_x_right = x_synch_bb_right;
    }
  }

  if (y_synch) {
    int virgin = 1;
    for (ALLPLOTTERS) {
      if (virgin || ccmp(pl->y_type, pl_y_bottom, y_synch_bb_bottom, <))
	y_synch_bb_bottom = pl_y_bottom;
      if (virgin || ccmp(pl->y_type, pl_y_top, y_synch_bb_top, >))
	y_synch_bb_top = pl_y_top;
      virgin = 0;
    }
    for (ALLPLOTTERS) {
      pl_y_top = y_synch_bb_top;
      pl_y_bottom = y_synch_bb_bottom;
    }
  }

  for (ALLPLOTTERS) {
    if (option_one_at_a_time == FALSE
	|| pl->next == 0
	) {
      display_plotter(pl);
    }
  }
    
  do {
    int SAVx, SAVy, SAVc, SAVd;
    lXPoint a,b;
    if (XPending(dpy) == 0
	&& (dpy2 == 0 || XPending(dpy2) == 0)
	) {
      int visible_count = 0;
      for (ALLPLOTTERS) {

	/* if option_one_at_a_time, ensure that at least last plotter
	   on list is visible if no others are */ 
	if (visible_count == 0
	    && pl->next == 0
	    && pl->win == 0
	    && option_one_at_a_time
	    ) 
	  display_plotter(pl);

	/* if plotter is not yet displayed, do nothing */
	if (pl->win == 0) continue;

	visible_count++;
	
	if (pl->size_changed) {
	  int i;
	  XRectangle xr[1];

	  pl->size_changed = 0;
	  pl->clean = 0;
	  size_window(pl);
	  XClearWindow(pl->dpy, pl->win);
	  pl->pointer_marks_on_screen = FALSE;

	  xr[0].x = pl->origin.x;
	  xr[0].y = pl->origin.y - 2;
	  xr[0].width = pl->size.x + 2;
	  xr[0].height = pl->size.y + 2;

	  for (i = 0; i < NColors; i++)
	    XSetClipRectangles(pl->dpy, pl->gcs[i], 0, 0, xr, 1, YXBanded);

	}
	if (pl->new_expose) {
	  pl->clean = 0;
	  for (c = pl->commands; c != NULL; c = c->next) {
	    if (c->mapped)
	      c->needs_redraw = TRUE;
	  }
	  pl->new_expose = 0;
	}
	if (pl->visibility != VisibilityFullyObscured && pl->clean == 0) {
	  SAVx = -100000; SAVy = -100000; SAVc = SAVd = 0;
	  for (c = pl->commands; c != NULL; c = c->next)
	    if (c->mapped)
	      if (c->needs_redraw) {
		GC gc;
		dXPoint da,db;
		c->needs_redraw = FALSE;
		if (c->decoration
		    || c->type == TITLE
		    || c->type == XLABEL
		    || c->type == YLABEL)
		  gc = pl->decgc;
		else
		  if ( c->color >= 0 && c->color < NColors)
		    gc = pl->gcs[c->color];
		  else
		    gc = pl->gcs[0];

#ifndef WINDOW_COORDS_IN_COMMAND_STRUCT
		da.x = map_coord(pl->x_type, pl_x_left, pl_x_right, pl->size.x,
				 c->xa);
		da.y = (pl->size.y - 1) -
		  map_coord(pl->y_type, pl_y_bottom, pl_y_top, pl->size.y,
			    c->ya);

		db.x = map_coord(pl->x_type, pl_x_left, pl_x_right, pl->size.x,
				 c->xb);
		db.y = (pl->size.y - 1) -
		  map_coord(pl->y_type, pl_y_bottom, pl_y_top, pl->size.y,
			    c->yb);

		da = tomain(pl,da);
		db = tomain(pl,db);
#else
		da.x = c->a.x;
		da.y = c->a.y;
		db.x = c->b.x;
		db.y = c->b.y;
#endif
		{
#if 1 /* Xqdss has bugs, so we need to clamp at just a few thousand */
#define CLAMP 3000.0
#else
#define CLAMP 10000.0
#endif
		  if (da.x >  CLAMP) {
		    if (db.x-da.x != 0)
		      da.y -= (db.y-da.y)*(da.x-CLAMP)/(db.x-da.x);
		    da.x =  CLAMP;}
		  if (da.x < -CLAMP) {
		    if (db.x-da.x != 0)
		      da.y -= (db.y-da.y)*(da.x+CLAMP)/(db.x-da.x);
		    da.x = -CLAMP;}
		  if (da.y >  CLAMP) {
		    if (db.y-da.y != 0)
		      da.x -= (db.x-da.x)*(da.y-CLAMP)/(db.y-da.y);
		    da.y =  CLAMP;}
		  if (da.y < -CLAMP) {
		    if (db.y-da.y != 0)
		      da.x -= (db.x-da.x)*(da.y+CLAMP)/(db.y-da.y);
		    da.y = -CLAMP;}

		  if (db.x >  CLAMP) {
		    if (da.x-db.x != 0)
		      db.y -= (da.y-db.y)*(db.x-CLAMP)/(da.x-db.x);
		    db.x =  CLAMP;}
		  if (db.x < -CLAMP) {
		    if (da.x-db.x != 0)
		      db.y -= (da.y-db.y)*(db.x+CLAMP)/(da.x-db.x);
		    db.x = -CLAMP;}
		  if (db.y >  CLAMP) {
		    if (da.y-db.y != 0)
		      db.x -= (da.x-db.x)*(db.y-CLAMP)/(da.y-db.y);
		    db.y =  CLAMP;}
		  if (db.y < -CLAMP) {
		    if (da.y-db.y != 0)
		      db.x -= (da.x-db.x)*(db.y+CLAMP)/(da.y-db.y);
		    db.y = -CLAMP;}

		  a.x = (int) rint(da.x);
		  a.y = (int) rint(da.y);
		  b.x = (int) rint(db.x);
		  b.y = (int) rint(db.y);
		}
		
		switch (c->type) {
		case DLINE:
		  XDrawLine(pl->dpy, pl->win, gc, a.x, a.y-2, a.x, a.y+2);
		  XDrawLine(pl->dpy, pl->win, gc, a.x-2, a.y, a.x+2, a.y);
		  XDrawLine(pl->dpy, pl->win, gc, b.x, b.y-2, b.x, b.y+2);
		  XDrawLine(pl->dpy, pl->win, gc, b.x-2, b.y, b.x+2, b.y);
		  /*fall through*/
		case LINE:
		  XDrawLine(pl->dpy, pl->win, gc, a.x, a.y, b.x, b.y);
		  break;
		case X:
		  XDrawLine(pl->dpy, pl->win, gc,
			    a.x - 2, a.y - 2, a.x + 2, a.y + 2);
		  XDrawLine(pl->dpy, pl->win, gc,
			    a.x - 2, a.y + 2, a.x + 2, a.y - 2);
		  break;
		case DOT:
		  /* Lines are much faster on some displays */
		  if ( SAVx == a.x && SAVy == a.y )
		    {
		      SAVc++;
		    }
		  else
		    {
		      SAVd++;
		      SAVx = a.x; SAVy = a.y;
#if 0
		      /*    
		            ***
			   *****
                           *****
                           *****
                            ***
		       */

		      XDrawLine(pl->dpy, pl->win, gc,a.x-1,a.y-2, a.x+1,a.y-2);
		      XDrawLine(pl->dpy, pl->win, gc,a.x-2,a.y-1, a.x+2,a.y-1);
		      XDrawLine(pl->dpy, pl->win, gc,a.x-2,a.y  , a.x+2,a.y  );
		      XDrawLine(pl->dpy, pl->win, gc,a.x-2,a.y+1, a.x+2,a.y+1);
		      XDrawLine(pl->dpy, pl->win, gc,a.x-1,a.y+2, a.x+1,a.y+2);
#else
#if 1
		      /*
			     *
			    ***
			     *
		       */


		      XDrawLine(pl->dpy, pl->win, gc, a.x, a.y-1, a.x, a.y+1);
		      XDrawLine(pl->dpy, pl->win, gc, a.x-1, a.y, a.x+1, a.y);
#else
		      /*
			     *                           
		       */

		      XDrawPoint(pl->dpy, pl->win, gc, a.x, a.y);
#endif		      
#endif
		    }
		  break;
		case PLUS:
		  XDrawLine(pl->dpy, pl->win, gc,
			    a.x, a.y - 2, a.x, a.y + 2);
		  XDrawLine(pl->dpy, pl->win, gc,
			    a.x - 2, a.y, a.x + 2, a.y);
		  break;
		case BOX:
		  { XSegment segs[4];
		    int x,y;
		    const int BOXRADIUS=3;

		    /* use XDrawSegments so that things can
		       still be collected into one big PolySegment by xlib
		       
		       --0-|
		       |   |
		       3   1
		       |   |
		       |-2--  */

		    x = a.x;
		    segs[0].x1 = segs[3].x1 = segs[3].x2 = x-BOXRADIUS;
		    segs[2].x2 = x-(BOXRADIUS-1);
		    segs[0].x2 = x+(BOXRADIUS-1);
		    segs[1].x1 = segs[1].x2 = segs[2].x1 = x+BOXRADIUS;

		    y = a.y;
		    segs[0].y1 = segs[0].y2 = segs[1].y1 = y-BOXRADIUS;
		    segs[3].y2 = y-(BOXRADIUS-1);
		    segs[1].y2 = y+(BOXRADIUS-1);
		    segs[2].y1 = segs[2].y2 = segs[3].y1 = y+BOXRADIUS;
		    
		    XDrawSegments(pl->dpy, pl->win, gc, segs, 4);
		  }
		  break;
		case DIAMOND:
		  { XSegment segs[4];
		    int x,y;
		    /*
		          /
		         1 \
		        /   2
		       \     \
		        0   / 
		         \ 3
			  /
			 
			  */
		    x = a.x;
		    y = a.y;
		    segs[0].x1 = x-1; segs[0].y1 = y+2;
		    segs[0].x2 = x-3; segs[0].y2 = y;
		    segs[1].x1 = x-2; segs[1].y1 = y-1;
		    segs[1].x2 = x;   segs[1].y2 = y-3;
		    segs[2].x1 = x+1; segs[2].y1 = y-2;
		    segs[2].x2 = x+3; segs[2].y2 = y;
		    segs[3].x1 = x+2; segs[3].y1 = y+1;
		    segs[3].x2 = x;   segs[3].y2 = y+3;
		    
		    XDrawSegments(pl->dpy, pl->win, gc, segs, 4);
		  }
		  break;
#define D (pl->thick? 6: 3)
		case UTICK:
		  XDrawLine(pl->dpy, pl->win, gc, a.x, a.y, a.x, a.y-D);
		  break;
		case DTICK:
		  XDrawLine(pl->dpy, pl->win, gc, a.x, a.y, a.x, a.y+D);
		  break;
		case RTICK:
		  XDrawLine(pl->dpy, pl->win, gc, a.x, a.y, a.x+D, a.y);
		  break;
		case LTICK:
		  XDrawLine(pl->dpy, pl->win, gc, a.x, a.y, a.x-D, a.y);
		  break;
		case HTICK:
		  XDrawLine(pl->dpy, pl->win, gc, a.x-D, a.y, a.x+D, a.y);
		  break;
		case VTICK:
		  XDrawLine(pl->dpy, pl->win, gc, a.x, a.y-D, a.x, a.y+D);
		  break;
#undef D
#define D (pl->thick? 4: 2)
		case UARROW:
		  XDrawLine(pl->dpy, pl->win, gc, a.x - D, a.y + D, a.x, a.y);
		  XDrawLine(pl->dpy, pl->win, gc, a.x + D, a.y + D, a.x, a.y);
		  break;
		case DARROW:
		  XDrawLine(pl->dpy, pl->win, gc, a.x - D, a.y - D, a.x, a.y);
		  XDrawLine(pl->dpy, pl->win, gc, a.x + D, a.y - D, a.x, a.y);
		  break;
		case RARROW:
		  XDrawLine(pl->dpy, pl->win, gc, a.x - D, a.y - D, a.x, a.y);
		  XDrawLine(pl->dpy, pl->win, gc, a.x - D, a.y + D, a.x, a.y);
		  break;
		case LARROW:
		  XDrawLine(pl->dpy, pl->win, gc, a.x + D, a.y - D, a.x, a.y);
		  XDrawLine(pl->dpy, pl->win, gc, a.x + D, a.y + D, a.x, a.y);
#undef D
		  break;
		case TEXT:
		case TITLE:
		case XLABEL:
		case YLABEL:
		  { 
		    lXPoint p;
		    int direction;
		    int font_ascent;
		    int font_descent;
		    XCharStruct xcs;
		    /* int height; */
		    int width;
		    int space = 5;

		    if (c->type == TITLE) {
		      p.x = (int) rint(pl->size.x / 2);
		      p.y = 0;
		      p = lXPoint_from_dXPoint(tomain(pl,
						      dXPoint_from_lXPoint(p)
						      ));
		      c->position = ABOVE;
		    } else if (c->type == XLABEL) {
		      p.x = (int) pl->size.x;
		      p.y = (int) pl->size.y + 22;
		      p = lXPoint_from_dXPoint(tomain(pl,
						      dXPoint_from_lXPoint(p)
						      ));
		      c->position = TO_THE_LEFT;
		    } else if (c->type == YLABEL) {
		      p.x = 0;
		      p.y = -5;
		      p = lXPoint_from_dXPoint(tomain(pl,
						      dXPoint_from_lXPoint(p)
						      ));
		      c->position = ABOVE;
		    } else
		      p = a;

		    XTextExtents(pl->font_struct, c->text, strlen(c->text),
				 &direction, &font_ascent, &font_descent, &xcs);
		    width = xcs.width;
		    /* height = font_ascent + font_descent; */
		    switch (c->position) {
		    case CENTERED:
		    case ABOVE:
		    case BELOW:        p.x -= width/2;       break;
		    case TO_THE_LEFT:  p.x -= width + space; break;
		    case TO_THE_RIGHT: p.x += space;         break;
		    default: panic("drawloop, case TEXT: unknown text positioning");
		    }
		    switch (c->position) {
		    case CENTERED:
		    case TO_THE_LEFT:
		    case TO_THE_RIGHT: p.y += font_ascent/2;        break;
		    case ABOVE:        p.y -= space + font_descent; break;
		    case BELOW:        p.y += space + font_ascent;  break;
		    default: panic("drawloop, case TEXT: unknown text positioning");
		    }

		    XDrawString(pl->dpy, pl->win, gc, p.x, p.y,
				c->text, strlen(c->text));
		  }
		  break;
		default:
		  panic("unknown command type");
		}
		/* if something has happened, stop drawing and go handle it */
		if (pl->state != NORMAL) XSync(pl->dpy,False);
		if (XEventsQueued(dpy, QueuedAlready) != 0) break;
		if (dpy2 != 0 &&
		    XEventsQueued(dpy2, QueuedAlready) != 0) break;
	      }
	  if (c == NULL) pl->clean = 1;
/* #define SAVE_PRINTOUTS */
#ifdef SAVE_PRINTOUTS
	  fprintf(stderr, "saved %d/%d %f%%\n",
		  SAVc, SAVc+SAVd, SAVc*100.0/(SAVc+SAVd) );
#endif
	}
      }
      if (visible_count == 0) break; /* will exit */
    }

    do {
      if (XPending(dpy) != 0) {
	XNextEvent(dpy, &event);	
	dpy_of_event = dpy;
	break;
      } else if (dpy2 != 0 && XPending(dpy2) != 0) {
	XNextEvent(dpy2, &event);
	dpy_of_event = dpy2;
	break;
      } else {
	fd_set fds;
	int maxfd_plus_1 = 0;
	int r;

	FD_ZERO(&fds);
	FD_SET(ConnectionNumber(dpy), &fds);
	maxfd_plus_1 = ConnectionNumber(dpy) + 1;
	if (dpy2 != 0) {
	  FD_SET(ConnectionNumber(dpy2), &fds);
	  maxfd_plus_1 = max(maxfd_plus_1, ConnectionNumber(dpy2)+1);
	}
	r = select (maxfd_plus_1, &fds, 0, 0, 0);
	if (r < 0) {
	  perror("select");
	  exit(1);
	}
      }
    } while(1);

    for (ALLPLOTTERS)
      if (pl->dpy == dpy_of_event
	  && pl->win == event.xany.window)
	break;
    if (pl == 0)
      continue; /* this happens when windows get deleted */

#if 0
    fprintf(stderr,"event %d\n", event.type);
#endif
    switch(event.type) {
    case Expose:
      pl->new_expose = 1;
      if (pl->pointer_marks_on_screen) {
	draw_pointer_marks(pl, pl->bacgc);
	pl->pointer_marks_on_screen = FALSE;
      }
      continue;
    case VisibilityNotify:
      pl->visibility = event.xvisibility.state;
      break;
    case ConfigureNotify:
      /* see if the window has changed size */
      if (pl->mainsize.x != event.xconfigure.width ||
	  pl->mainsize.y != event.xconfigure.height)
	{
	  pl->mainsize.x = event.xconfigure.width;
	  pl->mainsize.y = event.xconfigure.height;

	  pl->size_changed = 1;
	}
      break;
    case MapNotify:
      if (pl->mainsize.x == 0 && pl->mainsize.y == 0) {
	pl->mainsize.x = pl->xsh.width;
	pl->mainsize.y = pl->xsh.height;
	pl->size_changed = 1;
      }
      break;
    case ButtonPress:
      if (pl->pointer_marks_on_screen) {
	draw_pointer_marks(pl, pl->xorgc);
	pl->pointer_marks_on_screen = FALSE;
      }
      if (x_synch && y_synch) {
	PLOTTER savepl = pl;
	
	for (ALLPLOTTERS) {
	  if (pl == savepl) continue;
	  if (pl->pointer_marks_on_screen) {
	    draw_pointer_marks(pl, pl->xorgc);
	    pl->pointer_marks_on_screen = FALSE;
	  }
	}
	pl = savepl;
      }
      pl->buttonsdown += 1;

#ifdef TCPTRACE
      show_dist = 0;
#endif /* TCPTRACE */

      switch (pl->state) {
      case NORMAL:
	switch(event.xbutton.button) {
	case Button1:
	  if (event.xbutton.state & ShiftMask)
	    pl->state = PRINTING;
	  else
	    if (event.xbutton.y > pl->size.y + pl->origin.y)
	      pl->state = HZOOM;
	    else if (event.xbutton.x < pl->origin.x)
	      pl->state = VZOOM;
	    else
	      pl->state = ZOOM;
	  break;
	case Button2:
	  if (event.xbutton.state & ShiftMask)
	    pl->state = FIGING;
#ifdef TCPTRACE
	  else if (event.xbutton.state & ControlMask)
	  {
	      show_dist = 1;
	      if (event.xbutton.y > pl->size.y + pl->origin.y)
		  pl->state = HDRAG;
	      else if (event.xbutton.x < pl->origin.x)
		  pl->state = VDRAG;
	      else
		  pl->state = DRAG;
	  }
#endif /* TCPTRACE */
	  else
	    if (event.xbutton.y > pl->size.y + pl->origin.y)
	      pl->state = HDRAG;
	    else if (event.xbutton.x < pl->origin.x)
	      pl->state = VDRAG;
	    else
	      pl->state = DRAG;
	  break;
	case Button3:
	  if (event.xbutton.state & ControlMask) {
	    pl->state = EXITING;
	  } else {
	    if (option_one_at_a_time) {
	      if (event.xbutton.state & ShiftMask) {
		pl->state = BACKINGUP;
	      } else {
		pl->state = ADVANCING;
	      }
	    } else {
	      if (event.xbutton.state & ShiftMask) {
		pl->state = THINFIGING;
	      } else {
		pl->state = EXITING;
	      }
	    }
	  }
	  break;
	 default:
	  pl->state = WEDGED;
	}

	/***** should draw rectangle for HZOOM & VZOOM
	  For HZOOM, should setup dragend.y with opposite end of
	  desired rectangle and leave it be during the rest of the
	  HZOOM operation.  (same for VZOOM) 

	  should rethink HDRAG and VDRAG visual feedback
	  (maybe use filled solid rectangles drawn in xor mode?)

	  */


	pl->raw_dragstart.x = event.xbutton.x;
	pl->raw_dragstart.y = event.xbutton.y;

	pl->dragstart = pl->raw_dragstart;
	switch(pl->state) {
	case HZOOM:
	  pl->dragstart.y = pl->origin.y;
	  break;
	case VZOOM:
	  pl->dragstart.x = pl->origin.x;
	  break;
	default:
	  break;
	}
	pl->dragstart = detent(pl, pl->dragstart);
	switch (pl->state) {
	case HDRAG:
	  pl->dragstart.y = pl->origin.y + pl->size.y - 1;
	  break;
	case VDRAG:
	  pl->dragstart.x = pl->origin.x;
	  break;
	default:
	  break;
	}
	if (XQueryPointer(pl->dpy, pl->win, &dummy_window, &dummy_window,
			  &dummy_int, &dummy_int,
			  &(pl->pointer.x), &(pl->pointer.y),
			  &dummy_unsigned_int)
	    == 0) {
	  pl->state = WEDGED;
	  break;
	}
	pl->pointer_marks = detent(pl, pl->pointer);

	pl->dragend = detent(pl, pl->pointer);
	switch(pl->state) {
	case HZOOM:
	  pl->dragend.y = pl->origin.y + pl->size.y;
	  break;
	case VZOOM:
	  pl->dragend.x = pl->origin.x + pl->size.x;
	  break;
	default:
	  break;
	}

	if (x_synch && y_synch) {
	  PLOTTER savepl = pl;

	  for (ALLPLOTTERS) {
	    if (pl == savepl) continue;

	    if (pl -> state == NORMAL) {
	      pl->state = SLAVE;
	      pl->master = savepl;
	      pl->master_state = savepl->state;
	      pl->dragstart = map_pl_pl(savepl, pl, savepl->dragstart);
	      pl->dragend = map_pl_pl(savepl, pl, savepl->dragend);
	    }
	  }
	  pl = savepl;
	}

	break;

	/* remaining cases of a button down event when not in state NORMAL
	   in each case, go to state WEDGED after cleaning up the state we
	   are coming from */
      case ZOOM:
      case HZOOM:
      case VZOOM:
      case HDRAG:
      case VDRAG:
      case DRAG:
	pl->state = WEDGED; /* this must be after undrawing on slaves */
	break;
      case EXITING:
      case PRINTING:
      case FIGING:
      case THINFIGING:
      case ADVANCING:
      case BACKINGUP:
	pl->state = WEDGED;
	break;
      case WEDGED:
	break;
      case SLAVE:
	XBell(pl->dpy,100);
	break;
      default:
	panic("unknown state");
	break;
      }
      
      draw_pointer_marks(pl, pl->xorgc);
      pl->pointer_marks_on_screen = TRUE;

      if (x_synch && y_synch) {
	PLOTTER savepl = pl;

	for (ALLPLOTTERS) {
	  if (pl == savepl) continue;

	  draw_pointer_marks(pl, pl->xorgc);
	  pl->pointer_marks_on_screen = TRUE;
	}
	pl = savepl;
      }
      
      break;
    case ButtonRelease:
      pl->buttonsdown -= 1;
      if (pl->state != SLAVE) {
	if (pl->pointer_marks_on_screen) {
	  draw_pointer_marks(pl, pl->xorgc);
	  pl->pointer_marks_on_screen = FALSE;
	}
	if (x_synch && y_synch) {
	  PLOTTER savepl = pl;
	  
	  for (ALLPLOTTERS) {
	    if (pl == savepl) continue;
	    if (pl->pointer_marks_on_screen) {
	      draw_pointer_marks(pl, pl->xorgc);
	      pl->pointer_marks_on_screen = FALSE;
	    }
	  }
	  pl = savepl;
	}
	
	if (x_synch && y_synch) {
	  PLOTTER savepl = pl;
	  for (ALLPLOTTERS) {
	    if (pl == savepl) continue;
	    if (pl->state == SLAVE) {
	      if (pl->buttonsdown == 0)
		pl->state = NORMAL;
	      else
		pl->state = WEDGED;
	    }
	  }
	  pl = savepl;
	}

      }

      switch (pl->state) {
      case ZOOM:
      case HZOOM:
      case VZOOM:
	pl->dragend.x = event.xbutton.x;
	pl->dragend.y = event.xbutton.y;
	switch(pl->state) {
	case HZOOM:
	  pl->dragend.y = pl->origin.y + pl->size.y;
	  break;
	case VZOOM:
	  pl->dragend.x = pl->origin.x + pl->size.x;
	  break;
	default:
	  break;
	}
	pl->dragend = detent(pl, pl->dragend);
	break;
      case HDRAG:
	pl->dragend.x = event.xbutton.x;
	pl->dragend.y = pl->dragstart.y;
	pl->dragend = detent(pl, pl->dragend);
	break;
      case VDRAG:
	pl->dragend.x = pl->dragstart.x;
	pl->dragend.y = event.xbutton.y;
	pl->dragend = detent(pl, pl->dragend);
	break;
      case DRAG:
	pl->dragend.x = event.xbutton.x;
	pl->dragend.y = event.xbutton.y;
	pl->dragend = detent(pl, pl->dragend);
	break;
      default:
	break;
      }
      switch (pl->state) {
      case ZOOM:
      case HZOOM:
      case VZOOM:
      case DRAG:
      case HDRAG:
      case VDRAG:
	pl->dragstart =
	  lXPoint_from_dXPoint(tosub(pl, dXPoint_from_lXPoint(pl->dragstart)));
	pl->dragend =
	  lXPoint_from_dXPoint(tosub(pl, dXPoint_from_lXPoint(pl->dragend)));
	break;
      default:
	break;
      }
      switch (pl->state) {
      case ZOOM:
      case HZOOM:
      case VZOOM:
	{
	  lXPoint dragstart, dragend;
	  PLOTTER savepl = pl;
	  bool must_pop_others = FALSE, do_x = FALSE, do_y = FALSE;

	  dragstart = pl->dragstart;
	  dragend = pl->dragend;

	  /* Do the plotter in which the button was pressed first, then do
	     all the others if we are synchronizing (and need to). */
	  if ((abs(pl->raw_dragstart.x - event.xbutton.x) > 7) ||
	      (abs(pl->raw_dragstart.y - event.xbutton.y) > 7)) {

	    if (((pl->state != VZOOM) && (abs(dragstart.x - dragend.x) > 7))
		|| ((pl->state != HZOOM) && (abs(dragstart.y - dragend.y) > 7))) {

	      int newviewno;
	      
	      newviewno = pl->viewno + 1;
	      if (newviewno == NUMVIEWS) newviewno -= 1;
	
	      if (abs(dragstart.x - dragend.x) > 7) {
		zoom_in_coord(pl->x_type, pl_x_left, pl_x_right,
			      dragstart.x, dragend.x,
			      pl->size.x,
			      &(pl->x_left[newviewno]),
			      &(pl->x_right[newviewno]));
		do_x = x_synch;
	      } else {
		pl->x_left[newviewno] = pl_x_left;
		pl->x_right[newviewno] = pl_x_right;
	      }
	      if (abs(dragstart.y - dragend.y) > 7) {
		zoom_in_coord(pl->y_type, pl_y_bottom, pl_y_top,
			      pl->size.y - dragstart.y,
			      pl->size.y - dragend.y,
			      pl->size.y,
			      &(pl->y_bottom[newviewno]),
			      &(pl->y_top[newviewno]));
		do_y = y_synch;
	      } else {
		pl->y_bottom[newviewno] = pl_y_bottom;
		pl->y_top[newviewno] = pl_y_top;
	      }
	      pl->viewno = newviewno;
	    } else {
	      /* do nothing  (don't zoom and don't pop) */
	      if (0) goto G0093;
	    }
	  } else {
	    if (pl->viewno > 1)
	      {
		/* Only pop the others if the synchronized axis changed. */
		if ((x_synch
		     && (xcmp(pl_x_left, pl->x_left[pl->viewno-1], !=)
			 || xcmp(pl_x_right, pl->x_right[pl->viewno-1], !=)))
		    || (y_synch
			&& (ycmp(pl_y_top, pl->y_top[pl->viewno-1], !=)
			    || ycmp(pl_y_bottom, pl->y_bottom[pl->viewno-1], !=))))
		  must_pop_others = TRUE;
		pl->viewno--;
	      }
	    else {
	      pl_x_left   = pl->x_left[0];
	      pl_x_right  = pl->x_right[0];
	      pl_y_top    = pl->y_top[0];
	      pl_y_bottom = pl->y_bottom[0];
	    }
	  }
	  XClearWindow(pl->dpy, pl->win);
	  pl->pointer_marks_on_screen = FALSE;
	  pl->size_changed = 1;
	  pl->state = NORMAL;
	  /* If we need to, loop through all the plotters. */
	  if (do_x || do_y || must_pop_others)
	    for (ALLPLOTTERS)
	      {
		if (pl == savepl) continue;
		if (must_pop_others)
		  {
		    if (pl->viewno > 1) pl->viewno--;
		    if (x_synch)
		      {
			pl_x_left   = savepl->x_left[savepl->viewno];
			pl_x_right  = savepl->x_right[savepl->viewno];
		      }
		    if (y_synch)
		      {
			pl_y_top    = savepl->y_top[savepl->viewno];
			pl_y_bottom = savepl->y_bottom[savepl->viewno];
		      }
		  }
		else
		  {
		    int newviewno;
		    newviewno = pl->viewno + 1;
		    if (newviewno == NUMVIEWS) newviewno -= 1;

		    if (do_x)  {
		      pl->x_left[newviewno] = savepl->x_left[savepl->viewno];
		      pl->x_right[newviewno] = savepl->x_right[savepl->viewno];
		    } else {
		      pl->x_left[newviewno] = pl_x_left;
		      pl->x_right[newviewno] = pl_x_right;
		    }
		    if (do_y)  {
		      pl->y_bottom[newviewno] =
			savepl->y_bottom[savepl->viewno];
		      pl->y_top[newviewno] = savepl->y_top[savepl->viewno];
		    } else {
		      pl->y_bottom[newviewno] = pl_y_bottom;
		      pl->y_top[newviewno] = pl_y_top;
		    }
		    pl->viewno = newviewno;

		    if ( do_x && ! y_synch)
		      shrink_to_bbox(pl,0,1);

		  }		
		pl->size_changed = 1;
	      }
	G0093:
	  pl = savepl;		/* Don't know if I have to do this, but... */
	}
        break;
      case DRAG:
      case HDRAG:
      case VDRAG:
#ifdef TCPTRACE
	if (!show_dist)
	{
#endif /* TCPTRACE */
	{
	  PLOTTER savepl = pl;

	  drag_coord(pl->x_type, pl_x_left, pl_x_right,
		     pl->dragend.x,
		     pl->dragstart.x,
		     pl->size.x,
		     &(pl_x_left), &(pl_x_right));
	  drag_coord(pl->y_type, pl_y_bottom, pl_y_top,
		     pl->size.y - pl->dragend.y,
		     pl->size.y - pl->dragstart.y,
		     pl->size.y,
		     &(pl_y_bottom), &(pl_y_top));
	
	  pl->size_changed = 1;
	  if (x_synch || y_synch)
	    for (ALLPLOTTERS)
	      {
		if (pl == savepl) continue;
		if (x_synch && savepl->state != VDRAG)
		  {
		    pl_x_left = savepl->x_left[savepl->viewno];
		    pl_x_right = savepl->x_right[savepl->viewno];
		    if (!y_synch) shrink_to_bbox(pl,0,1);
		    pl->size_changed = 1;
		  }
		if (y_synch && savepl->state != HDRAG)
		  {
		    pl_y_top = savepl->y_top[savepl->viewno];
		    pl_y_bottom = savepl->y_bottom[savepl->viewno];
		    if (!x_synch) shrink_to_bbox(pl,1,0);
		    pl->size_changed = 1;
		  }
	      }
	  pl = savepl;
	}
        XClearWindow(pl->dpy, pl->win);
#ifdef TCPTRACE
	}
#endif /* TCPTRACE */
	pl->pointer_marks_on_screen = FALSE;
        pl->state = NORMAL;
        break;
      case EXITING:
	pl->state = NORMAL;  
	if (undisplay_plotter(pl, option_one_at_a_time?-1:0) == 0) {
	  /* went past last plotter */
	  if (option_one_at_a_time == 0) {
	    
	    /* old behavior */

	  } else {
	    /* we'll loop around, and start at the beginning with a
               new window */
	  }
	}
	break;
      case PRINTING:
      case FIGING:
      case THINFIGING:
	{
	  FILE *fp, *make_name_open_file();
	  if ((fp = make_name_open_file(pl))) {
	    emit_PS(pl, fp, pl->state);
	    (void) fclose(fp);
	    pl->size_changed = 1;
	  }
	}
        pl->state = NORMAL;
        break;
      case ADVANCING:
	pl->state = NORMAL;
	undisplay_plotter(pl, -1);
	break;
      case BACKINGUP:
	pl->state = NORMAL;
	if (undisplay_plotter(pl, option_one_at_a_time?1:0) == 0) {
	  /* trying to back up before first plotter */

	  /* display the last plotter (the first on the list) */
	  if (option_one_at_a_time) 
	    display_plotter(the_plotter_list);
	}
	break;
      case WEDGED:
	if (pl->buttonsdown == 0)
	  pl->state = NORMAL;
	break;
      case SLAVE:
	/* do nothing */
	break;
      default:
	/* Do nothing.  Note that this really can happen due to the semantics
	 * of the implicit grab associated with a button press event.
	 */
	break;
	
      }
      break;
    case EnterNotify:
      /*      printf("enter %d\n", pl->win); */
      break;
    case LeaveNotify:
      /*      printf("leave %d\n", pl->win); */
      if (pl->pointer_marks_on_screen) {
	draw_pointer_marks(pl, pl->xorgc);
	pl->pointer_marks_on_screen = FALSE;
      }
      if (x_synch && y_synch) {
	PLOTTER savepl = pl;

	for (ALLPLOTTERS) {
	  if (pl == savepl) continue;

	  if (pl->pointer_marks_on_screen) {
	    draw_pointer_marks(pl, pl->xorgc);
	    pl->pointer_marks_on_screen = FALSE;
	  }
	}
	pl = savepl;
      }

      break;
    case MotionNotify:
      if (pl->pointer_marks_on_screen) {
	draw_pointer_marks(pl, pl->xorgc);
	pl->pointer_marks_on_screen = FALSE;
      }
      if (XQueryPointer(pl->dpy, pl->win, &dummy_window, &dummy_window,
			&dummy_int, &dummy_int,
			&(pl->pointer.x), &(pl->pointer.y),
			&dummy_unsigned_int)
	  == 0) {
	pl->state = WEDGED;
	break;
      }
      if ( pl->size.x == 0 || pl->size.y == 0 )
	{
	  fprintf(stderr, "MotionNotify while size is zero ignored.\n");
	  break;
	}
      pl->pointer_marks = detent(pl, pl->pointer);

      if (pl->state != SLAVE) {
	pl->dragend = detent(pl, pl->pointer);
	switch(pl->state) {
	case HZOOM:
	  pl->dragend.y = pl->origin.y + pl->size.y;
	  break;
	case VZOOM:
	  pl->dragend.x = pl->origin.x + pl->size.x;
	  break;
	default:
	  break;
	}

      }

      draw_pointer_marks(pl, pl->xorgc);
      pl->pointer_marks_on_screen = TRUE;

      if (x_synch && y_synch) {
	PLOTTER savepl = pl;
	
	for (ALLPLOTTERS) {
	  if (pl == savepl) continue;
	  if (pl->win == 0) continue; /* -1 commandline option. Is this correct? */

	  if (pl->slave_draw_in_progress) {
	    pl->slave_motion_pending = TRUE;
	    pl->master_pointer = map_pl_pl(savepl, pl, savepl->pointer_marks);
	  } else {
	    if (pl->pointer_marks_on_screen) {
	      draw_pointer_marks(pl, pl->xorgc);
	      pl->pointer_marks_on_screen = FALSE;
	    }
	    pl->pointer_marks = map_pl_pl(savepl, pl, savepl->pointer_marks);
	    pl->dragend = pl->pointer_marks;
	    switch(pl->master_state) {
	    case HZOOM:
	      pl->dragend.y = pl->origin.y + pl->size.y;
	      break;
	    case VZOOM:
	      pl->dragend.x = pl->origin.x + pl->size.x;
	      break;
	    default:
	      break;
	    }
	    draw_pointer_marks(pl, pl->xorgc);
	    pl->pointer_marks_on_screen = TRUE;
	    pl->slave_draw_in_progress = TRUE;
	    {
	      XEvent e;
	      int r; 

	      e.xclient.type = ClientMessage;
	      e.xclient.serial = 0;
	      e.xclient.send_event = 1;
	      e.xclient.display = pl->dpy;
	      e.xclient.window = pl->win;
	      e.xclient.message_type = pl->xplot_nagle_atom;
	      e.xclient.format = 8;

	      r = XSendEvent(pl->dpy, pl->win, False, 0, &e);
	      if (r != 1)
		printf("DEBUG: XSendEvent returned %d\n", r);
	    }
	  }
	}
	pl = savepl;
      }
      
      break;
    case ClientMessage:
      if (event.xclient.message_type == pl->xplot_nagle_atom) {

#if 0
	printf("XPLOT_NAGLE\n");
#endif
	pl->slave_draw_in_progress = FALSE;

	if (pl->slave_motion_pending) {
	  if (pl->pointer_marks_on_screen) {
	    draw_pointer_marks(pl, pl->xorgc);
	    pl->pointer_marks_on_screen = FALSE;
	  }
	  pl->pointer_marks = pl->master_pointer;
	  pl->dragend = pl->master_pointer;
	  switch(pl->master_state) {
	  case HZOOM:
	    pl->dragend.y = pl->origin.y + pl->size.y;
	    break;
	  case VZOOM:
	    pl->dragend.x = pl->origin.x + pl->size.x;
	    break;
	  default:
	    break;
	  }
	  draw_pointer_marks(pl, pl->xorgc);
	  pl->pointer_marks_on_screen = TRUE;
	  pl->slave_motion_pending = FALSE;
	  pl->slave_draw_in_progress = TRUE;
	  {
	    XEvent e;
	    int r; 

	    e.xclient.type = ClientMessage;
	    e.xclient.serial = 0;
	    e.xclient.send_event = 1;
	    e.xclient.display = pl->dpy;
	    e.xclient.window = pl->win;
	    e.xclient.message_type = pl->xplot_nagle_atom;
	    e.xclient.format = 8;

	    r = XSendEvent(pl->dpy, pl->win, False, 0, &e);
	    if (r != 1)
	      printf("DEBUG: XSendEvent returned %d\n", r);
	  }
	}

      } else {
	printf("event ClientMessage unknown type %ld, %s\n",
	       event.xclient.message_type,
	       XGetAtomName(pl->dpy, event.xclient.message_type)
	       );
      }
      break;
    default:
#if 0      
      /* other events do happen (for example, ReparentNotify)
	 so just silently ignore them */ 
      printf("unknown event type %d\n", event.type);
#endif
      break;
    }
  } while (the_plotter_list != 0);

 doexit:
  if (dpy2 != 0) XCloseDisplay(dpy2);
  XCloseDisplay(dpy);
  return 0;
}

#ifdef __GNUC__
inline
#endif
static char **gettokens(FILE *fp)
{
  static char buf[1000];
  static char *tokens[1000];
  char *cp;
  int i;

  if (fgets(buf, sizeof(buf), fp) == NULL)
    return 0;

  i=0;
  cp=buf; 

  while (*cp != '\0') {
    while (*cp == ' ' || *cp == '\t') cp++;
    if (*cp == '\n') break;
    tokens[i++] = cp;
    while (*cp != ' ' && *cp != '\t' && *cp != '\n' && *cp != '\0') cp++;
    if (*cp == '\n' || *cp == '\0') {
      break;
    }
    *cp = '\0';
    cp++;
  }
  if (*cp == '\0') return 0;
  else *cp = '\0';
  tokens[i] = 0;
  return tokens;
}

#ifdef __GNUC__
static inline
int mystrcmp(char *s1, char *s2)
{
  while (*s1 == *s2++)
    if (*s1++ == '\0')
      return 0;
  return 1;
}
#else
#define mystrcmp strcmp
#endif

xpcolor_t parse_color(char *s)
{
  int atoi();
  int i;

  if (isdigit(*s))
    return (xpcolor_t) atoi(s);
  for (i=0; i < NCOLORS; ++i)
    if (mystrcmp(s,ColorNames[i]) == 0)
      return(i);
  return(-1);  /* not a color name */
}
 

int get_input(FILE *fp, Display *dpy, int lineno, struct plotter *pl)
{
  
  char **tokens;
  int ntokens = 0;
  command *com;

#define parseerror(s) \
  { \
      int i; \
      fprintf(stderr, \
	      "in line number %d: %s\noffending line: ", lineno, s); \
      for (i = 0; i < ntokens; i++) \
      { \
	  fputs(tokens[i], stderr); \
	  fputc(' ', stderr); \
      } \
      fputc('\n', stderr); \
      fflush(stderr); \
      exit(1); \
  }
  do {
    lineno++;
    tokens = gettokens(fp);
    if (tokens == 0) parseerror("EOF before first line of input");
    for (ntokens = 0; tokens[ntokens] != 0; ntokens++);
  } while (ntokens == 1 && mystrcmp(tokens[0], "new_plotter") == 0);
  
  if (ntokens != 2)
    parseerror("invalid input format -- expecting coord type names");

  pl->x_type = parse_coord_name(tokens[0]);
  pl->y_type = parse_coord_name(tokens[1]);

  if (((int) pl->x_type) < 0 || ((int) pl->y_type) < 0)
    parseerror("unknown coord type");
  
  for (;;) {

    lineno++;
    tokens = gettokens(fp);
    if (tokens == 0) break;
    for (ntokens = 0; tokens[ntokens] != 0; ntokens++);
    if (ntokens == 0) continue;
    if (tokens[0][0] == ';') continue;
    
    /* check for color key alone on a line */
    if (ntokens == 1) {
      xpcolor_t c;

      c = parse_color(tokens[0]);
      if (c != -1) {
	/* color keyword */
	pl->default_color = pl->current_color = c;
	continue;
      }
    }

    if (ntokens == 2 &&
	parse_coord_name(tokens[0]) == pl->x_type &&
	parse_coord_name(tokens[1]) == pl->y_type)
      continue;

#define BUFSIZE 1024     
#define not_ntokens_equal_to_3_or_4	(ntokens != 3 && ntokens != 4)
#define COLORfromTOK3  (com->color = ntokens == 4 ?\
			parse_color(tokens[3]) : pl->current_color)

    if (mystrcmp(tokens[0],"aspect_ratio") == 0) {
      if (ntokens != 2) parseerror("input format error");
      if (pl->x_type != pl->y_type)
	parseerror("aspect_ratio requires identical coordinate types");
      pl->aspect_ratio = atof(tokens[1]);
    } else if (mystrcmp(tokens[0],"x") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = X;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],".") == 0
	       || mystrcmp(tokens[0],"dot") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = DOT;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"+") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = PLUS;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"plus") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = PLUS;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"box") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = BOX;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"diamond") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = DIAMOND;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"utick") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = UTICK;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"dtick") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = DTICK;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
    } else if (mystrcmp(tokens[0],"ltick") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = LTICK;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"rtick") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = RTICK;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"htick") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = HTICK;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"vtick") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = VTICK;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"uarrow") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = UARROW;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"darrow") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = DARROW;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"larrow") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = LARROW;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"rarrow") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = RARROW;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"line") == 0) {
      if (ntokens != 5 && ntokens != 6) parseerror("input format error");
      com = new_command(pl);
      com->type = LINE;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      com->xb = parse_coord(pl->x_type, tokens[3]);
      com->yb = parse_coord(pl->y_type, tokens[4]);
      com->color = ntokens == 6 ? parse_color(tokens[5]) : pl->current_color;
    } else if (mystrcmp(tokens[0],"dline") == 0) {
      if (ntokens != 5 && ntokens != 6) parseerror("input format error");
      com = new_command(pl);
      com->type = DLINE;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      com->xb = parse_coord(pl->x_type, tokens[3]);
      com->yb = parse_coord(pl->y_type, tokens[4]);
      com->color = ntokens == 6 ? parse_color(tokens[5]) : pl->current_color;
    } else if (mystrcmp(tokens[0], "title") == 0) {
      char buf[BUFSIZE];
      char *cp;
      buf[0] = '\0';
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);

      com = new_command(pl);
      com->type = TITLE;
      com->text = cp;
    } else if (mystrcmp(tokens[0], "ctext") == 0) {
      char buf[BUFSIZE];
      char *cp;
      buf[0] = '\0';
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);
      com = new_command(pl);
      com->type = TEXT;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      com->text = cp;
      com->position = CENTERED;
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0], "atext") == 0) {
      char buf[BUFSIZE];
      char *cp;
      buf[0] = '\0';
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);
      com = new_command(pl);
      com->type = TEXT;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      com->text = cp;
      com->position = ABOVE;
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0], "btext") == 0) {
      char buf[BUFSIZE];
      char *cp;
      buf[0] = '\0';
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);
      com = new_command(pl);
      com->type = TEXT;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      com->text = cp;
      com->position = BELOW;
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0], "ltext") == 0) {
      char buf[BUFSIZE];
      char *cp;
      buf[0] = '\0';
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);
      com = new_command(pl);
      com->type = TEXT;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      com->text = cp;
      com->position = TO_THE_LEFT;
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0], "rtext") == 0) {
      char buf[BUFSIZE];
      char *cp;
      buf[0] = '\0';
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);
      com = new_command(pl);
      com->type = TEXT;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      com->text = cp;
      com->position = TO_THE_RIGHT;
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0], "xlabel") == 0) {
      char buf[1024];
      char *cp;
      buf[0] = '\0';
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);

      com = new_command(pl);
      com->type = XLABEL;
      com->text = cp;
    } else if (mystrcmp(tokens[0], "ylabel") == 0) {
      char buf[1024];
      char *cp;
      buf[0] = '\0';
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);

      com = new_command(pl);
      com->type = YLABEL;
      com->text = cp;
    } else if (mystrcmp(tokens[0], "xunits") == 0) {
      char buf[1024];
      char *cp;
      buf[0] = '\0';
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);

      pl->x_units = cp;
    } else if (mystrcmp(tokens[0], "yunits") == 0) {
      char buf[1024];
      char *cp;
      buf[0] = '\0';
      (void) fgets(buf, sizeof(buf), fp);
      for (cp=buf;*cp != '\0';cp++)
	if (*cp == '\n') { *cp = '\0'; break; }
      cp = malloc((unsigned)strlen(buf) + 1);
      if (cp == 0) fatalerror("malloc returned null");
      (void) strcpy(cp, buf);

      pl->y_units = cp;
    } else if (mystrcmp(tokens[0],"invisible") == 0) {
      if (not_ntokens_equal_to_3_or_4) parseerror("input format error");
      com = new_command(pl);
      com->type = INVISIBLE;
      com->xa = parse_coord(pl->x_type, tokens[1]);
      com->ya = parse_coord(pl->y_type, tokens[2]);
      COLORfromTOK3;
    } else if (mystrcmp(tokens[0],"go") == 0) {
#if 0
      fprintf(stderr,"xplot pid %d go!\n",getpid());
#endif
      break;
    } else if (mystrcmp(tokens[0],"new_plotter") == 0) {
      return lineno;
    } else
      parseerror("input format error");
  }
  return 0;
}


static char *esc_paren(char *s)
{
  static char buf[1024];
  char *b = buf;

  while (*s) {
    if (*s == '(' || *s == ')')
      *b++ = '\\';
    *b++ = *s++;
  }
  *b = '\0';
  return buf;
}
    
/******
  Function to emit a PostScript description of the current plot.
*/
void emit_PS(struct plotter *pl, FILE *fp, enum plstate state)
{
  struct plotter pspl;
  command *c, *cc;
  int counter;
  int currentcolor;
  bool finished_decoration, output_decoration;
  double figwidth;
  double figheight;
  double lmargin,rmargin,bmargin,tmargin;
  double bbllx,bblly,bburx,bbury;
  double limit_height;
  dXPoint a,b;

  /* Make a copy of both the plotter and its commands. */
  pspl = *pl;

#if 0
  /* Copy list of commands.  This is somewhat kludgy because the
   * order of the commands is important (at least all the decoration
   * commands must come first), so I can't use new_command().
   * It always puts the new command at the head of the list,
   * and I can only walk the old list head->tail.
   */

  /* The next statement is only possible because next is the
     first item in the command structure.  */
  cc = (command *) &pspl.commands;
  for (c = pl->commands; c != NULL; c = c->next)  {
    cc->next = (command *) malloc(sizeof(command));
    if (cc->next == 0) fatalerror("malloc returned null");
    *cc->next = *c;
    cc = cc->next;
    cc->next = NULL;
    /* If it is text, copy that too. */
    if (c->type == TEXT || c->type == TITLE)  {
      cc->text = malloc((unsigned)strlen(c->text) + 1);
      if (cc->text == 0) fatalerror("malloc returned null");
      (void) strcpy(cc->text, c->text);
    }
  }
#else
  /* Instead of copying we use the same commands.
   * Now the caller above recomputes everything when we return.
   */
  cc = pl->commands;
  pspl.commands = cc;
#endif

  /* Because xplot only deals with integer output coords, use PS units
   * which are a multiple of the pixels per inch of the actual printer.
   * By doing so, some undesirable effects are avoided.
   * 7200 is the least common multiple of 1440 and 1200.
   * There is some code below that just might depend on this being
   * a multiple of 600.   So think carefully if you are tuning this.
   */
#define PER_INCH 7200

  /* Calculate new window coordinates for everything. */
  { 
    command *c;
    switch(state) {
    default:
      panic("emit_PS: unexpected state");
    case PRINTING:
      /* landscape mode */
      lmargin = 1.25;
      rmargin = 0.75;
      bmargin = 0.85;
      tmargin = 0.75;
      figwidth = 11;
      figheight = 8.5;
      limit_height = figheight;
      break;
    case FIGING:
    case THINFIGING:
      /* portrait mode, for use in documents */
      lmargin = 0.7;
      rmargin = 0.2;
      bmargin = 0.3;
      tmargin = 0.15;
      figwidth = 6.0;
      figheight = 4.0; /* changed below if THINFIGING */
      limit_height = 7.5; /* biggest figure height TeX can handle */
      break;
    }

    if (state == THINFIGING)
      figheight = 2.75;

    if (pl->aspect_ratio != 0) {

      double plotter_width = figwidth - (lmargin+rmargin);
      double plotter_height = figheight - (bmargin+tmargin);

      /* stretch vertically to make aspect ratio correct */
      plotter_height = 1/pl->aspect_ratio * plotter_width;
      figheight = plotter_height + (bmargin+tmargin);

      if (figheight > limit_height) {
	/* figure is too tall, scale back plotter width
	   and height equally to fit */
	double scale =
	  (limit_height - (bmargin+tmargin)) /
	    plotter_height;
	plotter_height *= scale;
	plotter_width *= scale;

      }
	
      figwidth = plotter_width + (lmargin+rmargin);
      figheight = plotter_height + (bmargin+tmargin);
    }


    /* we translate the origin to provide margins, so bb is 0 to figsize */
    bbllx = 0.0 * 72.0;
    bblly = 0.0 * 72.0;
    bburx = figwidth * 72.0;
    bbury = figheight * 72.0;

    pspl.origin.x = 0; /* not necessary? */
    pspl.origin.y = 0;
    pspl.size.x = (int) ((figwidth - lmargin - rmargin) * PER_INCH);
    pspl.size.y = (int) ((figheight - tmargin - bmargin) * PER_INCH);

    /*************** abstraction violation!!! */
    /* code copied from size_window above */
    while (pspl.commands && pspl.commands->decoration) {
      c = pspl.commands;
      pspl.commands = pspl.commands->next;
      free_command(c);
    };
    
    axis(&pspl);
    
    for (c = pspl.commands; c != NULL; c = c->next)
      compute_window_coords(&pspl, c);
  }
  

  /*
   * Print out the prologue for the picture.
   */

  /*
   * GDT: 950310: put dot font hack in.
   */

#ifdef TCPTRACE
  fputs("%!PS\n", fp);
#else
  fputs("%!PostScript\n", fp);
#endif /* TCPTRACE */
  /* Bracket the PS program with gsave/grestore so these page descriptions
     can be concatenated, then printed. */
  fputs("%%BoundingBox: ", fp);
  fprintf(fp, "%g %g %g %g\n", bbllx, bblly, bburx, bbury);
  fputs("%%EndComments\n", fp);
  fputs("/Docolors true def\n", fp); /* gdt - delete to not print in color */
  fputs("gsave\n", fp);

  /* Set up scale */
  fputs("%/sign { dup 0 gt { pop 1 } { 0 lt { -1 } { 0 } ifelse } ifelse } def\n"
        "\n"
        "%matrix currentmatrix\n"
        "%aload pop\n"
        "%6 2 roll sign\n"
        "%6 1 roll sign\n"
        "%6 1 roll sign\n"
        "%6 1 roll sign\n"
        "%6 1 roll\n"
        "%matrix astore setmatrix\n"
        "\n", fp);

  fprintf(fp, "72 %d div dup scale\n", PER_INCH);

  fprintf(fp, "/theta {%d mul} def\n", ( (state == PRINTING) ? PER_INCH/150 : PER_INCH/300));

  /* Set up units of measurement. */
  fprintf(fp, "/inch {%d mul} def\n", PER_INCH);
  fputs("/pt {inch 72 div} def\n"
        "%\n"
        "%\n"
        "/tfont /Times-Bold findfont 12 pt scalefont def\n"
        "%\n"
        "/lfont /Times-Roman findfont 10 pt scalefont def\n"
        "%\n"
        "%tfont /FontBBox get\n"
        "%  aload pop\n"
        "%  tfont /FontMatrix get dtransform pop /tascent exch def\n"
        "%  tfont /FontMatrix get dtransform pop neg /tdescent exch def\n"
        "lfont /FontBBox get\n"
        "  aload pop\n"
        "  lfont /FontMatrix get dtransform pop 0.65 mul /lascent exch def\n"
        "  lfont /FontMatrix get dtransform pop neg /ldescent exch def\n"
        "% begin gdt mod\n"
        "% define font for xplot characters\n"
        "/BuildCharDict 10 dict def\n"
        "/Xplotfontdict 7 dict def\n"
        "Xplotfontdict begin\n"
        "  /FontType 3 def\n"
        "  /FontMatrix [1 0 0 1 0 0] def\n"
        "  /FontBBox [-1 -1 1 1]def\n"
        "  /Encoding 256 array def\n"
        "  0 1 255 {Encoding exch /.notdef put} for\n"
        "  Encoding (.) 0 get /xplotfontdot put\n"
        "  /CharacterDefs 3 dict def\n"
        "  CharacterDefs /.notdef {} put\n"
        "  CharacterDefs /xplotfontdot\n"
        "    { newpath\n"
        "	0 0 1 0 360 arc fill\n"
        "    } put\n"
        "  /BuildChar\n"
        "    { BuildCharDict begin\n"
        "	/char exch def\n"
        "	/fontdict exch def\n"
        "	/charname fontdict /Encoding get\n"
        "	char get def\n"
        "	/charproc fontdict /CharacterDefs\n"
        "        get charname get def\n"
        "	1 0 -1 -1 1 1 setcachedevice\n"
        "	gsave charproc grestore\n"
        "      end\n"
        "    } def\n"
        "end\n"
        "/XplotFont Xplotfontdict definefont pop\n"
        "% scale font according to theta\n"
        "/dotsetup { /dotfont /XplotFont findfont 4 theta scalefont def } def\n"
        "% DONE gdt mod\n"
        "%define procedures for each xplot primitive.\n"
        "% x y x --\n"
        "/x {moveto 8 8 rlineto -16 -16 rlineto\n"
        "    8 8 rmoveto\n"
        "    -8 8 rlineto 16 -16 rlineto} def\n"
        "% x y ?arrow --\n"
        "/darrow {moveto 8 theta 8 theta rmoveto -8 theta -8 theta rlineto\n"
        "         -8 theta 8 theta rlineto } def\n"
        "/uarrow {moveto -8 theta -8 theta rmoveto 8 theta 8 theta rlineto\n"
        "         8 theta -8 theta rlineto } def\n"
        "/rarrow {moveto -8 theta 8 theta rmoveto 8 theta -8 theta rlineto\n"
        "         -8 theta -8 theta rlineto } def\n"
        "/larrow {moveto 8 theta 8 theta rmoveto -8 theta -8 theta rlineto\n"
        "         8 theta -8 theta rlineto } def\n"
        "%x y x y line --\n"
        "/line {moveto lineto} def\n"
        "%x y dot --\n"
        "% begin gdt mod\n"
        "/dot { moveto dotfont setfont (.) show } def\n"
        "%/dot {stroke 8 theta 0 360 arc fill} def\n"
        "% end gdt mod\n"
        "%x y . --\n"
        "% begin gdt mod\n"
        "/. { moveto dotfont setfont (.) show } def\n"
        "%/. {stroke 8 theta 0 360 arc fill} def\n"
        "% end gdt mod\n"
        "%x y plus --\n"
        "/plus {moveto -8 theta 0 rmoveto 16 theta 0 rlineto\n"
        "       -8 theta -8 theta rmoveto 0 16 theta rlineto} def\n"
        "%x y + --\n"
        "/+ {moveto -8 theta 0 rmoveto 16 theta 0 rlineto\n"
        "       -8 theta -8 theta rmoveto 0 16 theta rlineto} def\n"
        "%x y box --\n"
        "/box {moveto -8 theta -8 theta rmoveto\n"
        "      16 theta 0 rlineto\n"
        "      0 16 theta rlineto\n"
        "      -16 theta 0 rlineto\n"
        "      0 -16 theta rlineto} def\n"
        "%x y diamond --\n"
        "/diamond { moveto 0 theta 24 theta rmoveto\n"
        "           -24 theta -24 theta rlineto\n"
        "            24 theta -24 theta rlineto\n"
        "            24 theta  24 theta rlineto\n"
        "           -24 theta  24 theta rlineto} def\n"
        "%x y ?tick --\n"
        "/utick {moveto 0 6 theta rlineto} def\n"
        "/dtick {moveto 0 -6 theta rlineto} def\n"
        "/ltick {moveto -6 theta 0 rlineto} def\n"
        "/rtick {moveto 6 theta 0 rlineto} def\n"
        "/htick {moveto -6 theta 0 rmoveto 12 theta 0 rlineto} def\n"
        "/vtick {moveto 0 -6 theta rmoveto 0 12 theta rlineto} def\n"
        "%Separate functions for each text position.\n"
        "%x y string ?text --\n"
        "/space 6 pt def\n"
        "% Set the font, figure out the width.\n"
        "% x y string tsetup string x width y\n"
        "/tsetup {lfont setfont dup stringwidth pop exch\n"
        "         4 1 roll exch} def\n"
        "%CENTER\n"
        "/ctext {tsetup lascent 2 div sub\n"
        "        3 1 roll 2 div sub exch\n"
        "% stack should now be string x y\n"
        "        moveto show} def\n"
        "%ABOVE\n"
        "/atext {tsetup space ldescent add add\n"
        "        3 1 roll 2 div sub exch moveto show} def\n"
        "%BELOW\n"
        "/btext {tsetup space lascent add sub\n"
        "        3 1 roll 2 div sub exch moveto show} def\n"
        "%TO_THE_LEFT\n"
        "/ltext {tsetup lascent 2 div sub\n"
        "        3 1 roll space add sub exch moveto show} def\n"
        "%TO_THE_RIGHT\n"
        "/rtext {tsetup lascent 2 div sub\n"
        "        3 1 roll pop space add exch moveto show} def\n", fp);
  {
    int i;

    fputs("/XPlotUseColor\n", fp);
    fputs("/Docolors where { pop true } { false } ifelse\n", fp);
    fputs("%product (Ghostscript) eq or\n", fp);
    fputs("def\n", fp);
    fputs("XPlotUseColor\n{\n", fp);
    for ( i = 0; i < NCOLORS; i++ )
      fprintf(fp, "/color%s { %s } def\n",
	      ColorNames[i], ColorPSrep[i]);
    fprintf(fp, "}\n{\n");
    for ( i = 0; i < NCOLORS; i++ )
      fprintf(fp, "/color%s { %s } def\n",
	      ColorNames[i], GrayPSrep[i]);

    fputs("}\nifelse\n", fp);
  }

  fputs("%% string title --\n", fp);
  fprintf(fp, "/title {tfont setfont dup stringwidth pop neg\n");
  fprintf(fp, "        %d add 2 div\n", ((int)rint(pspl.size.x)));
  fprintf(fp, "        %d\n", ((int)rint(pspl.size.y)));
  fprintf(fp, "        moveto show} def\n");

  fputs("%% string xlabel --\n", fp);
  fprintf(fp, "/xlabel {tfont setfont dup stringwidth pop neg\n");
  fprintf(fp, "         %d add\n", ((int)rint(pspl.size.x)));
  fprintf(fp, "         0 lascent ldescent add 3 mul sub\n");
  fprintf(fp, "         moveto show} def\n");

  fputs("%% string ylabel --\n", fp);
  fprintf(fp, "/ylabel {tfont setfont dup stringwidth pop neg\n");
  fprintf(fp, "         0 add 2 div\n");
  fprintf(fp, "         %d lascent ldescent add 1 mul add\n", ((int)rint(pspl.size.y)));
  fprintf(fp, "         moveto show} def\n");

  /* Final prelude:  Change scale, move & rotate origin for landscape
     printing & provide for a margin. */

  /* Orient for landscape printing, margin to lower left corner. */
  if (state == PRINTING) {
    fputs("-90 rotate -11 inch 0 inch translate\n", fp);
  } else {
    fputs("\n/notintex { currentdict userdict eq } def\n"
          "notintex { 1.5 inch 5.0 inch translate } if\n", fp);
  }
  /* Move origin to create left & bottom margins. */
  fprintf(fp, "%g inch %g inch translate\n", lmargin, bmargin);
  /* Relatively thick lines for axes & ticks. */
  fputs("4 theta setlinewidth newpath\n", fp);

  fputs("\n% The actual drawing:\n\n", fp);

  /*
   * Now do all the drawing commands.
   */

#define xtoPSx(xxxx) ((int) rint(xxxx))
#define ytoPSy(yyyy) ((int) rint(pspl.size.y - yyyy))

  finished_decoration = output_decoration = FALSE;
  counter = 0;
  currentcolor = 0;		/* black */
  /* loop twice - once for decoration, once for data */
  for (c = pspl.commands; c != NULL; 
       c = c->next
       ? c->next
       : (finished_decoration == FALSE
	  ? (finished_decoration = TRUE, pspl.commands)
	  : NULL))
    {
    if ( finished_decoration && output_decoration == FALSE )  {
      /* Thinner lines for the actual drawing. */
      fputs("stroke\n", fp);
      fprintf(fp, "/theta {%d mul} def\n", ( (state == PRINTING) ? PER_INCH/300 : PER_INCH/600));
      fputs("2 theta setlinewidth\n", fp);
      fputs("dotsetup\n", fp);	/* gdt */
      /* Set clipping region so that we don't draw past the axes. */
      fprintf(fp, "0 0 moveto %d 0 lineto %d %d lineto\n",
	      ((int)rint(pspl.size.x)),
	      ((int)rint(pspl.size.x)), ((int)rint(pspl.size.y)));
      fprintf(fp, "0 %d lineto 0 0 lineto clip newpath\n",
	      ((int)rint(pspl.size.y)));
      output_decoration = TRUE;
    }

    if ( finished_decoration == TRUE
	&& (c->decoration
	    || c->type == TITLE
	    || c->type == XLABEL
	    || c->type == YLABEL
	    ) )
      continue;
    if ( finished_decoration == FALSE
	&& ! (c->decoration
	      || c->type == TITLE
	      || c->type == XLABEL
	      || c->type == YLABEL
	      ) )
      continue;


    if (c->mapped)  {
      if ( !option_mono && c->color != currentcolor ) {
          if ( counter > 0 ) {
            counter = 0;
	    fprintf(fp, "stroke\n");
          }
	  currentcolor = c->color;
	  if ( currentcolor >= 0 && currentcolor < NCOLORS )
	    fprintf(fp, "color%s\n", ColorNames[currentcolor]);
	  else
	    fprintf(fp, "colorwhite\n");
      }

#ifndef WINDOW_COORDS_IN_COMMAND_STRUCT
      a.x = map_coord(pspl.x_type, pspl_x_left, pspl_x_right, pspl.size.x,
		       c->xa);
      a.y = (pspl.size.y - 1) -
	map_coord(pspl.y_type, pspl_y_bottom, pspl_y_top, pspl.size.y,
		  c->ya);
      
      b.x = map_coord(pspl.x_type, pspl_x_left, pspl_x_right, pspl.size.x,
		       c->xb);
      b.y = (pspl.size.y - 1) -
	map_coord(pspl.y_type, pspl_y_bottom, pspl_y_top, pspl.size.y,
		  c->yb);

      a = tomain(&pspl, a);
      b = tomain(&pspl, b);
#else
      a = c->a;
      b = c->b;
#endif

      switch (c->type)  {
      case X:
	fprintf(fp, "%d %d x\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case DOT:
	fprintf(fp, "%d %d dot\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case PLUS:
	fprintf(fp, "%d %d plus\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case BOX:
	fprintf(fp, "%d %d box\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case DIAMOND:
	fprintf(fp, "%d %d diamond\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case UTICK:
	fprintf(fp, "%d %d utick\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case DTICK:
	fprintf(fp, "%d %d dtick\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case LTICK:
	fprintf(fp, "%d %d ltick\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case RTICK:
	fprintf(fp, "%d %d rtick\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case HTICK:
	fprintf(fp, "%d %d htick\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case VTICK:
	fprintf(fp, "%d %d vtick\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case UARROW:
	fprintf(fp, "%d %d uarrow\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case DARROW:
	fprintf(fp, "%d %d darrow\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case LARROW:
	fprintf(fp, "%d %d larrow\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case RARROW:
	fprintf(fp, "%d %d rarrow\n", xtoPSx(a.x), ytoPSy(a.y));
	break;
      case DLINE:
	fprintf(fp, "%d %d +\n", xtoPSx(a.x), ytoPSy(a.y));
	fprintf(fp, "%d %d +\n", xtoPSx(b.x), ytoPSy(b.y));
	counter += 2;
	/* fall through and draw the line */
      case LINE:
	fprintf(fp, "%d %d %d %d line\n", xtoPSx(a.x), ytoPSy(a.y),
		xtoPSx(b.x), ytoPSy(b.y));
	break;
      case TEXT:
	switch (c->position)  {
	case CENTERED:
	  fprintf(fp, "%d %d (%s) ctext\n", xtoPSx(a.x), ytoPSy(a.y),
		  esc_paren(c->text));
	  break;
	case ABOVE:
	  fprintf(fp, "%d %d (%s) atext\n", xtoPSx(a.x), ytoPSy(a.y),
		  esc_paren(c->text));
	  break;
	case BELOW:
	  fprintf(fp, "%d %d (%s) btext\n", xtoPSx(a.x), ytoPSy(a.y),
		  esc_paren(c->text));
	  break;
	case TO_THE_LEFT:
	  fprintf(fp, "%d %d (%s) ltext\n", xtoPSx(a.x), ytoPSy(a.y),
		  esc_paren(c->text));
	  break;
	case TO_THE_RIGHT:
	  fprintf(fp, "%d %d (%s) rtext\n", xtoPSx(a.x), ytoPSy(a.y),
		  esc_paren(c->text));
	  break;
	}
	break;
      case TITLE:
	fprintf(fp, "(%s) title\n", esc_paren(c->text));
	break;
      case XLABEL:
	fprintf(fp, "(%s) xlabel\n", esc_paren(c->text));
	break;
      case YLABEL:
	fprintf(fp, "(%s) ylabel\n", esc_paren(c->text));
	break;
      case INVISIBLE:
	break;
      }
      if (++counter > 50) {
	counter = 0;
	fprintf(fp, "stroke\n");
      }
    }
  }
  
  fputs("stroke ", fp);
  if (state == PRINTING)
    fputs("showpage ", fp);
  else
    fputs("notintex { showpage } if\n", fp);
  fputs("grestore\n", fp);
  (void) fflush(fp);
  
  /* return our list of commands to the caller... */
  pl->commands = pspl.commands;
  
}    


/* Take a plotter, and open a file with a name that is either the
   title of the plot or "xplot.PS" with a number appended.  */

FILE *
make_name_open_file(struct plotter *pl)
{
  command *c;
  char *name = NULL, *versionp;
  static int version = 0;
  FILE *fp;

  for (c = pl->commands; c != NULL; c = c->next)  {
    if (c->type == TITLE && name == NULL)  {
      /* Allow space for the number. */
      name = malloc((unsigned)strlen(c->text) + 15);
      if (name == 0) fatalerror("malloc returned null");
#if 0
      (void) strcpy(name, c->text);
#else
      {
	char *from = c->text;
	char *to = name;
	char c;
	
	while (*from != '\0') {
	  switch (c = *from++) {
	  case '/':
	    *to++ = '_';
	    break;
	  default:
	    *to++ = c;
	  }
	}
	*to = '\0';
      }
#endif

    }
  }

  /* If no title found, just call it "xplot" */
  if (!name)  {
    name = malloc(sizeof("xplot") + 15);
    if (name == 0) fatalerror("malloc returned null");
    (void) strcpy(name, "xplot");
  }

  (void) strcat(name, ".PS.");
  versionp = name + strlen(name);
  do  {
    (void) sprintf(versionp, "%d", version++);
  } while (access(name, F_OK) == 0);

  fp = fopen(name, "w");
  free(name);
  return fp;
}


#ifdef TCPTRACE
#undef malloc
void *
MALLOC(int nbytes)
{
    char *ptr;

    if ((ptr = malloc(nbytes)) == NULL) {
	perror("malloc() virtual memory allocation");
	fprintf(stderr,"You probably need more swap space to run this file\n");
	fprintf(stderr,"Exiting...\n");
	exit(1);
    }

    return(ptr);
}
#endif /* TCPTRACE */
