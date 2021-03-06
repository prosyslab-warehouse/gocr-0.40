/*
This is a Optical-Character-Recognition program
Copyright (C) 2000  Joerg Schulenburg

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 see README for EMAIL-address

  sometimes I have written comments in german language, sorry for that

 - look for ??? for preliminary code
*/

/* General headerfile with gocr-definitions */

#ifndef __GOCR_H__
#define __GOCR_H__

#include "pnm.h"
#include "unicode.h"
#include "list.h"
#include <stddef.h>
#ifdef HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif

/*
 *  wchar_t should always exist (ANSI), but WCHAR.H is sometimes missing
 *  USE_UNICODE should be removed or replaced by HAVE_WCHAR_H in future
 */
#ifdef HAVE_WCHAR_H
#define USE_UNICODE 1
#endif

/* extern "C"{ */
// ------------------------ feature extraction -----------------
#define AT 7	// mark
#define M1 1	// mark
enum direction {
  UP=1, DO, RI, LE
};
typedef enum direction DIRECTION;
#define ST 7    // stop
// -------------------------------------------------------------
// detect maximas in of line overlapps (return in %) and line koord
#define HOR 1    // horizontal
#define VER 2    // vertikal
#define RIS 3    // rising=steigend
#define FAL 4    // falling=fallend

#define MAXlines 1024

/* ToDo: if we have a tree instead of a list, a line could be a node object */
struct tlines {
    int num;
    /* ToDo: add dx, so we have a vector (dx,dy) (js) */
    int dy;		/* offset for straight/skew lines dy(x+px) */
    int m1[MAXlines],   /* start of line = upper bound of 'A' */
        m2[MAXlines],   /* upper bound of 'e' */
        m3[MAXlines],	/* lower bound of 'e' = baseline */
        m4[MAXlines];	/* stop of line = lower bound of 'q' */
    int x0[MAXlines],
        x1[MAXlines];		/* left and right border */
};

#define NumAlt 10 // maximal number of alternative chars (table length)
struct box {	// this structure should contain all pixel infos of a letter
    int x0,x1,y0,y1,x,y,dots;	// xmin,xmax,ymin,ymax,reference-pixel,i-dots
    wchar_t c,ac;	// detected (alternate) char
    wchar_t modifier;	// default=0, see compose() in unicode.c
    int num;		// same nummer = same char
    int line;		// line number (points to struct tlines lines)
    int m1,m2,m3,m4;	// m2 = upper boundary, m3 = baseline
    // planed: sizeof hole_1, hole_2, certainty (run1=100%,run2=90%,etc.)
    pix *p;		// pointer to pixmap (v0.2.5)
    /* tac, wac is used together with setac() to manage very similar chars */
    int num_ac;         // length of table (alternative chars), default=0
    wchar_t tac[NumAlt]; // alternative chars, only used by setac(),getac()
    int     wac[NumAlt]; // weight of alternative chars
    char    *obj;        // pointer to text-object ... (default=NULL)
                         //  ... (melted chars, barcode, picture coords, ...)
                         //  must be freed before box is freed!
                         //  do _not_ copy obj !!!!!
};
typedef struct box Box;

#define outbounds(p, tx, ty)   	    (tx < 0 || ty < 0 || tx >= (p)->x || ty >= (p)->y)

typedef struct path {
  int start;			// color at the beginning of the path
				// (0=white, 1=black)
  int *x;			// x coordinates of transitions
  int *y;			// y coordinates of transitions
  int num;			// current number of entries in x or y
  int max;			// maximum number of entries in x or y
				// (if more values need to be stored,
				// the arrays are enlarged)
} path_t;

/* job_t contains all information needed for an OCR task */
typedef struct job_s {
  struct { /* source data */
    char *fname; /* input filename; default value: "-" */
    pix p; /* source pixel data */
           /* pixelmap 8bit gray */
  } src;
  struct { /* temporary stuff, e.g. buffers */
#ifdef HAVE_GETTIMEOFDAY
    struct timeval init_time; /* starting time of this job */
#endif
    pix ppo; /* pixmap for visual debugging output */

    /* sometimes recognition function is called again and again, if result was 0
       n_run tells the pixel function to return alternative results */
    int n_run; /* num of run, if run_2 critical pattern get other results */
               /* used for 2nd try, pixel uses slower filter function etc. */
    List dblist; /* FIXME jb: desc */
  } tmp;
  struct { /* results */
    List boxlist; /* FIXME jb: desc */
    List linelist; /* FIXME jb: desc */
    
    struct tlines lines; /* used to access to line-data */
                         /* here the positions (frames) of lines are */
                         /* stored for further use */

    int avX,avY; /* average X,Y (avX=sumX/numC) */
    int sumX,sumY,numC; /* sum of all X,Y; num chars */
  } res;
  struct { /* configuration */
    int cs; /* FIXME jb: grey level 0<160<=255; default value: 0 */
            /* critical value (pixel<cs => black pixel) */

    int spc; /* spacewidth/dots (0 = autodetect); default value: 0 */
    int mode; /* operation modes; default value: 0 */
            /* operation mode (see --help) */
    int dust_size; /* dust size; default value: 10 */
    int only_numbers; /* numbers only; default value: 0 */
    int verbose; /* verbose mode; default value: 0 */ 
                 /* verbose option (see --help) */
    FORMAT out_format; /* output format; default value: ISO8859_1*/
    char *lc; /* list of chars (_ = not recognized chars); default value: "_" */
    char *db_path; /* pathname for database; default value: NULL */
    char *cfilter; /* char filter; default value: NULL */
  } cfg;
} job_t;

/* initialze job structure */
void job_init(job_t *job);

/* free job structure */
void job_free(job_t *job);

//FIXME jb: remove JOB;
extern job_t *JOB;

// kalkulate the overlapp of the line (0-1) with black points 
// by rekursiv bisection 
// (evl. Fehlertoleranz mit pixel in Umgebung dx,dy suchen) (umschaltbar) ???
// MidPoint Line Algorithm (Bresenham) Foley: ComputerGraphics better?

// gerade y=dy/dx*x+b, implizit d=F(x,y)=dy*x-dx*y+b*dx=0 
// incrementell y(i+1)=m*(x(i)+1)+b, F(x+1,y+1)=f(F(x,y))
int get_line(int x0, int y0, int x1, int y1, pix *p, int cs, int ret);
int get_line2(int x0, int y0, int x1, int y1, pix *p, int cs, int ret);

// look for white 0x02 or black 0x01 dots (0x03 = white+black)
char get_bw(int x0, int x1, int y0, int y1,
             pix *p, int cs,int mask);

// look for black crossing a line x0,y0,x1,y1
// follow line and count crossings ([white]-black-transitions)
int num_cross(int x0, int x1, int y0, int y1,
               pix *p,  int cs);

// memory allocation with error checking
void *xrealloc(void *ptr, size_t size);

// follow a line x0,y0,x1,y1 recording locations of transitions,
// return count of transitions
int follow_path(int x0, int x1, int y0, int y1, pix *p,  int cs, path_t *path);

// -------------------------------------------------------------
// mark edge-points
//  - first move forward until b/w-edge
//  - more than 2 pixel?
//  - loop around
//    - if forward    pixel : go up, rotate right
//    - if forward no pixel : rotate left
//  - stop if found first 2 pixel in same order
// mit an rechter-Wand-entlang-gehen strategie
// --------------------------------------------------------------
// turmite game: inp: start-x,y, regel r_black=UP,r_white=RIght until border
//               out: last-position
// Zaehle dabei, Schritte,Sackgassen,xmax,ymax,ro-,ru-,lo-,lu-Ecken
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// is this the right place for declaration?
void turmite(pix *p, int *x, int *y, 
             int x0, int x1, int y0, int y1, int cs, int rw, int rb);

// test if points are connected via t-pixel (rekursiv!)
int joined(pix *p, int x0, int y0, int x1, int y1, int cs);

// move from x,y to direction r until pixel or l steps
// return number of steps
int loop(pix *p, int x, int y, int l, int cs, int col, DIRECTION r);

#define MAX_HOLES 3
typedef struct list_holes { 
  int num;    /* numbers of holes, initialize with 0 */ 
  struct hole_s {
    int size,x,y,x0,y0,x1,y1; /*  size, start point, outer rectangle */
  } hole[MAX_HOLES];
} holes_t;
          
// look for white holes surrounded by black points
// at moment white point with black in all four directions
int num_hole(int x0, int x1, int y0, int y1, pix *p, int cs, holes_t *holes);

// count for black nonconnected objects --- used for i,auml,ouml,etc.
int num_obj(int x0, int x1, int y0, int y1, pix  *p, int cs);

int distance(   pix *p1, struct box *box1,	// box-frame
		pix *p2, struct box *box2, int cs);

int pixel(pix *p, int x, int y);
// call the OCR engine ;)
//char whatletter(struct box *box1,int cs);

/* declared in pgm2asc.c */
int marked(pix *p, int  x, int  y);
void put(pix * p, int x, int y, int ia, int io);

/* } */ /* extern C */
#endif /* __GOCR_H__ */
