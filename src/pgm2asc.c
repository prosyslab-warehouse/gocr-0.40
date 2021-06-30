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
 - space: avX=22 11-13 (empirical estimated)
          avX=16  5-7
          avX= 7  5-6
         
 ToDo: - add filter (r/s mismatch) g300c1
       - better get_line2 function (problems on high resolution)
       - write parallelizable code!
       - learnmode (optimize filter)
       - use ispell for final control or if unsure
       - better line scanning (if not even)
       - step 5: same chars differ? => expert mode
       - chars dx>dy and above 50% hor-crossing > 4 is char-group ?
       - detect color of chars and background
       - rotation of chars in the pixmap to avoid y-corrections
       - better word space calculation (look at the examples)
          (distance: left-left, middle-middle, left-right, thickness of e *0.75)

   GLOBAL DATA (mostly structures)
   - pix   : image - one byte per pixel  bits0-2=working
   - lines : rows of the text (points to pix)
   - box   : list of bounding box for character 
   - obj   : objects (lines, splines, etc. building a character)
 */


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include "amiga.h"
#include "list.h"
#include "pgm2asc.h"
#include "pcx.h"        /* needed for writebmp (removed later) */
/* ocr1 is the test-engine - remember: this is development version */
#include "ocr1.h"
/* first engine */
#include "ocr0.h"
#include "otsu.h"
#include "barcode.h"

#include "gocr.h"

/* wew: will be exceeded by capitals at 1200dpi */
#define MaxBox (100*200)	// largest possible letter (buffersize)
#define MAX(a,b)			((a) >= (b) ? (a) : (b))

/* if the system does not know about wchar.h, define functions here */
#ifndef HAVE_WCHAR_H
/* typedef unsigned wchar_t; */
/* Find the first occurrence of WC in WCS.  */
wchar_t *wcschr (wchar_t *wcs, wchar_t wc) {
  int i; for(i=0;wcs[i];i++) if (wcs[i]==wc) return wcs+i; return NULL;
}
wchar_t *wcscpy (wchar_t *dest, const wchar_t *src) {
  int i; for(i=0;src[i];i++) dest[i]=src[i]; dest[i]=0; return dest;
}
size_t wcslen (const wchar_t *s){
  size_t i; for(i=0;s[i];i++); return i;
}
#endif
#ifndef HAVE_WCSDUP
wchar_t * wcsdup (const wchar_t *WS) {	/* its a gnu extension */
  wchar_t *copy;
  copy = (wchar_t *) malloc((wcslen(WS)+1)*sizeof(wchar_t));
  if (!copy)return NULL;
  wcscpy(copy, WS);
  return copy;
}   
#endif

// ------------------------ feature extraction -----------------
// -------------------------------------------------------------
// detect maximas in of line overlaps (return in %) and line coordinates
// this is for future use
#define HOR 1    // horizontal
#define VER 2    // vertical
#define RIS 3    // rising=steigend
#define FAL 4    // falling=fallend

/* exchange two variables */
static void swap(int *a, int *b) { 
  int c = *a;
  *a = *b;
  *b = c;
}

// calculate the overlapping of the line (0-1) with black points 
// by recursive bisection 
// line: y=dy/dx*x+b, implicit form: d=F(x,y)=dy*x-dx*y+b*dx=0 
// incremental y(i+1)=m*(x(i)+1)+b, F(x+1,y+1)=f(F(x,y))
// ret & 1 => inverse pixel!
// d=2*F(x,y) integer numbers
int get_line(int x0, int y0, int x1, int y1, pix *p, int cs, int ret){
   int dx,dy,incrE,incrNE,d,x,y,r0,r1,ty,tx,
       *px,*py,*pdx,*pdy,*ptx,*pty,*px1;
   dx=abs(x1-x0); tx=((x1>x0)?1:-1);    // tx=x-spiegelung (new)  
   dy=abs(y1-y0); ty=((y1>y0)?1:-1);	// ty=y-spiegelung (new)
   // rotate coordinate system if dy>dx
/*bbg: can be faster if instead of pointers we use the variables and swaps? */
/*js: Do not know, I am happy that the current code is working and is small */
   if(dx>dy){ pdx=&dx;pdy=&dy;px=&x;py=&y;ptx=&tx;pty=&ty;px1=&x1; }
   else     { pdx=&dy;pdy=&dx;px=&y;py=&x;ptx=&ty;pty=&tx;px1=&y1; }
   if( *ptx<0 ){ swap(&x0,&x1);swap(&y0,&y1);tx=-tx;ty=-ty; }
   d=((*pdy)<<1)-(*pdx); incrE=(*pdy)<<1; incrNE=((*pdy)-(*pdx))<<1;  
   x=x0; y=y0; r0=r1=0; /* dd=tolerance (store max drift) */
   while( (*px)<=(*px1) ){ 
     if( ((pixel(p,x,y)<cs)?1:0)^(ret&1) ) r0++; else r1++;
     (*px)++; if( d<=0 ){ d+=incrE; } else { d+=incrNE; (*py)+=(*pty); }
   }
   return (r0*(ret&~1))/(r0+r1); // ret==100 => percentage %
}

// this function should detect whether a direct connection between points
//   exists or not, not finally implemented
// ret & 1 => inverse pixel!
// d=2*F(x,y) integer numbers, ideal line: ,I pixel: I@
//   ..@  @@@  .@.  ...,@2@. +1..+3 floodfill around line ???
//   ..@  .@@  .@.  ...,.@@@ +2..+4 <= that's not implemented yet
//   ..@  ..@  .@.  ...,.@@@ +2..+4
//   @.@  @..  .@.  ...,@@@. +1..+3
//   @.@  @@.  .@.  ...I@@@.  0..+3
//   @@@  @@@  .@.  ..@1@@..  0..+2
//   90%   0%  100%   90%     r1-r2
// I am not satisfied with it
int get_line2(int x0, int y0, int x1, int y1, pix *p, int cs, int ret){
   int dx,dy,incrE,incrNE,d,x,y,r0,r1,ty,tx,q,ddy,rx,ry,
       *px,*py,*pdx,*pdy,*ptx,*pty,*px1;
   dx=abs(x1-x0); tx=((x1>x0)?1:-1);    // tx=x-spiegelung (new)  
   dy=abs(y1-y0); ty=((y1>y0)?1:-1);	// ty=y-spiegelung (new)
   // rotate coordinate system if dy>dx
   if(dx>dy){ pdx=&dx;pdy=&dy;px=&x;py=&y;ptx=&tx;pty=&ty;px1=&x1;rx=1;ry=0; }
   else     { pdx=&dy;pdy=&dx;px=&y;py=&x;ptx=&ty;pty=&tx;px1=&y1;rx=0;ry=1; }
   if( *ptx<0 ){ swap(&x0,&x1);swap(&y0,&y1);tx=-tx;ty=-ty; }
   d=((*pdy)<<1)-(*pdx); incrE=(*pdy)<<1; incrNE=((*pdy)-(*pdx))<<1;  
   x=x0; y=y0; r0=r1=0; ddy=3; // tolerance = bit 1 + bit 0 = left+right
   // int t=(*pdx)/16,tl,tr;  // tolerance, left-,right delimiter
   while( (*px)<=(*px1) ){  // not finaly implemented
     q=((pixel(p,x,y)<cs)?1:0)^(ret&1);
     if ( !q ){		// tolerance one pixel perpenticular to the line
                        // what about 2 or more pixels tolerance???
       ddy&=(~1)|(((pixel(p,x+ry,y+rx)<cs)?1:0)^(ret&1));
       ddy&=(~2)|(((pixel(p,x-ry,y-rx)<cs)?1:0)^(ret&1))*2;
     } else ddy=3;
     if( ddy ) r0++; else r1++;
     (*px)++; if( d<=0 ){ d+=incrE; } else { d+=incrNE; (*py)+=(*pty); }
   }
   return (r0*(ret&~1))/(r0+r1); // ret==100 => percentage %
}

/* Look for dots in the rectangular region x0 <= x <= x1 and y0 <= y
 <= y1 in pixmap p.  The two low order bits in mask indicate the color
 of dots to look for: If mask==1 then look for black dots (where a
 pixel value less than cs is considered black).  If mask==2 then look
 for white dots.  If mask==3 then look for both black and white dots.
 If the dots are found, the corresponding bits are set in the returned
 value.  Heavily used by the engine ocr0*.cc */
char get_bw(int x0, int x1, int y0, int y1, pix * p, int cs, int mask) {
  char rc = 0;			// later with error < 2% (1 dot)
  int x, y;

  if (x0 < 0)        x0 = 0;
  if (x1 >= p->x)    x1 = p->x - 1;
  if (y0 < 0)        y0 = 0;
  if (y1 >= p->y)    y1 = p->y - 1;

  for ( y = y0; y <= y1; y++)
    for ( x = x0; x <= x1; x++) {
      rc |= ((pixel(p, x, y) < cs) ? 1 : 2);	// break if rc==3
      if ((rc & mask) == mask)
	return mask;		// break loop
    }
  return (rc & mask);
}

/* more general Mar2000 (x0,x1,y0,y1 instead of x0,y0,x1,y1! (history))
 * look for black crossings throw a line from x0,y0 to x1,y1 and count them
 * follow line and count crossings ([white]-black-transitions)
 *  ex: horizontal num_cross of 'm' would return 3 */
int num_cross(int x0, int x1, int y0, int y1, pix *p, int cs) {
  int rc = 0, col = 0, k, x, y, i, d;	// rc=crossings  col=0=white
  int dx = x1 - x0, dy = y1 - y0;

  d = MAX(abs(dx), abs(dy));
  for (i = 0, x = x0, y = y0; i <= d; i++) {
    if (d) {
      x = x0 + i * dx / d;
      y = y0 + i * dy / d;
    }
    k = ((pixel(p, x, y) < cs) ? 1 : 0);	// 0=white 1=black
    if (col == 0 && k == 1)
      rc++;
    col = k;
  }
  return rc;
}

/* check if test matches pattern
 *   possible pattern: "a-zA-Z0-9+\-\\"
 */
int my_strchr( char *pattern, char cc ) {
  char *s1;
  if (pattern==(char *)NULL) return 0;
  
  s1=strchr(pattern,cc);
  switch (cc) {
    case '-':
    case '\\':
      if ((!s1) || s1-pattern<1 || *(s1-1)!='\\') return 0;
                                             else return 1;
    default:
      if (s1) return 1; /* cc simply matches */
      s1=pattern+1;
      while (s1) {
        if ((!s1[0]) || (!s1[1])) return 0; /* end of string */
        if (*(s1-1)!='\\' && *(s1-1)<=cc && *(s1+1)>=cc) return 1;
        s1=strchr(s1+1,'-');  /* look for next '-' */
      }
  }
  return 0;
}

/* set alternate chars and its weight, called from the engine
    if a char is recognized to (weight) percent
   can be used for filtering (only numbers etc)
   often usefull if Il1 are looking very similar
   should this function stay in box.c ???
   weight is between 0 and 100 in percent, 100 means absolutely sure
   - not final, not time critical (js)
 */
int setac(struct box *b, wchar_t ac, int weight){
  int i,j;
  if (b->num_ac > NumAlt || b->num_ac<0) {
    fprintf(stderr,"\n#DEBUG: There is something wrong with setac()!");
    b->num_ac=0;
  }
  if (ac==0 || ac==UNKNOWN) {
    fprintf(stderr,"\n#DEBUG: setac(%s) makes no sence!",decode(ac,ASCII));
    return 0;
  }
  /* char filter (ex: only numbers) ToDo: cfilter as UTF8 or wchar? */
  if (JOB->cfg.cfilter) {
    /* do not accept chars which are not in the cfilter string */
    /* if ( ac>255 || !strchr(JOB->cfg.cfilter,(char)ac) ) return 0; */
    if ( ac>255 || !my_strchr(JOB->cfg.cfilter,(char)ac) ) return 0;
  }
  /* not sure that this is the right place, but where else? */
  if (b->modifier != SPACE  &&  b->modifier != 0)
      ac = compose(ac, b->modifier);
    
  /* remove same entries from table */
  for (i=0;i<b->num_ac;i++) if (ac==b->tac[i]) break;
  if (i<b->num_ac){
   if (weight<=b->wac[i]) return 0;
   if (b->num_ac>0) b->num_ac--; /* shrink table */
   for (j=i;j<b->num_ac-1;j++){  /* shift lower entries */
     b->tac[j]=b->tac[j+1];
     b->wac[j]=b->wac[j+1];
   }
  }
  /* sorting it to the table */
  for (i=0;i<b->num_ac;i++) if (weight>b->wac[i]) break;
  if (b->num_ac<NumAlt-1) b->num_ac++; /* enlarge table */
  for (j=b->num_ac-1;j>i;j--){  /* shift lower entries */
    b->tac[j]=b->tac[j-1];
    b->wac[j]=b->wac[j-1];
  }
  if (i<b->num_ac) { /* insert new entry */
    b->tac[i]=ac;
    b->wac[i]=weight;
  }
  if (i==0) b->c=ac; // store best result to b->c

  return 0;
}

/* test if ac in wac-table
   usefull for contextcorrection and box-splitting
   return 0 if not found
   return wac if found (wac>0)
 */
int testac(struct box *b, wchar_t ac){
  int i;
  if (b->num_ac > NumAlt || b->num_ac<0) {
    fprintf(stderr,"\n#DEBUG: There is something wrong with testac()!");
    b->num_ac=0;
  }
  /* search entries in table */
  for (i=0;i<b->num_ac;i++) if (ac==b->tac[i]) return b->wac[i];
  return 0;
}


/* look for edges: follow a line from x0,y0 to x1,y1, record the
 * location of each transition, and return their number.
 * ex: horizontal num_cross of 'm' would return 6 */
int follow_path(int x0, int x1, int y0, int y1, pix *p, int cs, path_t *path) {
  int rc = 0, prev, x, y, i, d, color; // rc=crossings  col=0=white
  int dx = x1 - x0, dy = y1 - y0;

  d = MAX(abs(dx), abs(dy));
  prev = pixel(p, x0, y0) < cs;	// 0=white 1=black
  path->start = prev;
  for (i = 1, x = x0, y = y0; i <= d; i++) {
    if (d) {
      x = x0 + i * dx / d;
      y = y0 + i * dy / d;
    }
    color = pixel(p, x, y) < cs; // 0=white 1=black
    if (color != prev){
      if (rc>=path->max){
	int n=path->max*2+10;
	path->x = xrealloc(path->x, n*sizeof(int));
	path->y = xrealloc(path->y, n*sizeof(int));
	path->max = n;
      }
      path->x[rc]=x;
      path->y[rc]=y;
      rc++;
    }      
    prev = color;
  }
  path->num=rc;
  return rc;
}

void *xrealloc(void *ptr, size_t size){
  void *p;
  p = realloc(ptr, size);
  if (size>0 && (!p)){
    fprintf(stderr, "insufficient memory");
    exit(1);
  }
  return p;
}

/*
 *  -------------------------------------------------------------
 *  mark edge-points
 *   - first move forward until b/w-edge
 *   - more than 2 pixel?
 *   - loop around
 *     - if forward    pixel : go up, rotate right
 *     - if forward no pixel : rotate left
 *   - stop if found first 2 pixel in same order
 *  go_along_the_right_wall strategy is very similar and used otherwhere
 *  --------------------------------------------------------------
 *  turmite game: inp: start-x,y, regel r_black=UP,r_white=RIght until border
 * 	       out: last-position
 * 
 *  could be used to extract more features:
 *   by counting stepps, dead-end streets ,xmax,ymax,ro-,ru-,lo-,lu-edges
 * 
 *   use this little animal to find features, I first was happy about it
 *    but now I prefer the loop() function 
 */

void turmite(pix *p, int *x, int *y,
	     int x0, int x1, int y0, int y1, int cs, int rw, int rb) {
  int r;
  if (outbounds(p, x0, y0))	// out of pixmap
    return;
  while (*x >= x0 && *y >= y0 && *x <= x1 && *y <= y1) {
    r = ((pixel(p, *x, *y) < cs) ? rb : rw);	// select rule 
    switch (r) {
      case UP: (*y)--; break;
      case DO: (*y)++; break;
      case RI: (*x)++; break;
      case LE: (*x)--; break;
      case ST:       break;
      default:       assert(0);
    }
    if( r==ST ) break;	/* leave the while-loop */
  }
}

/* search a way from p0 to p1 without crossing pixels of type t
 *  only two directions, useful to test if there is a gap 's'
 * labyrinth algorithm - do you know a faster way? */
int joined(pix *p, int x0, int y0, int x1, int y1, int cs){
  int t,r,x,y,dx,dy,xa,ya,xb,yb;
  x=x0;y=y0;dx=1;dy=0;
  if(x1>x0){xa=x0;xb=x1;} else {xb=x0;xa=x1;}
  if(y1>y0){ya=y0;yb=y1;} else {yb=y0;ya=y1;}
  t=((pixel(p,x,y)<cs)?1:0);
  for(;;){
    if( t==((pixel(p,x+dy,y-dx)<cs)?1:0)	// right free?
     && x+dy>=xa && x+dy<=xb && y-dx>=ya && y-dx<=yb) // wall
         { r=dy;dy=-dx;dx=r;x+=dx;y+=dy; } // rotate right and step forward
    else { r=dx;dx=-dy;dy=r; } // rotate left
    // fprintf(stderr," path xy %d-%d %d-%d %d %d  %d %d\n",xa,xb,ya,yb,x,y,dx,dy);
    if( x==x1 && y==y1 ) return 1;
    if( x==x0 && y==y0 && dx==1) return 0;
  }
  // return 0; // endless loop ?
}

/* move from x,y to direction r until pixel of color col is found
 *   or maximum of l steps
 * return the number of steps done */
int loop(pix *p,int x,int y,int l,int cs,int col, DIRECTION r){ 
  int i=0;
  if(x>=0 && y>=0 && x<p->x && y<p->y){
    switch (r) {
    case UP:
      for( ;i<l && y>=0;i++,y--)
	if( (pixel(p,x,y)<cs)^col )
	  break;
    case DO:
      for( ;i<l && y<p->y;i++,y++)
	if( (pixel(p,x,y)<cs)^col )
	  break;
    case LE:
      for( ;i<l && x>=0;i++,x--)
	if( (pixel(p,x,y)<cs)^col )
	  break;
    case RI:
      for( ;i<l && x<p->x;i++,x++)
	if( (pixel(p,x,y)<cs)^col )
	  break;
    default:;
    }
  }
  return i;
}

/* Given a point, frames a rectangle containing all points of the same
 * color surrounding it, and mark these points.
 *
 * looking for better algo: go horizontally and look for upper/lower non_marked_pixel/nopixel
 * use lowest three bits for mark
 *   - recursive version removed! AmigaOS has no Stack-OVL-Event
 * run around the chape using laby-robot
 * bad changes can lead to endless loop!
 *  - this is not absolutely sure but mostly works well
 *  diag - 0: only pi/2 direction, 1: pi/4 directions (diagonal)
 *  mark - 3 bit marker, mark each valid pixel with it
 */
int frame_nn(pix *p, int  x,  int  y,
             int *x0, int *x1, int *y0, int *y1,	// enlarge frame
             int cs, int mark,int diag){
#if 1 /* flood-fill to detect black objects, simple and faster? */
  /* ToDo: still causes an segfault! */
  int rc = 0, dx, col, maxstack=0; static int overflow=0;
  int bmax=1024, blen=0, *buf;  /* buffer as replacement for recursion stack */

  /* check bounds */
  if (outbounds(p, x, y))  return 0;
  /* check if already marked (with mark since v0.4) */
  if ((marked(p,x,y)&mark)==mark) return 0;

  col = ((pixel(p, x, y) < cs) ? 0 : 1);
  buf=(int *)malloc(bmax*sizeof(int)*2);
  if (!buf) { fprintf(stderr,"malloc failed (frame_nn)\n");return 0;}
  buf[0]=x;
  buf[1]=y;
  blen=1;

  g_debug(fprintf(stderr,"\nframe_nn   x=%4d y=%4d",x,y);)
  for ( ; blen ; ) {
    /* max stack depth is complexity of the object */
    if (blen>maxstack) maxstack=blen;
    blen--;             /* reduce the stack */
    x=buf[blen*2+0];
    y=buf[blen*2+1];
    if (y < *y0) *y0 = y;
    if (y > *y1) *y1 = y;
    /* first go to leftmost pixel */
    for ( ; x>0 && (col == ((pixel(p, x-1, y) < cs) ? 0 : 1)) ; x--);
    if ((marked(p,x,y)&mark)==mark) continue; /* already scanned */
    for (dx=-1;dx<2;dx+=2) /* look at upper and lower line, left */
    if (    diag && x<p->x && x-1>0 && y+dx >=0 && y+dx < p->y
         &&  col != ((pixel(p, x  , y+dx) < cs) ? 0 : 1)
         &&  col == ((pixel(p, x-1, y+dx) < cs) ? 0 : 1)
         && !((marked(p,x-1,y+dx)&mark)==mark)
       ) {
       if (blen+1>=bmax) { overflow|=1; continue; }
       buf[blen*2+0]=x-1;
       buf[blen*2+1]=y+dx;
       blen++;
    }
    if (x < *x0) *x0 = x;
    /* second go right, mark and get new starting points */ 
    for ( ; x<p->x && (col == ((pixel(p, x  , y) < cs) ? 0 : 1)) ; x++) {
      p->p[x + y * p->x] |= (mark & 7);    rc++;  /* mark pixel */
      /* enlarge frame */
      if (x > *x1) *x1 = x;
      for (dx=-1;dx<2;dx+=2) /* look at upper and lower line */
      if (     col == ((pixel(p, x  , y+dx) < cs) ? 0 : 1)
        && (
               col != ((pixel(p, x-1, y   ) < cs) ? 0 : 1)
            || col != ((pixel(p, x-1, y+dx) < cs) ? 0 : 1) )
        && !((marked(p,x,y+dx)&mark)==mark) && y+dx<p->y && y+dx>=0
         ) {
         if (blen+1>=bmax) { overflow|=1; continue; }
         buf[blen*2+0]=x;
         buf[blen*2+1]=y+dx;
         blen++;
      }
    }
    for (dx=-1;dx<2;dx+=2) /* look at upper and lower line, right */
    if (    diag  && x<p->x && x-1>0 && y+dx >=0 && y+dx < p->y
         &&  col == ((pixel(p, x-1, y   ) < cs) ? 0 : 1)
         &&  col != ((pixel(p, x  , y   ) < cs) ? 0 : 1)
         &&  col != ((pixel(p, x-1, y+dx) < cs) ? 0 : 1)
         &&  col == ((pixel(p, x  , y+dx) < cs) ? 0 : 1)
         && !((marked(p,x,y+dx)&mark)==mark) 
       ) {
       if (blen+1>=bmax) { overflow|=1; continue; }
       buf[blen*2+0]=x;
       buf[blen*2+1]=y+dx;
       blen++;
    }
  }
  
  /* debug, ToDo: use info maxstack and pixels for image classification */
  g_debug(fprintf(stderr," maxstack= %4d pixels= %6d",maxstack,rc);)
  if (overflow==1){
    overflow|=2;
    fprintf(stderr,"# Warning: frame_nn stack oerflow\n");
  }
  free(buf);
#else /* old version, ToDo: improve it for tmp04/005*.pgm.gz */
  int i, j, d, dx, ox, oy, od, nx, ny, rc = 0, rot = 0, x2 = x, y2 = y, ln;

  static const int d0[8][2] = { { 0, -1} /* up    */, {-1, -1}, 
				{-1,  0} /* left  */, {-1,  1}, 
				{ 0,  1} /* down  */, { 1,  1}, 
				{ 1,  0} /* right */, { 1, -1}};

  /* check bounds */
  if (outbounds(p, x, y))
    return 0;
  /* check if already marked */
  if ((marked(p,x,y)&mark)==mark) 
    return 0;

  i = ((pixel(p, x, y) < cs) ? 0 : 1);
  rc = 0;

  g_debug(fprintf(stderr," start frame:");)
  /* repeat the algorithm from other border ???
   * if first loop was around inner border (use labyrinth algo)
   * does not work for  @.......@
   *  		        @@@.X.@@@ < start on X => only right loops
   * 		        ..@@@@@..
   *  to avoid this store leftmost position for second start
   *  or change algorithm */
  for (ln = 0; ln < 2 && rot >= 0; ln++) {  // repeat if right-loop 
    g_debug(fprintf(stderr," ln=%d diag=%d cs=%d x=%d y=%d - go to border\n",ln,diag,cs,x,y);)
    
    od=d=(8+4*ln-diag)&7; // start robot looks up, right is a wall
    // go to right (left) border
    if (ln==1) { 
      x=x2;	y=y2; 
    } 
    /* start on leftmost position */
    for (dx = 1 - 2*ln; x + dx < p->x && x + dx >= 0 /* bounds */ &&
      	      	       i == ((pixel(p, x + dx, y) < cs) ? 0 : 1) /* color */; 
	      	       x += dx);

    g_debug(fprintf(stderr," ln=%d diag=%d cs=%d x=%d y=%d\n",ln,diag,cs,x,y);)

    /* robot stores start-position */
    ox = x;	oy = y;
    for (rot = 0; abs(rot) <= 64; ) {	/* for sure max. 8 spirals */
      /* leftmost position */
      if (ln == 0 && x < x2) {
	x2 = x; 	y2 = y;
      }	

      g_debug(fprintf(stderr," x=%3d y=%3d d=%d i=%d p=%3d rc=%d\n",x,y,d,i,pixel(p,x,y),rc);)

      if ( abs(d0[d][1]) ) {	/* mark left (right) pixels */
	for (j = 0, dx = d0[d][1]; x + j >= 0 && x + j < p->x
	              	&& i == ((pixel(p, x + j, y) < cs) ? 0 : 1); j += dx) {
	  if (!((marked(p, x + j, y)&mark)==mark))
	    rc++;
	  p->p[x + j + y * p->x] |= (mark & 7);
	}
      }
      /* look to the front of robot */
      nx = x + d0[d][0];
      ny = y + d0[d][1];
      /* if right is a wall */
      if ( outbounds(p, nx, ny) || i != ((pixel(p,nx,ny)<cs) ? 0 : 1) ) {
	/* rotate left */
        d=(d+2-diag) & 7; rot-=2-diag;
      }
      else {	/* if no wall, go forward and turn right (90 degrees) */
        x=nx; y=ny; d=(d+6) & 7; rot+=2;
	/* enlarge frame */
        if (x < *x0)      *x0 = x;
	if (x > *x1)	  *x1 = x;
	if (y < *y0)	  *y0 = y;
	if (y > *y1)	  *y1 = y;
      } 
      if(x==ox && y==oy && d==od) break;	// round trip finished
    }
  }
  g_debug(fprintf(stderr," rot=%d\n",rot);)
#endif
  return rc;
}

/* mark neighbouring pixel of same color, return number
 * better with neighbours of same color (more general) ???
 * parameters: (&~7)-pixmap, start-point, critical_value, mark
 *  recursion is removed */
int mark_nn(pix * p, int x, int y, int cs, int r) {
  /* out of bounds or already marked? */
  if (outbounds(p, x, y) || (marked(p, x, y)&r)==r) 
    return 0;
  {
    int x0, x1, y0, y1;
    x0 = x1 = x;
    y0 = y1 = y;			// not used
    return frame_nn(p, x, y, &x0, &x1, &y0, &y1, cs, r, JOB->tmp.n_run & 1);
    // using same scheme
  }
}


/* clear lowest 3 (marked) bits (they are used for marking) */ 
void clr_bits(pix * p, int x0, int x1, int y0, int y1) {
  int x, y;
  for ( y=y0; y <= y1; y++)
    for ( x=x0; x <= x1; x++)
      p->p[x+y*p->x] &= ~7;
}

/* look for white holes surrounded by black points
 * at the moment look for white point with black in all four directions
 *  - store position of hole in coordinates relativ to box!
 *  ToDo: count only holes with vol>10% ??? */
int num_hole(int x0, int x1, int y0, int y1, pix * p, int cs, holes_t *holes) {
  int num_holes = 0, x, y, hole_size;
  pix b;			// temporary mini-page
  int dx = x1 - x0 + 1, dy = y1 - y0 + 1;
  unsigned char *buf;	//  2nd copy of picture, for working 

  if (holes) holes->num=0;
  if(dx<3 || dy<3) return 0;
  b.p = buf = malloc( dx * dy * sizeof(unsigned char) );
  if( !buf ){
    fprintf( stderr, "\nFATAL: malloc(%d) failed, skip num_hole", dx*dy );
    return 0;
  }
  if (copybox(p, x0, y0, dx, dy, &b, dx * dy))
    { free(b.p); return -1;}

  // printf(" num_hole(");
  /* --- mark white-points connected with border */
  for (x = 0; x < b.x; x++) {
    if (pixel(&b, x, 0) >= cs)
      mark_nn(&b, x, 0, cs, AT);
    if (pixel(&b, x, b.y - 1) >= cs)
      mark_nn(&b, x, b.y - 1, cs, AT);
  }
  for (y = 0; y < b.y; y++) {
    if (pixel(&b, 0, y) >= cs)
      mark_nn(&b, 0, y, cs, AT);
    if (pixel(&b, b.x - 1, y) >= cs)
      mark_nn(&b, b.x - 1, y, cs, AT);
  }

  g_debug(out_b(NULL,b,0,0,b.x,b.y,cs);)
  // --- look for unmarked white points => hole
  for (x = 0; x < b.x; x++)
    for (y = 0; y < b.y; y++)
      if (!((marked(&b, x, y)&AT)==AT))	// unmarked
	if (pixel(&b, x, y) >= cs) {	// hole found
#if 0
	  hole_size=mark_nn(&b, x, y, cs, AT);  /* old version */
	  if (hole_size > 1 || dx * dy <= 40)
	    num_holes++;
#else
          {    /* new version, for future store of hole characteristics */
            int x0, x1, y0, y1, i, j;
            x0 = x1 = x;
            y0 = y1 = y;			// not used
            hole_size=frame_nn(&b, x, y, &x0, &x1, &y0, &y1, cs, AT, JOB->tmp.n_run & 1);
            // store hole for future use, num is initialized with 0
 	    if (hole_size > 1 || dx * dy <= 40){
 	      num_holes++;
              if (holes) {
                // sort in table
                for (i=0;i<holes->num && i<MAX_HOLES;i++)
                  if (holes->hole[i].size < hole_size) break;
                for (j=MAX_HOLES-2;j>=i;j--)
                  holes->hole[j+1]=holes->hole[j];
                if (i<MAX_HOLES) {
                  // printf("  i=%d size=%d\n",i,hole_size);
                  holes->hole[i].size=hole_size;
                  holes->hole[i].x=x;
                  holes->hole[i].y=y;
                  holes->hole[i].x0=x0;
                  holes->hole[i].y0=y0;
                  holes->hole[i].x1=x1;
                  holes->hole[i].y1=y1;
                }
                holes->num++;
              }
            }
          }
#endif
	}
  free(b.p);
  // printf(")=%d",num_holes);
  return num_holes;
}

/* count for black nonconnected objects --- used for i,auml,ouml,etc. */
int num_obj(int x0, int x1, int y0, int y1, pix * p, int cs) {
  int x, y, rc = 0;		// rc=num_obj
  unsigned char *buf; // 2nd copy of picture, for working
  pix b;

  if(x1<x0 || y1<y0) return 0;
  b.p = buf = malloc( (x1-x0+1) * (y1-y0+1) * sizeof(unsigned char) );
  if( !buf ){
    fprintf( stderr, "\nFATAL: malloc(%d) failed, skip num_obj",(x1-x0+1)*(y1-y0+1) );
    return 0;
  }
  if (copybox(p, x0, y0, x1 - x0 + 1, y1 - y0 + 1, &b, (x1-x0+1) * (y1-y0+1)))
    { free(b.p); return -1; }
  // --- mark black-points connected with neighbours
  for (x = 0; x < b.x; x++)
    for (y = 0; y < b.y; y++)
      if (pixel(&b, x, y) < cs)
	if (!((marked(&b, x, y)&AT)==AT)) {
	  rc++;
	  mark_nn(&b, x, y, cs, AT);
	}
  free(b.p);
  return rc;
}

#if 0
// ----------------------------------------------------------------------
// first idea for making recognition based on probability
//  - start with a list of all possible chars
//  - call recognition_of_char(box *) 
//    - remove chars from list which could clearly excluded
//    - reduce probability of chars which have wrong features
//  - font types list could also build
// at the moment it is only an idea, I should put it to the todo list
//  
char *list="0123456789,.\0xe4\0xf6\0xfc"	// "a=228 o=246 u=252
           "abcdefghijklmnopqrstuvwxyz"
           "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int  wert[100];
int  listlen=0,numrest=0;
// initialize a new character list (for future)
void ini_list(){ int i;
    for(i=0;list[i]!=0 && i<100;i++) wert[i]=0;
    numrest=listlen=i; } 
// exclude??? (for future) oh it was long time ago, I wrote that :/
void exclude(char *filt){ int i,j;
    for(j=0;filt[j]!=0 && j<100;j++)
    for(i=0;list[i]!=0 && i<100;i++)
    if( filt[j]==list[i] ) { if(!wert[i])numrest--; wert[i]++; } }
// get the result after all the work (for future)
char getresult(){ int i;
    if( numrest==1 )
    for(i=0;list[i]!=0 && i<100;i++) if(!wert[i]) return list[i];
    return '_';
 }
#endif

//  look at the environment of the pixel too (contrast etc.)
//   detailed analysis only of diff pixels!
//
// 100% * distance, 0 is best fit
// = similarity of two chars for recognition of garbled (verstuemmelter) chars
//   weight of pixels with only one same neighbour set to 0
//   look at contours too! v0.2.4: B==H
int distance( pix *p1, struct box *box1,
              pix *p2, struct box *box2, int cs){
   int rc=0,x,y,v1,v2,i1,i2,rgood=0,rbad=0,x1,y1,x2,y2,dx,dy,dx1,dy1,dx2,dy2;
   x1=box1->x0;y1=box1->y0;x2=box2->x0;y2=box2->y0;
   dx1=box1->x1-box1->x0+1; dx2=box2->x1-box2->x0+1; dx=((dx1>dx2)?dx1:dx2);
   dy1=box1->y1-box1->y0+1; dy2=box2->y1-box2->y0+1; dy=((dy1>dy2)?dy1:dy2);
   if(abs(dx1-dx2)>1+dx/16 || abs(dy1-dy2)>1+dy/16) return 100;
   // compare relations to baseline and upper line
   if(2*box1->y1>box1->m3+box1->m4 && 2*box2->y1<box2->m3+box2->m4) rbad+=128;
   if(2*box1->y0>box1->m1+box1->m2 && 2*box2->y0<box2->m1+box2->m2) rbad+=128;
   // compare pixels
   for( y=0;y<dy;y++ )
   for( x=0;x<dx;x++ ) {	// try global shift too ???
     v1     =((pixel(p1,x1+x  ,y1+y  )<cs)?1:0); i1=8;	// better gray?
     v2     =((pixel(p2,x2+x  ,y2+y  )<cs)?1:0); i2=8;	// better gray?
     if(v1==v2) { rgood+=16; continue; } // all things are right!
     // what about different pixel???
     // test overlap of surounding pixels ???
     v1=-1;
     for(i1=-1;i1<2;i1++)
     for(i2=-1;i2<2;i2++)if(i1!=0 || i2!=0){
       if( ((pixel(p1,x1+x+i1*(1+dx/32),y1+y+i2*(1+dy/32))<cs)?1:0)
         !=((pixel(p2,x2+x+i1*(1+dx/32),y2+y+i2*(1+dy/32))<cs)?1:0) ) v1++;
     }
     if(v1>0)rbad+=16*v1;
   }
   if(rgood+rbad) rc= 100*rbad/(rgood+rbad); else rc=99;
//   if(rc<10 && vvv){
//     fprintf(stderr," distance rc=%d\n",rc);
//     out_x(box1);out_x(box2);
//   }
   return rc;
}



// ============================= call OCR engine ================== ;)
//  nrun=0 from outside, nrun=1 from inside (allows modifications)
wchar_t whatletter(struct box *box1, int cs, int nrun){
   wchar_t bc=UNKNOWN;			// best letter
   wchar_t um=SPACE;			// umlaut? '" => modifier
   pix *p=box1->p;   // whole image
   int	x,y,dots,xa,ya,x0,x1,y0,y1,dx,dy,i;
   pix b;            // box
   struct box bbuf=*box1;  // restore after modifikation!

   xa=box1->x; ya=box1->y;
   x0=box1->x0;y0=box1->y0;
   x1=box1->x1;y1=box1->y1;
   // int vol=(y1-y0+1)*(x1-x0+1);	// volume
   // crossed l-m , divided chars
   while( get_bw(x0,x1,y0,y0,p,cs,1)!=1  &&  y0+1<y1) y0++;
   while( get_bw(x0,x1,y1,y1,p,cs,1)!=1  &&  y0+1<y1) y1--;
   dx=x1-x0+1;
   dy=y1-y0+1;	// size

   // better to proof the white frame too!!! ????
   // --- test for german umlaut and points above, not robust enough???
   // if three chars are connected i-dots (ari) sometimes were not detected
   //  - therefore after division a test could be useful
   // modify y0 only in second run!
   testumlaut(box1,cs,nrun+1,&um); /* set box1->modifier too */

   dots=box1->dots;
   y0  =box1->y0;	// dots==2 => y0 below double dots
   dy  =y1-y0+1;

   // move upper and lower border (for divided letters)
   while( get_bw(x0,x1,y0,y0,p,cs,1)==0  &&  y0+1<y1) y0++;
   while( get_bw(x0,x1,y1,y1,p,cs,1)==0  &&  y0+1<y1) y1--;
   while( get_bw(x0,x0,y0,y1,p,cs,1)==0  &&  x0+1<x1) x0++;
   while( get_bw(x1,x1,y0,y1,p,cs,1)==0  &&  x0+1<x1) x1--;
   dx=x1-x0+1;
   dy=y1-y0+1;	// size
   box1->x0=x0;box1->y0=y0;	// set reduced frame
   box1->x1=x1;box1->y1=y1;

   // set good startpoint (probably bad from division)?
   if( xa<x0 || xa>x1 || ya<y0 || ya>y1
     || pixel(p,xa,ya)>=cs /* || 2*ya<y0+y1 */ || dots>0   ){
     // subfunction? also called after division of two glued chars?
     for(y=y1;y>=y0;y--) // low to high (not i-dot)
     for(x=(x0+x1)/2,i=0;x>=x0 && x<=x1;i++,x+=((2*i&2)-1)*i) /* is that ok? */
     if(pixel(p,x,y)<cs && (pixel(p,x+1,y)<cs
                         || pixel(p,x,y+1)<cs)){ xa=x;ya=y;y=-1;break; }
     /* should box1->x,y be set? */
   }

   // ----- create char-only-box -------------------------------------
   if(dx<1 || dy<1) return bc; /* should not happen */
   b.p = (char *) malloc(dx*dy);
   if (!b.p) fprintf(stderr,"Warning: malloc failed L%d\n",__LINE__);
   if( copybox(p,x0,y0,dx,dy,&b,dx*dy) ) 
     { free(b.p); return bc; }
   // clr_bits(b.p,0,b.x-1,0,b.y-1);
   // ------ use diagonal too (only 2nd run?) 
   /* following code failes on ! and ?
      ToDo:
       - mark pixels neighoured to pixels outside and remove them from &b
         v0.40
         will be replaced by list of edge vectors  
       - mark accents, dots and remove them from &b
    */
   if (x0>0)  // mark left overlap
   for ( y=y0; y<=y1; y++) {
     if (pixel(p,x0-1,y)<cs && pixel(p,x0,y)<cs && (marked(&b,0,y-y0 )&1)!=1)
     mark_nn(&b,0,y-y0,cs,1);
   }
   if (x1<p->x-1)  // mark right overlap
   for ( y=y0; y<=y1; y++) {
     if (pixel(p,x1+1,y)<cs && pixel(p,x1,y)<cs && (marked(&b,x1-x0,y-y0)&1)!=1)
     mark_nn(&b,x1-x0,y-y0,cs,1);
   }
   mark_nn(&b,xa-x0,ya-y0,cs,2); // not glued chars
   for(x=0;x<b.x;x++)
   for(y=0;y<b.y;y++){
     if (  (marked(&b,x,y  )&3)==1 && pixel(&b,x,y  )<cs )
     b.p[x+y*b.x] = 255&~7;
   }
   
   if (bc == UNKNOWN)
     bc=ocr0(box1,&b,cs);

   // look for serifs and divide melted one (nmhk,wv)
   //    ##  #
   //    ##  #
   //  #########
   //      ^
   if (bc == UNKNOWN) {
     y=dy-1;
     x=loop(&b,0,y,dx,cs,0,RI);x1=loop(&b,x,y,dx,cs,1,RI);
     if(pixel(&b,x     ,y-1)>cs)
     if(pixel(&b,x+x1-1,y-1)>cs)
     if(pixel(&b,x+x1/2,y-1)>cs)
     if( num_cross(x+1,x+x1-2,y-2,y-2,&b,cs) > 1 )
     if( num_cross(x+1,x+x1/2,y-2,y-2,&b,cs) == 1 ){
       put(&b,x+x1/2,y,0,176);
       put(p,box1->x0+x+x1/2,box1->y0+y,0,176);
       bc=ocr0(box1,&b,cs);
     }
   }
   if (box1->num_ac==0 || box1->wac[0]<95)  /* not sure */
                      {JOB->tmp.n_run+=1;bc=ocr0(box1,&b,cs);JOB->tmp.n_run-=1;}
   if (box1->num_ac==0 || box1->wac[0]<90)  /* not sure */
                      {JOB->tmp.n_run+=2;bc=ocr0(box1,&b,cs);JOB->tmp.n_run-=2;}
   // ToDo: call next two only for gray images!
   if (box1->num_ac==0 || box1->wac[0]<90)  /* not sure */
                      {bc=ocr0(box1,&b,cs+32);}
   if (box1->num_ac==0 || box1->wac[0]<90)  /* not sure */
                      {bc=ocr0(box1,&b,cs-32);}
   if (box1->num_ac>0 && box1->wac[0]>95 && bc==UNKNOWN) { bc=box1->tac[0]; } // bug? 
   if (bc == UNKNOWN && um && nrun==0)  /* try to remove modifier */
                      { bc=whatletter(box1,cs,1); }
  /* ToDo: try to change pixels near cs?? or melt? */

   if (um) {  /* ToDo: is that obsolete now? */
     bc = compose(bc, um );
     box1->ac = compose(box1->ac, um );
     if (nrun>0) {
       // restore modified boxes
       box1->x0=bbuf.x0;
       box1->y0=bbuf.y0;
       box1->y1=bbuf.y1;
       box1->x1=bbuf.x1;
     }
   }
//   if (box1->c==UNKNOWN) out_b(box1,&b,0,0,dx,dy,cs); // test

   free(b.p);
   return bc;
}

/* ---------------------------- progress output ---------------------- */
FILE *fp=NULL; /* output stream 0..100% */ 

/* initialization of progress output, fname="<fileID>","<filename>","-"  */
int ini_progress(char *fname){
  int fd;
  if (fp) { fclose(fp); fp=NULL; }
  if (fname) if (fname[0]) {
    fd=atoi(fname);
    if(fd>255 || fname[((fd>99)?3:((fd>9)?2:1))]) fd=-1; /* be sure */
    if (fname[0]=='-' && fname[1]==0) { fp=stdout; }
#ifdef __USE_POSIX
    else if (fd>0) { fp=fdopen(fd,"w"); } /* not sure that "w" is ok ???? */
#endif
    else { fp=fopen(fname,"w");if(!fp)fp=fopen(fname,"a"); }
    if (!fp) {
      fprintf(stderr,"could not open %s for progress output\n",fname);
      return -1; /* no success */
    }
  }
  // fprintf(stderr,"# progress: fd=%d\n",fileno(fp));
  return 0; /* no error */
}

/* progress output p1=main_progress_0..100% p2=sub_progress_0..100% */
int progress(int p1, int p2){
  /*FIXME jb static*/static int p1old=0,p2old=100; /* only output if changed */
  if (p1<0) p1=p1old;  /* accept 0..100, -1 used as joker */
  if (p2<0) p2=p2old;
  if (fp && (p1old!=p1 || p2old!=p2)){
    if (fileno(fp)<3) fprintf(fp," %3d %3d\r",p1,p2);
    else              fprintf(fp," %3d %3d\n",p1,p2);
    fflush(fp);
    p1old=p1; p2old=p2;
    return 0;
  }
  return 0; /* no error */
}
/* --------------------- end of progress output ---------------------- */

/*
** creates a list of boxes/frames around objects detected 
** on the pixmap p for further work
** returns number of boxes created.
** - by the way: get average X, Y (avX=sumX/numC,..)
*/
int scan_boxes( pix *p ){
  int x,y,x0,x1,y0,y1,dots,cs;
  struct box *box3;

  if (JOB->cfg.verbose)
    fprintf(stderr,"# scanning boxes");

  cs = JOB->cfg.cs;
  JOB->res.sumX = JOB->res.sumY = JOB->res.numC = 0;
  clr_bits(p,0,p->x-1,0,p->y-1);

  for(y=0; y < p->y; y++)     // step 2 gives speedup and should work too
    for(x=0; x < p->x; x++) { // NO - dust of size 1 is not removed !!!
      if( marked(p,x,y)       )  // marked
	continue;
      if( pixel (p,x,y) >= cs )  // no pixel
	continue;
      x0=x;      x1=x;
      y0=y;      y1=y;
      dots=0;	// box

      frame_nn(p,x,y,&x0,&x1,&y0,&y1,cs,AT,1);  // frame and mark nn-dots
      pixel_atp(p,x,y)|=M1;			// mark startpoint
      JOB->res.numC++; JOB->res.sumX+=x1-x0+1; JOB->res.sumY+=y1-y0+1;

      // --- insert in list
      box3 = (struct box *)malloc(sizeof(struct box));
      box3->x0=x0;     box3->x1=x1;
      box3->y0=y0;     box3->y1=y1;
      box3->x=x;       box3->y=y;
      box3->dots=dots; 
      box3->c=(((y1-y0+1)*(x1-x0+1)>=MaxBox)? PICTURE : UNKNOWN);
      box3->ac=UNKNOWN;
      box3->modifier='\0';
      box3->num=JOB->res.numC;
      box3->line=0;	// not used here
      box3->m1=0; box3->m2=0; box3->m3=0; box3->m4=0;
      box3->p=p;
      box3->num_ac=0;   // for future use
      box3->obj=NULL;   // no object
      list_app(&(JOB->res.boxlist), box3); 	// append to list
      // out_x(box3);
  }
  if(JOB->res.numC){ if(JOB->cfg.verbose)fprintf(stderr," %d\n",JOB->res.numC); }
  return JOB->res.numC;
}

/* compare ints for sorting.  Return -1, 0, or 1 according to
   whether *vr < *vs, vr == *vs, or *vr > *vs */
int 
intcompare (const void *vr, const void *vs)
{
  int *r=(int *)vr;
  int *s=(int *)vs;

  if (*r < *s) return -1;
  if (*r > *s) return 1;
  return 0;
}

/* no negative numbers */
static int a0( int x ){ return ((x<0)?0:x); }

static int monospaced, pitch_mono, pitch_prop;
/*
  measure_pitch - detect monospaced font and measure the pitch
  ToDo: measure relative to line-high (m4-m1), to handle diff. fontsize (js)
*/
void
measure_pitch(){
  int width, maxwidths=0, numwidths=0, n, prop_min=9999, prop_max=-1000;
  int prev=-1000, center, *widths=0, dist, prop_av=0, np=0;
  struct box *box2,*box3=NULL;
  double v;

  if(JOB->cfg.verbose){ fprintf(stderr,"# check for word pitch ..."); }
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    center = box2->x0 + box2->x1; /* this doubles the calculated widths */
    width = center-prev;
    /* fonts are expected to be 7 to 60 pixels high, which is about
       5 to 50 pixels wide.  We allow some extra margin. */
    if (4 < width && width < 150) {
      if (numwidths >= maxwidths) {
	n=maxwidths*2+10;
	widths = xrealloc(widths, n*sizeof(int)); /* what if error ??? */
	maxwidths = n;
      }
      widths[numwidths++] = width;
    }
    prev = center;
    /* measure distance between */
    if(box3){
      dist = a0(box2->x0 - box3->x1 + 1);
      if (2*dist<3*JOB->res.avX && box3->y1-box2->y1<JOB->res.avY/2) {
        if (dist<prop_min) prop_min=dist;
        if (dist>prop_max) prop_max=dist;
        prop_av+=dist*dist;  /* for every line? y0>y1 */
        np+=dist;
      }
    }
    box3 = box2;
  } end_for_each(&(JOB->res.boxlist));
  
  if (np){
    pitch_prop=(prop_av+np/2+1)/(np)+1;
    if(JOB->cfg.verbose){ fprintf(stderr," min=%d max=%d pitch_p=%d\n#  ... ",
      prop_min,prop_max,pitch_prop); }
  }
  
  if( !numwidths ){
    if( widths )
      free(widths); /* avoid memory leak */
    fprintf(stderr," no spaces found\n");
    return;
  }
  
  qsort (widths, numwidths, sizeof (int), intcompare);

  v = (widths[numwidths*7/10]-widths[numwidths/5])/(double)widths[numwidths/5];
  /* measurements showed v=.09 for Courier and .44 for Times-Roman */
  monospaced = (v < .22);
  pitch_mono = widths[numwidths*2/5]/2; /* compensate for the factor of 2 */
  if(JOB->cfg.verbose){ fprintf(stderr," min=%d max=%d v=%f mono=%d pitch_m=%d\n",
    widths[0]/2,widths[numwidths-1]/2,v,monospaced,pitch_mono); }
  if( widths )
    free(widths);

}

/* ---- glue broken chars ( before step1 ??? )  ---------------------------------
    use this carefully, do not destroy previous detection ~fi, broken K=k' g 
    glue if boxes are near or diagonally connected 
    other strategy: mark boxes for deleting and delete in extra loop at end
    faster: check only next two following boxes because list is sorted!
    ToDo: store m4 of upper line to m4_of_prev_line, and check that "-points are below  
*/
int glue_broken_chars( pix *pp ){
  int ii,y,cs,x0,y0,x1,y1;
  struct box *box2,*box4,*box5;
  cs=JOB->cfg.cs;
  {
    if(JOB->cfg.verbose){ fprintf(stderr,"# glue broken chars ..."); }
    ii=0;
    for_each_data(&(JOB->res.boxlist)) {
      box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
      x0 = box2->x0;
      x1 = box2->x1;
      y0 = box2->y0;
      y1 = box2->y1;

      // vertical broken (g965T umlauts etc.)
      // not: f,
/*    ~; ???
      if( 3*y0>box2->m2+2*box2->m3 
       &&   y1>box2->m3 
       &&   x1-x0 < y1-y0 ) continue; // ~komma
*/
      
      if( box2->m4>0 && y0>box2->m4   ) continue; /* dust outside ? */
      if( box2->m1>0 && y0<box2->m1-(box2->m3-box2->m2) ) continue;
      /* ToDo:
       *  - check that y0 is greater as m3 of the char/line above 
       */

      // check near larger boxes
      if( 2*(y1-y0) < box2->m4 - box2->m1
       && 2*y1<(box2->m3+box2->m2) ) {  // only check fragments
        box5=NULL;          // nearest box
        for_each_data(&(JOB->res.boxlist)) {
  	  box4=(struct box *)list_get_current(&(JOB->res.boxlist));
          if( box4!=box2 && box4->c != PICTURE )
          if (box4->line>=0 && box2->line>=0 && box4->line==box2->line)
          {
             if (!box5) box5=box4;
             if ( abs(box4->x0 + box4->x1 - 2*box2->x0)
                 <abs(box5->x0 + box5->x1 - 2*box2->x0)) box5=box4;
  	  }
        } end_for_each(&(JOB->res.boxlist));
	box4=box5;
      	{
          if( /* umlaut "a "o "u, ij; box2 is the dot, box4 the body */
            (      y1<=box2->m2
              &&   box4->x1>=3*x0-2*x1  /* test if box4 is around box2 */
              && 2*box4->x0<=x0+x1+2    /* +2 for 1 pixel thick i's */
              && ( x1-x0-(x1-x0)/4 <= box4->x1-box4->x0+2 ) /* +1 for i's */
              && ( y0+2>=box2->m1 || 4*(y1-y0)<box2->m4-box2->m1 )
            ) || (	/* broken T */
              3*(box2->x1 - box2->x0) > 2*JOB->res.avX
            && 4*box4->x0>3*box2->x0+box2->x1
            && 4*box4->x1<box2->x0+3*box2->x1
            )
          ||  /* !?; box2 is the dot, box4 the body */
            (    2*box4->x1>=x0+x1 	/* test if box4 is around box2 */
              && 2*box4->x0<=2*x1 /* +x0+1 Jan00 */
              && ( x1-x0 <= box4->x1-box4->x0+2 )
              &&   2*y0>=box2->m2+box2->m3 
              &&   4*y1>=box2->m2+3*box2->m3 
              &&   4*(y1-y0)<box2->m4-box2->m1
              &&   (8*box4->y1 < box4->m2+7*box4->m3
                   || box4->m4-box4->m1<16) /* Jan00 */
            )
          ||  /* =;: */
            (    2*box4->x1>=x0+x1 	/* test if box4 is around box2 */
              && 2*box4->x0<=2*x1 /* +x0+1 */
              && ( x1-x0   <= box4->x1-box4->x0+4 )
              && ( x0 <= (3*box4->x1+box4->x0)/4   )
              && (( box2->m2 && box4->m2
                &&   y1< box2->m3
                && 2*box4->y1 >    box4->m3+box4->m2
                && 4*box4->y0 >= 3*box4->m2+box4->m3 
                && 2*box2->y0 <    box2->m3+box2->m2
                && (  4*box4->y1 < box4->m2+3*box4->m3 // :
                   ||   box4->y1 > box4->m3   // ; ocr-a-;
                   ) 
                 )
               || ( (!box2->m2) || (!box4->m2) )
              )
            )
          )
          if( ( box4->y1<box2->y0 && box2->y0-box4->y1<=2*(box2->y1-box2->y0)+4 ) 
           || ( box2->y1<box4->y0 && box4->y0-box2->y1<=2*(box4->y1-box4->y0)+4 ))
          {  // fkt melt(box2,box4)
            if( box4->x0<x0 ) x0=box2->x0=box4->x0;
            if( box4->x1>x1 ) x1=box2->x1=box4->x1;
            if( box4->y0<y0 ) y0=box2->y0=box4->y0;
            if( box4->y1>y1 ) y1=box2->y1=box4->y1;
            // out_x(box2);  // used for debugging
            JOB->res.numC--;ii++;	// remove
            // if (box4->obj) free(box4->obj);
	    list_del(&(JOB->res.boxlist), box4); /* ret&1: error-message ??? */
	    free(box4);
          }
	}
      }
// continue;
      // horizontally broken w' K'
      if(     2*y1  <   (box2->m3+box2->m2) )
      if( 2*(y1-y0) <   (box2->m3+box2->m2) )	// fragment
      for_each_data(&(JOB->res.boxlist)) {
	box4=(struct box *)list_get_current(&(JOB->res.boxlist));
        if(box4!=box2 && box4->c != PICTURE )
	{
          if( box4->line>=0 && box4->line==box2->line
          && box4->x1>=x0-1 && box4->x1<x0  // do not glue 6-
          && box4->x0+3*box4->x1<4*x0)
          if( get_bw(x0  ,x0  ,y1,y1  ,pp,cs,1) == 1)
          if( get_bw(x0-2,x0-1,y1,y1+2,pp,cs,1) == 1)
          {  // fkt melt(box2,box4)
            put(pp,x0,y1+1,~(128+64),0);
            if( box4->x0<x0 ) x0=box2->x0=box4->x0;
            if( box4->x1>x1 ) x1=box2->x1=box4->x1;
            if( box4->y0<y0 ) y0=box2->y0=box4->y0;
            if( box4->y1>y1 ) y1=box2->y1=box4->y1;
            JOB->res.numC--;ii++;	// remove
            // if (box4->obj) free(box4->obj);
	    list_del(&(JOB->res.boxlist), box4);
	    free(box4);
          }
        }
      } end_for_each(&(JOB->res.boxlist));
      // horizontally broken n h	(h=l_)		v0.2.5 Jun00
      if( abs(box2->m2-y0)<=(y1-y0)/8 )
      if( abs(box2->m3-y1)<=(y1-y0)/8 )
      if( num_cross(x0,         x1,(y0+  y1)/2,(y0+  y1)/2,pp,cs) == 1)
      if( num_cross(x0,         x1,(y0+3*y1)/4,(y0+3*y1)/4,pp,cs) == 1)
      if(    get_bw((3*x0+x1)/4,(3*x0+x1)/4,(3*y0+y1)/4,y1,pp,cs,1) == 0)
      if(    get_bw(x0,(3*x0+x1)/4,(3*y0+y1)/4,(y0+3*y1)/4,pp,cs,1) == 0)
      if(    get_bw(x0,         x0,         y0,(3*y0+y1)/4,pp,cs,1) == 1)
      for_each_data(&(JOB->res.boxlist)) {
	box4=(struct box *)list_get_current(&(JOB->res.boxlist));
      	if(box4!=box2 && box4->c != PICTURE )
	{
          if( box4->line>=0 && box4->line==box2->line
          && box4->x1>x0-3 && box4->x1-2<x0
           && abs(box4->y1-box2->m3)<2)
      	  {  // fkt melt(box2,box4)
      	    y=loop(pp,x0,y0,y1-y0,cs,0,DO);if(2*y>y1-y0) continue;
            put(pp,x0-1,y0+y  ,~(128+64),0);
            put(pp,x0-1,y0+y+1,~(128+64),0);
            if( box4->x0<x0 ) x0=box2->x0=box4->x0;
            if( box4->x1>x1 ) x1=box2->x1=box4->x1;
      	    if( box4->y0<y0 ) y0=box2->y0=box4->y0;
            if( box4->y1>y1 ) y1=box2->y1=box4->y1;
            JOB->res.numC--;ii++;	// remove
            // if (box4->obj) free(box4->obj);
	    list_del(&(JOB->res.boxlist), box4);
	    free(box4);
          }
      	}
      } end_for_each(&(JOB->res.boxlist));
    } end_for_each(&(JOB->res.boxlist)); 
    if(JOB->cfg.verbose)fprintf(stderr," %3d times glued, remaining boxes %d\n",ii,JOB->res.numC);
  }
  return 0;
}

/*
** this is a simple way to improve results on noisy images:
** - find similar chars (build cluster of same chars)
** - analyze clusters (could be used for generating unknown font-base)
** - the quality of the result depends mainly on the distance function
*/
  // ---- analyse boxes, compare chars, compress picture ------------
  // ToDo: - error-correction only on large chars! 
int find_same_chars( pix *pp){
  int i,k,d,cs,dist,n1,dx; struct box *box2,*box3,*box4,*box5;
  pix p=(*pp);
  cs=JOB->cfg.cs;
  {
    if(JOB->cfg.verbose)fprintf(stderr,"# packing");
    i = list_total(&(JOB->res.boxlist));
    for_each_data(&(JOB->res.boxlist)) {
      box4 = box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
      dist=1000;	// 100% maximum
      dx = box2->x1 - box2->x0 + 1;

      if(JOB->cfg.verbose)fprintf(stderr,"\r# packing %5d",i);
      if( dx>3 )
      for(box3=(struct box *)list_next(&(JOB->res.boxlist),box2);box3;
	  box3=(struct box *)list_next(&(JOB->res.boxlist),box3)) {
        if(box2->num!=box3->num){
          int d=distance(&p,box2,&p,box3,cs);
          if ( d<dist ) { dist=d; box4=box3; }	// best fit
          if ( d<5 ){   // good limit = 5% ??? 
            i--;n1=box3->num;		// set all num==box2.num to box2.num
	    for_each_data(&(JOB->res.boxlist)) {
	      box5=(struct box *)(struct box *)list_get_current(&(JOB->res.boxlist));
	      if(box5!=box2)
              if( box5->num==n1 ) box5->num=box2->num;
	    } end_for_each(&(JOB->res.boxlist));
          // out_x2(box2,box5);
          // fprintf(stderr," dist=%d\n",d);
          }
      	}
      }
      // nearest dist to box2 has box4
      //    out_b2(box2,box4);
      //    fprintf(stderr," dist=%d\n",dist); 
    } end_for_each(&(JOB->res.boxlist));
    k=0;
    if(JOB->cfg.verbose)fprintf(stderr," %d different chars",i);
    for_each_data(&(JOB->res.boxlist)) {
      struct box *box3,*box4;
      int j,dist;
      box2=(struct box *)list_get_current(&(JOB->res.boxlist));
      for(box3=list_get_header(&(JOB->res.boxlist));box3!=box2 && box3!=NULL;
	  box3=list_next(&(JOB->res.boxlist), box3))
        if(box3->num==box2->num)break;
      if(box3!=box2 && box3!=NULL)continue;
      i++;
      // count number of same chars
      dist=0;box4=box2;
      
      for(box3=box2,j=0;box3;box3=list_next(&(JOB->res.boxlist), box3)) {
	if(box3->num==box2->num){
          j++;
          d=distance(&p,box2,&p,box3,cs);
          if ( d>dist ) { dist=d; box4=box3; }	// worst fit
	}
      }
      if(JOB->cfg.verbose&8){
        out_x2(box2,box4);
        fprintf(stderr," no %d char %4d %5d times maxdist=%d\n",i,box2->num,j,dist);
      }
      // calculate mean-char (error-correction)
      // ToDo: calculate maxdist in group 
      k+=j;
  //    if(j>1)
  //    out_b(box1,NULL,0,0,0,0,cs);
      if(JOB->cfg.verbose&8)
      fprintf(stderr," no %d char %4d %5d times sum=%d\n",i,box2->num,j,k);   
    } end_for_each(&(JOB->res.boxlist));
    if(JOB->cfg.verbose)fprintf(stderr," ok\n");
  }
  return 0; 
}

/*
** call the first engine for all boxes and set box->c=result;
**
*/
int char_recognition( pix *pp, int mo){
  int i,ii,ni,cs,x0,y0,x1,y1;
  struct box *box2, *box3;
  wchar_t cc;
  cs=JOB->cfg.cs;
  // ---- analyse boxes, find chars ---------------------------------
  if(JOB->cfg.verbose)fprintf(stderr,"# step 1: char recognition");
  i=ii=ni=0;
  for_each_data(&(JOB->res.boxlist)) { /* count boxes */
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    /* wew: isn't this just JOB->res.numC? */
    /* js: The program is very complex. I am not sure anymore
           wether numC is the number of boxes or the number of valid
           characters.
           Because its not time consuming I count the boxes here. */
    if (box2->c==UNKNOWN)  i++;
    if (box2->c==PICTURE) ii++;
    ni++;
  } end_for_each(&(JOB->res.boxlist)); 
  if(JOB->cfg.verbose)
    fprintf(stderr," unknown= %d picts= %d boxes= %d",i,ii,ni);
  if (!ni) return 0;
  i=ii=0;
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    x0=box2->x0;x1=box2->x1;
    y0=box2->y0;y1=box2->y1;	// box

    cc=box2->c;
    if ((mo&256)==0) { /* this case should be default (main engine) */
      if(cc==UNKNOWN)
        cc=whatletter(box2,cs   ,0);
      
      if(cc==UNKNOWN && cs+32<256){  // only makes sense for gray-pictures!
        cc=whatletter(box2,cs+32,0); // 90%
      }
    }

    if(mo&2) 
      if(cc==UNKNOWN)
	cc=ocr_db(box2);


    // box2->c=cc; bad idea (May03 removed)
    // set(box2,cc,95); ToDo: is that better? 

    if(cc==UNKNOWN) 
	i++;
    ii++;

    if(JOB->cfg.verbose&8) { 
      fprintf(stderr,"\n# code= %04lx %c",(long)cc,(char)((cc<255)?cc:'_')); 
      out_b(box2,pp,x0,y0,x1-x0+1,y1-y0+1,cs);
    }
    progress(-1,ii*100/ni);

  } end_for_each(&(JOB->res.boxlist));
  if(JOB->cfg.verbose)fprintf(stderr,", %d of %d chars unidentified\n",i,ii);
  return 0;
}


/*
** compare unknown with known chars,
** very similar to the find_similar_char_function but here only to
** improve the result
*/
int compare_unknown_with_known_chars(pix * pp, int mo) {
  int i, cs = JOB->cfg.cs, dist, d, ad, wac, ni, ii;
  struct box *box2, *box3, *box4;
  wchar_t bc;
  i = 0;			// ---- ------------------------------- 
  if (JOB->cfg.verbose)
    fprintf(stderr, "# step 2: try to compare unknown with known chars");
  if (!(mo & 8))
  {
    ii=ni=0;
    for_each_data(&(JOB->res.boxlist)) { ni++; } end_for_each(&(JOB->res.boxlist));
    for_each_data(&(JOB->res.boxlist)) {
      box2 = (struct box *)list_get_current(&(JOB->res.boxlist)); ii++;
      if (box2->c == UNKNOWN || (box2->num_ac>0 && box2->wac[0]<97))
	if (box2->y1 - box2->y0 > 4 && box2->x1 - box2->x0 > 1) { // no dots!
	  box4 = (struct box *)list_get_header(&(JOB->res.boxlist));;
	  dist = 1000;		/* 100% maximum */
	  bc = UNKNOWN;		/* best fit char */
	  for_each_data(&(JOB->res.boxlist)) {
	    box3 = (struct box *)list_get_current(&(JOB->res.boxlist));
            wac=((box3->num_ac>0)?box3->wac[0]:100);	    
	    if (box3 != box2 && box3->c != UNKNOWN && wac>95) {
	       d = distance(pp, box2, pp, box3, cs);
	       if (d < dist) {
		  dist = d;
		  bc = box3->c;
		  box4 = box3;
	       }
	    }
	  } end_for_each(&(JOB->res.boxlist));
	  if (dist < 10) {
            /* sureness can be maximal of box3 */
	    if (box4->num_ac>0) ad = box4->wac[0];
	    else                ad = 97;
	    ad-=dist; if(ad<1) ad=1; 
	    /* ToDo: ad should depend on ad of bestfit */
	    setac(box2,(wchar_t)bc,ad);
	    i++;
	  }			// limit as option???
	  //  => better max distance('e','e') ???
	  if (dist < 50 && (JOB->cfg.verbose & 7)) {	// only for debugging
	    fprintf(stderr, "\n# L%02d best fit was %04x %c dist=%3d%% i=%d", box2->line,
		    (int)bc, (char)((bc<255)?bc:'_'), dist, i);
	    if(box4->num_ac>0)fprintf(stderr," w= %3d%%",box4->wac[0]);
	    if ((JOB->cfg.verbose & 4) && dist < 10)
	      out_x2(box2, box4);
	  }
	  progress(-1,ii*100/ni);
	}
    } end_for_each(&(JOB->res.boxlist));
  }
  if (JOB->cfg.verbose)
    fprintf(stderr, " - found %d\n", i);
  return 0;
}

/*
// ---- divide overlapping chars which !strchr("_,.:;",c);
// block-splitting (two ore three glued chars)
// division if dots>0 does not work properly! ???
//
// what about glued "be"?
// what about recursive division?
*/
int  try_to_divide_boxes( pix *pp, int mo){
  struct box *box2,*box3,boxa,boxb,boxc;
  int cs=JOB->cfg.cs, ad=100;
  wchar_t c1,c2,c3,s1[]={ UNKNOWN, '_', '.', ',', '\'', '!', ';', '?', ':', '-', 
      '=', '(', ')', '\0' };	// not accepted chars, \0-terminated!
  int k2,x0,x1,y0,y1,x,x2;
  int i,ii,j,k,m,m1,m2,m3,n1,i1,i2,i3,a1,a2,ai[3],am[3],dx,dy;
  // pix p=(*pp); // remove!
  if(JOB->cfg.verbose)fprintf(stderr,"# step 3: try to divide unknown chars");
  if(!(mo&16))  // put this to the caller
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if(box2->c==UNKNOWN && box2->x1-box2->x0>5 && box2->y1-box2->y0>4){
      c1=c2=c3=UNKNOWN; 
      x=x2=0;
      x0=box2->x0; x1=box2->x1;
      y0=box2->y0; y1=box2->y1;
      ad=100;
      
      /* get minimum vertical lines */
      n1 = num_cross(x0,x1,(  y1+y0)/2,(  y1+y0)/2,pp,cs);
      ii = num_cross(x0,x1,(3*y1+y0)/4,(3*y1+y0)/4,pp,cs); if (ii<n1) n1=ii;
      if (box2->m2)
      for (i=box2->m2+1;i<=box2->m3-1;i++) {
        if (loop(pp,x0+1,i,x1-x0,cs,1,RI) > (x1-x0-2)) continue; // ll
        ii = num_cross(x0,x1,i,i,pp,cs); if (ii<n1) n1=ii;
      } if (n1<2) continue;  // seems to make no sence to divide
      if (n1<4) ad=98*ad/100;
      if (n1<3) ad=98*ad/100;
            
      if( 2*y1 < box2->m3+box2->m4    /* baseline char ? */
       && num_cross(x0,x1,y1-1,y1-1,pp,cs)==1  // -1 for slopes
       && num_cross((x0+2*x1)/3,(x0+3*x1)/4,y0,y1,pp,cs)<3  // not exclude tz
       && num_cross((3*x0+x1)/4,(2*x0+x1)/3,y0,y1,pp,cs)<3  // not exclude zl
       && loop(pp,x0,y1-(y1-y0)/32,x1-x0,cs,0,RI)
         +loop(pp,x1,y1-(y1-y0)/32,x1-x0,cs,0,LE) > (x1-x0+1)/2
        ) continue; /* do not try on bvdo"o etc. */
        
      // one vertical line can not be two glued chars
      if( num_cross(x0,x1,(  y1+y0)/2,(  y1+y0)/2,pp,cs)>1 )
      {	// doublet = 2 letters
        dx=(x1-x0)/32;
        dy=(y1-y0+1);
        if(JOB->cfg.verbose&2){ 
          fprintf(stderr,"\n# divide box: %4d %4d %3d %3d\n",x0,y0,x1-x0+1,y1-y0+1);
          if(JOB->cfg.verbose&4)out_b(NULL,pp,x0,y0,x1-x0+1,y1-y0+1,cs);
        }
        m1=m2=m3=0; i1=i2=i3=0; // searching minima m1 m2 m3
        // it would be better if testing is only if most right and left char
        //   is has no horizontal gap (below m2) ex: be
        for(i=0;i<(x1-x0)/2-2;i++)   // rm <=> nn .@ mask? for better sorting
        for(ii=-1;ii<2;ii+=((i)?2:4)){ // left and right from middle
          x=(x1+x0)/2+ii*i;
          for(k=0,j=y0;j<=y1;j++) k+=((pixel(pp,x  ,j)<cs)?1:0);
          if(4*k>3*(y1-y0+1)) continue;	// do not divide across black line
          // do not try division right of r in case of glued ar
          if( ii>0 && num_cross(x+1,x1,(y1+y0)/2,(y1+y0)/2,pp,cs)==0 ) continue; 
          if( ii<0 && num_cross(x0,x-1,(y1+y0)/2,(y1+y0)/2,pp,cs)==0 ) continue;
          m=loop(pp,x  ,y0,y1-y0+1,cs,0,DO)
           +loop(pp,x  ,y1,y1-y0+1,cs,0,UP);
          if( loop(pp,x  ,y1,y1-y0+1,cs,0,UP)>(y1-y0)/2
           && loop(pp,x+1,y1,y1-y0+1,cs,0,UP)<(y1-y0)/2 ){ // re
            k=loop(pp,x+1,y0,y1-y0+1,cs,0,DO)
             +loop(pp,x  ,y1,y1-y0+1,cs,0,UP);
          } else {
            k=loop(pp,x  ,y0,y1-y0+1,cs,0,DO)
             +loop(pp,x-1,y1,y1-y0+1,cs,0,UP);
          } if(k>m) m=k;
          k=loop(pp,x-2,y1,y1-y0+1,cs,0,UP);
          if(2*k>y1-y0){
           k=(y1-y0)/2;
           k+=loop(pp,x  ,y0  ,y1-y0+1,cs,0,DO)
             +loop(pp,x-1,y1-k,y1-y0+1,cs,0,UP); if(k>m) m=k;
          }
          m*=8;  // pretty good!
          for(k2=0,j=y0;j<=y1;j++){
            k=        ((pixel(pp,x  ,j)<cs)?0:1); m+=4*k; // using gray ???
            if(!k) m+=((pixel(pp,x-1,j)<cs)?0:2);
            if(!k) m+=((pixel(pp,x+1,j)<cs)?0:2);
            if(!k) m+=((pixel(pp,x-2,j)<cs)?0:1);
            if(!k) m+=((pixel(pp,x+2,j)<cs)?0:1);
            if(k!=k2) m-=dy/4; k2=k;  // many b/w changes are bad!
          }
          // replace one of 3 maxima (nearest or lowest
          if( abs(i3-ii*i)<2+dx ){ if(m>m3) { m3=m;i3= ii*i; } } else
          if( abs(i2-ii*i)<2+dx ){ if(m>m2) { m2=m;i2= ii*i; } } else
          if( abs(i1-ii*i)<2+dx ){ if(m>m1) { m1=m;i1= ii*i; } } else
                                 { if(m>m3) { m3=m;i3= ii*i; } }
          // sort it:  m1 > m2 > m3
          if( m3>m2 ){ k=m2;m2=m3;m3=k; k=i2;i2=i3;i3=k; }
          if( m2>m1 ){ k=m1;m1=m2;m2=k; k=i1;i1=i2;i2=k; }
          if( m3>m2 ){ k=m2;m2=m3;m3=k; k=i2;i2=i3;i3=k; }

        }
        x=0;
        ai[0]=i1+=(x1+x0)/2; am[0]=m1;
        ai[1]=i2+=(x1+x0)/2; am[1]=m2;
        ai[2]=i3+=(x1+x0)/2; am[2]=m3;
        if(JOB->cfg.verbose&2)fprintf(stderr," x123= %d %d %d  m123= %d %d %d\n",i1-x0,i2-x0,i3-x0,m1,m2,m3);
        // removing ->dots if dot only above one char !!! ??? not implemented
        for(a1=0;a1<3;a1++)
        if( 2*am[a1]>y1-y0 ) // minimum of white pixels should be found
        {
          boxa=*box2;boxb=*box2,boxc=*box2;	// copy contents
          x=ai[a1];
          boxa.x=x0; boxa.y=y0;boxa.x1=x;
          boxb.x=x+1;boxb.y=y0;boxb.x0=x+1;
          c1=whatletter(&boxa,cs,0); // unknown startpos!
          c2=whatletter(&boxb,cs,0);
          if(JOB->cfg.verbose&2)
            fprintf(stderr," x c12 =%2d %s %s. (%3d) (%3d)\n",x-x0,
              decode(c1,ASCII),decode(c2,ASCII),testac(&boxa,c1),testac(&boxb,c2));
	  // boxa..c changed!!! dots should be modified!!!
	  if (testac(&boxa,c1)*ad/100<95 || testac(&boxb,c2)*ad/100<95) x=0; else
	  if (wcschr(s1, c1) || wcschr(s1, c2)) x=0;
          if (x) break;
        }
        if(n1>2 && !x)
        for(a1=   0;a1<2;a1++)
        for(a2=a1+1;a2<3;a2++)
        if( 2*am[a2]>y1-y0 )
        {
          boxa=*box2;boxb=*box2;boxc=*box2;	// copy contents
          x2=ai[a2]; x=ai[a1]; if(x>x2){ k=x;x=x2;x2=k; }
          if (x-x0<2 || x2-x<2 || x1-x2<2) { x=0; continue; } // to small
          boxa.x=x0;  boxa.y=y0;boxa.x1=x;
          boxb.x=x+1; boxb.y=y0;boxb.x0=x+1;boxb.x1=x2;
          boxc.x=x2+1;boxc.y=y0;boxc.x0=x2+1;
          c1=whatletter(&boxa,cs,0); // unknown startpos!
          c2=whatletter(&boxb,cs,0);
          c3=whatletter(&boxc,cs,0);
          if(JOB->cfg.verbose&2)fprintf(stderr," x c123=%2d %2d %s %s %s.\n",x-x0,x2-x0,
                        decode(c1,ASCII),decode(c2,ASCII),decode(c3,ASCII));
	  if (testac(&boxa,c1)*ad/100<95
	   || testac(&boxb,c2)*ad/100<95
	   || testac(&boxc,c3)*ad/100<95) x=0; else
	  if (wcschr(s1, c1) || wcschr(s1, c2) || wcschr(s1, c3)) x=0;
          if (x) break;
        }
      }
      if(x>x0 && x<x1){			// separate first
        box2->y0=boxb.y0;
        box2->y1=boxb.y1;
        // --- insert ind list
        box3=malloc_box(&boxa);  // *box2=>boxa,boxb 024a4
        box3->x1=x;   box3->c=c1;
        box2->x0=x+1; box2->c=c2;
        box3->num=JOB->res.numC;
        box3->obj=NULL;
        // box3->num_ac=0;
        box2->num_ac=0;
        setac(box2,(wchar_t)c2,95*ad/100);
	if( list_ins(&(JOB->res.boxlist), box2, box3) ){ fprintf(stderr,"ERROR list_ins\n"); };
        JOB->res.numC++;
        if(x2>x && x2<x1){
          // --- insert in list
          box3=malloc_box(&boxb);
          box3->x1=x2;   box3->c=c2;
          box2->x0=x2+1; box2->c=c3;
          box3->num=JOB->res.numC;
          box3->obj=NULL;
          // box3->num_ac=0;
          box2->num_ac=0;
          setac(box2,(wchar_t)c3,95*ad/100);
	  list_ins(&(JOB->res.boxlist), box2, box3);
          JOB->res.numC++;
        }
        continue; 
      }
    }
  } end_for_each(&(JOB->res.boxlist));
  if(JOB->cfg.verbose)fprintf(stderr,", numC %d\n",JOB->res.numC); 
  return 0;
}

/*
// ---- divide vertical glued boxes (ex: g above T);
*/
int  divide_vert_glued_boxes( pix *pp, int mo){
  struct box *box2,*box3,*box4;
  int y0,y1,y,dy,flag_found,dx;
  if(JOB->cfg.verbose)fprintf(stderr,"# divide vertical glued boxes");
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if (box2->c != UNKNOWN) continue; /* dont try on pictures */
    y0=box2->y0; y1=box2->y1; dy=y1-y0+1;
    dx=4*(JOB->res.avX+box2->x1-box2->x0+1);     // we want to be sure to look at 4ex distance 
    if ( dy>2*JOB->res.avY && dy<6*JOB->res.avY && box2->m1
      && y0<=box2->m2+2 && y0>=box2->m1-2
      && y1>=box2->m4+JOB->res.avY-2)
    { // test if lower end fits one of the other lines?
      box4=box2; flag_found=0;
      for_each_data(&(JOB->res.boxlist)) {
        box4 = (struct box *)list_get_current(&(JOB->res.boxlist));
        if (box4->c != UNKNOWN) continue; /* dont try on pictures */
        if (box4->x1<box2->x0-dx || box4->x0>box2->x1+dx) continue; // ignore far boxes
        if (box4->line==box2->line  ) flag_found|=1;    // near char on same line
        if (box4->line==box2->line+1) flag_found|=2;    // near char on next line
        if (flag_found==3) break;                 // we have two vertical glued chars
      } end_for_each(&(JOB->res.boxlist));
      if (flag_found!=3) continue;         // do not divide big chars or special symbols
      y=box2->m4;  // lower end of the next line
      if(JOB->cfg.verbose&2){
        fprintf(stderr,"\n# divide box at y=%4d",y-y0);
        if(JOB->cfg.verbose&6)out_x(box2);
      }
      // --- insert box3 before box2
      box3=malloc_box(box2);
      box3->y1=y;
      box2->y0=y+1; box2->line++; // m1..m4 should be corrected!
      if (box4->line == box2->line){
        box2->m1=box4->m1;        box2->m2=box4->m2;
        box2->m3=box4->m3;        box2->m4=box4->m4;
      }
      box3->num=JOB->res.numC;
      box3->obj=NULL;
      if (list_ins(&(JOB->res.boxlist), box2, box3)) { fprintf(stderr,"ERROR list_ins\n"); };
      JOB->res.numC++;
    }
  } end_for_each(&(JOB->res.boxlist));
  if(JOB->cfg.verbose)fprintf(stderr,", numC %d\n",JOB->res.numC); 
  return 0;
}


/* 
   on some systems isupper(>255) cause a segmentation fault SIGSEGV
   therefore this function
   ToDo: should be replaced (?) by wctype if available on every system
 */
int wisupper(wchar_t cc){ return ((cc<128)?isupper(cc):0); }
int wislower(wchar_t cc){ return ((cc<128)?islower(cc):0); }
int wisalpha(wchar_t cc){ return ((cc<128)?isalpha(cc):0); }
int wisdigit(wchar_t cc){ return ((cc<128)?isdigit(cc):0); }
int wisspace(wchar_t cc){ return ((cc<128)?isspace(cc):0); }



/* ---- proof difficult chars Il1 by context view ----
  context: separator, number, vowel, nonvowel, upper case ????
  could be also used to find unknown chars if the environment (nonumbers)
    can be found in other places!
  ToDo:
   - box->ac as set of possible chars, ac set by engine, example:
       ac="l/" (not "Il|/\" because serifs detected and slant>0)
       correction only to one of the ac-set (alternative chars)!
   - should be language-settable; Unicode compatible 
   - box2->ad and wac should be changed? (not proper yet)
 *  ------------- */
int context_correction(pix * pp) {
 // const static char
  char *l_vowel="aeiouy";
    // *l_Vowel="AEIOU",chars if the environment (nonumbers)
  char *l_nonvo = "bcdfghjklmnpqrstvwxz";
  struct box *box4, *box3, *box2, *prev, *next;

  if (JOB->cfg.verbose)
    fprintf(stderr, "# step 4: context correction Il1 0O");

  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if (box2->c > 0xFF) continue; // temporary UNICODE fix 
    prev = (struct box *)list_get_cur_prev(&(JOB->res.boxlist));
    next = (struct box *)list_get_cur_next(&(JOB->res.boxlist));
    if( (prev) && (prev->c > 0xFF)) continue; //  temporary UNICODE fix 2
    if( (next) && (next->c > 0xFF)) continue; //  temporary UNICODE fix 3
    if (box2->num_ac<2) continue; // no alternatives
    if (box2->wac[0]==100 && box2->wac[1]<100) continue;

    /* check for Il1| which are general difficult to distinguish */
    /* bbg: not very good. Should add some tests to check if is preceded by '.',
     spelling, etc */
    /* ToDo: only correct if not 100% sure (wac[i]<100)
        and new char is in wat[] */
    if (strchr("Il1|", box2->c) && next && prev) {
//       if( strchr(" \n",prev->c)      // SPC 
//        && strchr(" \n",next->c) ) box2->c='I'; else // bad idea! I have ...
      if (wisalpha(next->c) && next->c!='i' && 
          ( prev->c == '\n' || 
	   ( prev->c == ' ' &&
	    ( box4=(struct box *)list_prev(&(JOB->res.boxlist), prev)) &&
	      box4->c == '.' ) ) )
	{ if (testac(box2,(wchar_t)'I')) box2->c = 'I'; }
      else if (box2->c!='1' && strchr(l_nonvo,next->c) && 
                               strchr("\" \n",prev->c)) /* lnt => Int, but 1st */
        /* do not change he'll to he'Il! */
	{ if (testac(box2,(wchar_t)'I')) box2->c = 'I'; }
      else if (strchr(l_vowel,next->c)) /* unusual? Ii Ie Ia Iy Iu */   
          /*  && strchr("KkBbFfgGpP",prev->c)) */ /* kle Kla Kli */
	{ if (testac(box2,(wchar_t)'l')) box2->c = 'l'; }
      else if (wisupper(next->c)
            && !strchr("O0I123456789",next->c)
            && !strchr("O0I123456789",prev->c)) /* avoid lO => IO (10) */
	{ if (testac(box2,(wchar_t)'I')) box2->c = 'I'; }
      else if (wislower(prev->c))
	{ if (testac(box2,(wchar_t)'l')) box2->c = 'l'; }
      else if (wisdigit(prev->c) || wisdigit(next->c)
       || (next->c=='O' && !wisalpha(prev->c)))  /* lO => 10 */
	{ if (testac(box2,(wchar_t)'1')) box2->c = '1'; }
    }
    
    /* check for O0 */
    else if (strchr("O0", box2->c) && next && prev) {
      if (wisspace(prev->c) && wisalpha(next->c)) /* initial letter */
	{ if (testac(box2,(wchar_t)'O')) box2->c = 'O'; }
      else if (wisalpha(prev->c) && wisalpha(next->c)) /* word in upper case */
	{ if (testac(box2,(wchar_t)'O')) box2->c = 'O'; }
      else if (wisdigit(prev->c) || wisdigit(next->c))
	{ if (testac(box2,(wchar_t)'0')) box2->c = '0'; }
    }

    /* was a space not found? xXx => x Xx ??? */
    if (wisupper(box2->c) && next && prev) {
      if (wislower(prev->c) && wislower(next->c)
	  && 2 * (box2->x0 - prev->x1) > 3 * (next->x0 - box2->x1)) {
	struct box *box3 = malloc_box((struct box *) NULL);
	box3->x0 = prev->x1 + 2;
	box3->x1 = box2->x0 - 2;
	box3->y0 = box2->y0;
	box3->y1 = box2->y1;
	box3->x = box2->x0 - 1;
	box3->y = box2->y0;
	box3->dots = 0;
	box3->c = ' ';
	box3->num = -1;
	box3->line = prev->line;
	box3->m1 = box3->m2 = box3->m3 = box3->m4 = 0;
	box3->p = pp;
	box3->num_ac = 0;
	box3->obj=NULL;
	list_ins(&(JOB->res.boxlist), box2, box3);
      }
    }
    
    /* a space before punctuation? but not " ./file" */
    if ( prev && next)
    if (prev->c == ' ' && strchr(" \n"    , next->c)
                       && strchr(".,;:!?)", box2->c))
      if (prev->x1 - prev->x0 < 2 * JOB->res.avX) {	// carefully on tables
	box3 = prev;
         // if (box3->obj) free(box3->obj);
	list_del(&(JOB->res.boxlist), box3);
	free(box3);
      }

    /* \'\' to \" */
    if ( prev )
    if ( (prev->c == '`' || prev->c == '\'')
      && (box2->c == '`' || box2->c == '\'') )
      if (prev->x1 - box2->x0 < JOB->res.avX) { // carefully on tables
        box2->c='\"';
	box3 = prev;
         // if (box3->obj) free(box3->obj);
	list_del(&(JOB->res.boxlist), box3);
	free(box3);
      }
  } end_for_each(&(JOB->res.boxlist));
  if (JOB->cfg.verbose)
    fprintf(stderr, "\n");
  return 0;
}


/* ---- insert spaces ----
   ToDo:
    - min/max distance-matrix  a-a,a-b,a-c,a-d ... etc;  td,rd > ie,el,es
    - OR measuring distance as min. pixel distance instead of box distance
    - if (!monospaced) get_pitch_of_line(maxline)
 * ------------------------ */
int list_insert_spaces( pix *pp, int spc ) { 
  int i=0, j1, j2, i1, lspc=spc, maxline=-1, dy=0; char cc;
  struct box *box2,*box3,*box4;

  // measure mean line height
  for(i1=1;i1<JOB->res.lines.num;i1++) {
    dy+=JOB->res.lines.m4[i1]-JOB->res.lines.m1[i1]+1;
  } if (JOB->res.lines.num>1) dy/=(JOB->res.lines.num-1);
  i=0; j2=0;
  for(i1=1;i1<JOB->res.lines.num;i1++) {
    j1=JOB->res.lines.m4[i1]-JOB->res.lines.m1[i1]+1;
    if (j1>dy*120/100 || j1<dy*80/100) continue; // only most frequently
    j2+=j1; i++;
  } if (i>0 && j2/i>7) dy=j2/i;
  if( JOB->cfg.verbose&1 ) fprintf(stderr,"# insert space between words (dy=%d) ...",dy);
  if (!dy) dy=(JOB->res.avY)*110/100+1;

  i=0;
  for_each_data(&(JOB->res.boxlist)) {
    box2 =(struct box *)list_get_current(&(JOB->res.boxlist));
    cc=0;
    if (box2->line>maxline) {  // lines and chars must be sorted!
      if (maxline>=0) cc='\n'; // NL
      maxline=box2->line;
      lspc=spc*(JOB->res.lines.m4[maxline]-JOB->res.lines.m1[maxline]+1)/dy;
      // if( JOB->cfg.verbose&1 ) fprintf(stderr," %d",lspc);
    }
    if((box3 = (struct box *)list_prev(&(JOB->res.boxlist), box2))){
      if (maxline && !box2->line && cc==0) cc=' ';
      if (box2->line<=maxline && cc==0) {  // lines and chars must be sorted!
        i1=a0(box2->x0 - box3->x1);         if (i1 >= lspc-1) cc=' '; // SPC
        if (!monospaced && wisupper(box2->c) && i1 >= lspc-1) cc=' '; // better?
        /* ToDo: lspc should get a better value! (screenshots failed, mono?) */
      }
    }
    if(cc){
      box4=(struct box *)list_prev(&(JOB->res.boxlist), box2);
      box3=(struct box *)malloc(sizeof(struct box));
      box3->x0=box2->x0-2;       box3->x1=box2->x0-2;
      box3->y0=box2->y0;         box3->y1=box2->y1;
      if(cc!='\n' && box4)
	box3->x0=box4->x1+2;
      if(cc=='\n' || !box4)
	box3->x0=JOB->res.lines.x0[box2->line];
      if(cc=='\n' && box4){
        box3->y0=box4->y1;	// better use lines.y1[box2->pre] ???
        box3->y1=box2->y0;
      }
      box3->x =box2->x0-1;       box3->y=box2->y0;
      box3->dots=0;              box3->c=cc;
      box3->modifier='\0';
      box3->num=-1;        box3->line=box2->line;
      box3->m1=box2->m1;   box3->m2=box2->m2;
      box3->m3=box2->m3;   box3->m4=box2->m4;
      box3->p=pp;
      box3->num_ac=0;
      box3->obj=NULL;
      list_ins(&(JOB->res.boxlist),box2,box3);
       i++;
    }
  } end_for_each(&(JOB->res.boxlist));
  if( JOB->cfg.verbose&1 ) fprintf(stderr," found %d\n",i);
  return 0;
}


/*
   add infos where the box is positioned to the box
   this is useful for better recognition
*/
int  add_line_info(/* List *boxlist2 */){
  pix *pp=&JOB->src.p;
  struct tlines *lines = &JOB->res.lines;
  struct box *box2;
  int i,xx,m1,m2,m3,m4;
  if( JOB->cfg.verbose&1 ) fprintf(stderr,"# add line infos to boxes ...");
  for_each_data(&(JOB->res.boxlist)) {
    box2 =(struct box *)list_get_current(&(JOB->res.boxlist));
    for(i=0;i<JOB->res.lines.num;i++)
    {
      xx=(box2->x1+box2->x0)/2;
      m1=lines->m1[i]+lines->dy*xx/pp->x;
      m2=lines->m2[i]+lines->dy*xx/pp->x;
      m3=lines->m3[i]+lines->dy*xx/pp->x;
      m4=lines->m4[i]+lines->dy*xx/pp->x;
      // fprintf(stderr," test line %d m1=%d %d %d %d\n",i,m1,m2,m3,m4);
      if (m4-m1==0) continue; /* no text line (line==0) */
#if 0
      if( box2->y1+2*JOB->res.avY >= m1
       && box2->y0-2*JOB->res.avY <= m4 ) /* not to far away */
#endif
      if( box2->x0 >= lines->x0[i]  &&  box2->x1 <= lines->x1[i] )
      if( box2->m2==0 || abs(box2->y0-box2->m2) > abs(box2->y0-m2) )
      { /* found nearest line */
        box2->m1=m1;
        box2->m2=m2;
        box2->m3=m3;
        box2->m4=m4;
        box2->line=i;
      }
    }
    if( box2->y1+2*JOB->res.avY < box2->m1
     || box2->y0-2*JOB->res.avY > box2->m4 ) /* to far away */
    { /* reset */
        box2->m1=0;
        box2->m2=0;
        box2->m3=0;
        box2->m4=0;
        box2->line=0;
    }
  } end_for_each(&(JOB->res.boxlist));
  if( JOB->cfg.verbose&1 ) fprintf(stderr," done\n");
  return 0;
}


/*
 *  bring the boxes in right order
 *  add_line_info must be executed first!
 */
int sort_box_func (const void *a, const void *b) {
  struct box *boxa, *boxb;

  boxa = (struct box *)a;
  boxb = (struct box *)b;

  if ( ( boxb->line < boxa->line ) ||
       ( boxb->line == boxa->line && boxb->x0 < boxa->x0 ) )
    return 1;
  return -1;
}    

// -------------------------------------------------------------
// ------             use this for entry from other programs 
// include pnm.h pgm2asc.h 
// -------------------------------------------------------------
// entry point for gocr.c or if it is used as lib
// better name is call_ocr ???
// jb: OLD COMMENT: not removed due to set_options_* ()
// args after pix *pp should be removed and new functions
//   set_option_mode(int mode), set_option_spacewidth() .... etc.
//   should be used instead, before calling pgm2asc(pix *pp)
//   ! change if you can ! - used by X11 frontend
int pgm2asc(job_t *job)
{
  pix *pp;

  assert(job);
  /* FIXME jb: remove pp */
  pp = &(job->src.p);

  if( job->cfg.verbose ) 
    fprintf(stderr, "# db_path= %s\n", job->cfg.db_path);

  progress(0,0); /* start progress output 0% 0% */

  /* ----- count colors ------ create histogram -------
     - this should be used to create a upper and lower limit for cs
     - cs is the optimum gray value between cs_min and cs_max
     - also inverse scans could be detected here later */
  if (job->cfg.cs==0)
    job->cfg.cs=otsu( pp->p,pp->y,pp->x,0,0,pp->x,pp->y, job->cfg.verbose & 1 );
  thresholding( pp->p,pp->y,pp->x,0,0,pp->x,pp->y, job->cfg.cs );
  // FIXME jb: ask for sense
  job->cfg.cs=128+32;

  progress(5,0); /* progress is only estimated */

  /* FIXME jb: malloc */
  if ( job->cfg.verbose & 32 ) { 
    // generate 2nd imagebuffer for debugging output
    job->tmp.ppo.p = (unsigned char *)malloc(job->src.p.y * job->src.p.x); 	
    // buffer
    assert(job->tmp.ppo.p);
    copybox(&job->src.p,
            0, 0, job->src.p.x, job->src.p.y,
            &job->tmp.ppo,
            job->src.p.x * job->src.p.y);
  }
  
  /* load character data base */
  if ( job->cfg.mode&2 )
    load_db();

  /* this is first step for reorganize the PG
     ---- look for letters, put rectangular frames around letters
     letter = connected points near color F
     should be used by dust removing (faster) and line detection!
     ---- 0..cs = black letters, last change = Mai99 */
  
  progress(8,0); /* progress is only estimated */

  scan_boxes( pp );
  if ( !job->res.numC ){ 
    fprintf( stderr,"# no boxes found - stopped\n" );
    if ( job->cfg.verbose & 32 ) 
      writebmp( "out20.bmp", job->tmp.ppo , job->cfg.verbose ); 
      /* colored should be better */
    /***** should free stuff, etc) */
    return(1);
  }

  progress(10,0); /* progress is only estimated */
  // output_list( pp, job->cfg.lc);  // for debugging 
  // ToDo: matrix printer preprocessing


  remove_dust( job ); /* from the &(job->res.boxlist)! */
// output_list( pp, job->cfg.lc);  // for debugging 
  smooth_borders( job ); /* only for big chars */
  progress(12,0); /* progress is only estimated */
// output_list( pp, job->cfg.lc);  // for debugging 

  detect_barcode( job );  /* mark barcode */
// output_list( pp, job->cfg.lc);  // for debugging 

  detect_pictures( job ); /* mark pictures */
// output_list( pp, job->cfg.lc);  // for debugging 

  remove_pictures( job ); /* do this as early as possible, before layout */
 // output_list( pp, job->cfg.lc);  // for debugging

  detect_rotation_angle( job );

#if 1 		/* Rotate the whole picture! move boxes */
  if( job->res.lines.dy!=0 ){  // move down lowest first, move up highest first
    // in work! ??? (at end set dy=0) think on ppo!
  }
#endif
  detect_text_lines( pp, job->cfg.mode ); /* detect and mark JOB->tmp.ppo */
  progress(20,0); /* progress is only estimated */

  add_line_info(/* &(job->res.boxlist) */);
  if(job->cfg.verbose&32) write_img("out10.bmp",&job->tmp.ppo,pp,0);
  divide_vert_glued_boxes( pp, job->cfg.mode); /* after add_line_info, before list_sort! */
//  if(env.job->cfg.verbose&32) write_img("out10.bmp",&ppo,pp,0);

  remove_melted_serifs( pp ); /* make some corrections on pixmap */
  /* list_ins seems to sort in the boxes on the wrong place ??? */
  list_sort(&(job->res.boxlist), sort_box_func);
  // ----------- write out10.pgm -----------
  // if(job->cfg.verbose&32) write_img("out10.bmp",&job->tmp.ppo,pp,0);

  glue_broken_chars( pp );

  remove_rest_of_dust( );

  measure_pitch( );

  if(job->cfg.mode&64) find_same_chars( pp );
  progress(30,0); /* progress is only estimated */
  // if(job->cfg.verbose&32) write_img("out10.bmp",&job->tmp.ppo,pp,0);

  char_recognition( pp, job->cfg.mode);
  progress(60,0); /* progress is only estimated */

#define BlownUpDrawing 1     /* german: Explosionszeichnung, temporarly */
#if     BlownUpDrawing == 1  /* german: Explosionszeichnung */
{ /* just for debugging */
  int i,ii,ni; struct box *box2;
  i=ii=ni=0;
  for_each_data(&(JOB->res.boxlist)) { /* count boxes */
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if (box2->c==UNKNOWN)  i++;
    if (box2->c==PICTURE) ii++;
    ni++;
  } end_for_each(&(JOB->res.boxlist)); 
  if(JOB->cfg.verbose)
    fprintf(stderr,"# debug: unknown= %d picts= %d boxes= %d\n",i,ii,ni);
}
#endif
  // ----------- write out20.pgm -----------
  if(job->cfg.verbose&32) write_img("out20.bmp",&job->tmp.ppo,pp,1);

  compare_unknown_with_known_chars( pp, job->cfg.mode);
  progress(70,0); /* progress is only estimated */

  try_to_divide_boxes( pp, job->cfg.mode);
  progress(80,0); /* progress is only estimated */

  /* --- list output ---- for debugging --- */
  if( job->cfg.verbose&6 ) output_list( pp, job->cfg.lc);

  if ( job->cfg.spc==0 ){
    if ( monospaced )
      job->cfg.spc = pitch_mono; 
    else
      job->cfg.spc = pitch_prop; /* old: (job->res.avX+18) / 4; */
    if(job->cfg.verbose) fprintf(stderr,"# set space width to %d\n", job->cfg.spc);
  }

  /* ---- insert spaces ---- */
  list_insert_spaces( pp , job->cfg.spc );

  // ---- proof difficult chars Il1 by context view ----
  if(!(job->cfg.mode&32)) context_correction( pp );
  
  store_boxtree_lines( job->cfg.mode );
  progress(90,0); /* progress is only estimated */

/* 0050002.pgm.gz ca. 109 digits, only 50 recognized (only in lines?)
 * ./gocr -v 39 -m 56 -e - -m 4 -C 0-9 -f XML tmp0406/0050002.pbm.gz
 *  awk 'BEGIN{num=0}/1<\/box>/{num++;}END{print num}' o
 * 15*0 24*1 18*2 19*3 15*4 6*5 6*6 6*7 4*8 8*9 sum=125digits counted boxes
 *  9*0 19*1 14*2 15*3 11*4 6*5 5*6 6*7 4*8 8*9 sum=97digits recognized
 * 1*1 1*7 not recognized (Oct04)
 *  33*SPC 76*NL = 109 spaces + 36*unknown sum=241 * 16 missed
 */
#if     BlownUpDrawing == 1  /* german: Explosionszeichnung */
{ /* just for debugging */
  int i,ii,ni; struct box *box2; const char *testc="0123456789ABCDEFGHIJK";
    i=ii=ni=0;
  for_each_data(&(JOB->res.boxlist)) { /* count boxes */
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if (box2->c==UNKNOWN)  i++;
    if (box2->c==PICTURE) ii++;
    if (box2->c>' ' && box2->c<='z') ni++;
  } end_for_each(&(JOB->res.boxlist)); 
  if(JOB->cfg.verbose)
    fprintf(stderr,"# debug: (_)= %d picts= %d chars= %d",i,ii,ni);
  for (i=0;i<20;i++) {
    ni=0;
    for_each_data(&(JOB->res.boxlist)) { /* count boxes */
      box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
      if (box2->c==testc[i]) ni++;
    } end_for_each(&(JOB->res.boxlist)); 
    if(JOB->cfg.verbose && ni>0)
      fprintf(stderr," (%c)=%d",testc[i],ni);
  }
  if(JOB->cfg.verbose)
    fprintf(stderr,"\n");
}
#endif

  // ---- frame-size-histogram
  // ---- (my own defined) distance between letters
  // ---- write internal picture of textsite
  // ----------- write out30.pgm -----------
  if( job->cfg.verbose&32 ) write_img("out30.bmp",&job->tmp.ppo,pp,2);
    
  progress(100,0); /* progress is only estimated */

  return 0; 	/* what should I return? error-state? num-of-chars? */
}
