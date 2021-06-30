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

 see README for EMAIL address
*/

#include <string.h>
#include "unicode.h"
#include "output.h"
#include "pcx.h"

/* function is only for debugging and for developing
   it prints out a part of pixmap b at point x0,y0 to stderr
   using dots .,; if no pixel, and @xoO for pixels
   modify n_run and print out what would happen on 2nd, 3th loop!
   new: output original and copied pixmap in the same figure
 */
void out_b(struct box *px, pix *b, int x0, int y0, int dx, int dy, int cs ){
  int x,y,x2,y2,yy0,tx,ty,n1,i;
  char c1='.';
  yy0=y0;
  if(px){ /* overwrite rest of arguments */
    if (!b) {
      b=px->p;
      x0=px->x0; dx=px->x1-px->x0+1;
      y0=px->y0; dy=px->y1-px->y0+1; yy0=y0;
    }
    if(cs==0) cs=JOB->cfg.cs;
    fprintf(stderr,"\n# list box dots=%d c=%s ac=%s mod=%s line=%d m= %d %d %d %d r= %d %d",
	  px->dots, decode(px->c,ASCII), decode(px->ac,ASCII),
	  decode(px->modifier,ASCII), px->line,
	  px->m1 - px->y0, px->m2 - px->y0, px->m3 - px->y0, px->m4 - px->y0,
	  px->x - px->x0, px->y - px->y0);
    /* output the object-string (picture position, barcodes, glyphs, ...) */
    if (px->obj) fprintf(stderr,"\n# list box object=%s",px->obj);
    if (px->num_ac){ /* output table of chars and its probabilities */
      fprintf(stderr,"\n# list box char: ");
      for(i=0;i<px->num_ac && i<NumAlt;i++)
         fprintf(stderr," %s(%d)",decode(px->tac[i],ASCII),px->wac[i]);
    }
    fprintf(stderr,"\n");
    if (px->dots && px->m2 && px->m1<y0) { yy0=px->m1; dy=px->y1-yy0+1; }
  }
  tx=dx/80+1;
  ty=dy/40+1; // step, usually 1, but greater on large maps 
  fprintf(stderr,"# list pattern   x=%4d %4d d=%3d %3d t=%d %d\n",x0,y0,dx,dy,tx,ty);

  for(y=yy0;y<yy0+dy;y+=ty) { // reduce the output to max 78x40
    for(x=x0;x<x0+dx;x+=tx){
      n1=0; c1='.';
      for(y2=y;y2<y+ty && y2<y0+dy && n1==0;y2++) /* Mai2000 */
      for(x2=x;x2<x+tx && x2<x0+dx && n1==0;x2++)
#if 0
      if((pixel(b,x2,y2)<cs)){ n1++; c1='@' }
#else
      {
        if((pixel(b,x2,y2)<cs)){ n1++; c1='@'; }
        if (px) { /* oO if pixmaps differ */
          if((pixel(px->p,x2-x0+px->x0,
                          y2-y0+px->y0)<cs)){ if (c1!='@') c1='o'; }
          else                              { if (c1!='.') c1='O'; }
        }
#if 0
        if(JOB->tmp.n_run==0){
          JOB->tmp.n_run++; if(!n1) if(pixel(b,x2,y2)<cs){ n1= 9; }
          JOB->tmp.n_run++; if(!n1) if(pixel(b,x2,y2)<cs){ n1=10; }
          JOB->tmp.n_run++; if(!n1) if(pixel(b,x2,y2)<cs){ n1=11; }
          JOB->tmp.n_run=0;
          if(!n1) if(pixel(b,x2,y2)<cs+20){ n1=7; }
          if(!n1) if(pixel(b,x2,y2)<cs+40){ n1=1; }
        }
#endif
      }
#endif
      fprintf(stderr,"%c", c1 );
    }
    if ( dx>0 ){
      if (px) if (y==px->m1 || y==px->m2 || y==px->m3 || y==px->m4)
        fprintf(stderr,"<");  /* line marks */
      if (y==y0 || y==yy0+dy-1)
        fprintf(stderr,"-");  /* boxmarks */
      fprintf(stderr,"\n");
    }
  }
}

/* same as out_b, but for faster use, only a box as argument
 */
void out_x(struct box *px) {
  out_b(px,NULL,0, 0, 0, 0, JOB->cfg.cs);
}


/* print out two boxes side by side, for debugging comparision algos */
void out_x2(struct box *box1, struct box *box2){
  int x,y,i,tx,ty,dy;
  /*FIXME jb static*/static char *c1="OXXXXxx@.,,,,,,,";
  pix *b=&JOB->src.p;
  dy=(box1->y1-box1->y0+1);
  if(dy<box2->y1-box2->y0+1)dy=box2->y1-box2->y0+1;
  tx=(box1->x1-box1->x0)/40+1;
  ty=(box1->y1-box1->y0)/40+1; // step, usually 1, but greater on large maps 
  if(box2)fprintf(stderr,"\n# list 2 patterns");
  for(i=0;i<dy;i+=ty) { // reduce the output to max 78x40???
    fprintf(stderr,"\n"); y=box1->y0+i;
    for(x=box1->x0;x<=box1->x1;x+=tx) 
    fprintf(stderr,"%c", c1[ ((pixel(b,x,y)<JOB->cfg.cs)?0:8)+marked(b,x,y) ] );
    if(!box2) continue;
    fprintf(stderr,"  "); y=box2->y0+i;
    for(x=box2->x0;x<=box2->x1;x+=tx)
    fprintf(stderr,"%c", c1[ ((pixel(b,x,y)<JOB->cfg.cs)?0:8)+marked(b,x,y) ] );
  }
}


/* ---- list output ---- for debugging --- */
int output_list(pix * pp, char *lc) {
  int i = 0, cs = JOB->cfg.cs;
  struct box *box2;

  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *) list_get_current(&(JOB->res.boxlist));
    if (!lc || (strchr(lc, box2->c) && box2->c < 256)
            || (strchr(lc, '_') && box2->c==UNKNOWN)) { // for compability
      if (!pp) pp=box2->p;
      fprintf(stderr,
	      "\n# list shape %3d x=%4d %4d d=%3d %3d h=%d o=%d dots=%d %04x %s",
	      i, box2->x0, box2->y0,
	      box2->x1 - box2->x0 + 1,
	      box2->y1 - box2->y0 + 1,
	      num_hole(box2->x0, box2->x1, box2->y0, box2->y1, pp, cs,NULL),
	      num_obj( box2->x0, box2->x1, box2->y0, box2->y1, pp, cs),
	      box2->dots, (int)box2->c,   /* wchar_t -> char ???? */
	      decode(box2->c,ASCII) );
      if (JOB->cfg.verbose & 4) {
	out_x(box2);
      }
    }
    i++;
  } end_for_each(&(JOB->res.boxlist));
  return 0;
}

/* --- output of image incl. corored lines usefull for developers ---
 * color/gray:  0x10=red, 0x20=blue, 0x40=green???
 * opt: 1 - mark unknown boxes red       (first pass)
 *      2 - mark unknown boxes more red  (final pass)
 *      4 - mark lines blue
 *      8 - reset coloring ??? obsolete
 */
int write_img(char *fname, pix * ppo, pix * p, int opt) {
  struct box *box2;
  int x, y, ic;

  if( opt & 8 )		/* refresh debug image */
    for(y=0;y<p->y;y++)
      for(x=0;x<p->x;x++)
        ppo->p[x+(p->x)*y]=p->p[x+(p->x)*y]&0xC0;

  /* for(x=0;x<ppo->x;x++)if((x&35)>32)
     put(ppo,x,lines.longest_line+dy*x/ppo.x,255,0x40); */

#if 0	/* struct tlines lines is not declared here, find elegant way */
  if( opt & 4 )
  {
      struct tlines *lines = &JOB->res.lines;
      dy = lines->dy;
      for (i = 0; i < lines->num; i++) {	// mark lines
	for (x = lines->x0[i]; x < lines->x1[i]; x++) {
	  y = lines->m1[i];
	  if ((x & 7) == 4)    put(&ppo, x, y + dy * x / ppo.x, 255, 32);
	  y = lines->m2[i];
	  if ((x & 3) == 2)    put(&ppo, x, y + dy * x / ppo.x, 255, 32);
	  y = lines->m3[i];
	  if ((x & 1) == 1)    put(&ppo, x, y + dy * x / ppo.x, 255, 32);
	  y = lines->m4[i];
	  if ((x & 7) == 4)    put(&ppo, x, y + dy * x / ppo.x, 255, 32);
	}
      }
  }
#endif   

  ic = ((opt & 2) ? 1 : 2);
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *) list_get_current(&(JOB->res.boxlist));
    /* mark boxes in 32=0x40=blue */
    if (box2->c != ' ' && box2->c != '\n') {
      for (y = box2->y0; y <= box2->y1; y += ic) 
	ppo->p[box2->x0 + y * p->x] |= 32;
      for (y = box2->y0; y <= box2->y1; y += ic)
	ppo->p[box2->x1 + y * p->x] |= 32;
      for (x = box2->x0; x <= box2->x1; x += ic)
	ppo->p[x + box2->y0 * p->x] |= 32;
      for (x = box2->x0; x <= box2->x1; x += ic)
	ppo->p[x + box2->y1 * p->x] |= 32;
      /* mark unknown chars by 0x20=red background */
      if (box2->c == UNKNOWN  && (opt & 3))
	for (y = box2->y0 + 1; y < box2->y1; y++)
	  for (x = box2->x0 + 1; x < box2->x1; x++)
	    if ((1 & (x + y)) != 0 || ic == 1)
	      ppo->p[x + y * p->x] |= 16;
      /* mark pictures by blue cross */
      if (box2->c == PICTURE)
        for (x = 0; x < box2->x1-box2->x0+1; x++){
           y=(box2->y1-box2->y0+1)*x/(box2->x1-box2->x0+1);
	   ppo->p[(box2->x0+x) + (box2->y0+y) * p->x] |= 32;
	   ppo->p[(box2->x1-x) + (box2->y0+y) * p->x] |= 32;
	}
    }
  } end_for_each(&(JOB->res.boxlist));
  if( strstr(fname,".pgm") ) writepgm(fname,ppo);
  else writebmp(fname, *ppo, JOB->cfg.verbose);	// colored should be better
  return 0;
}
