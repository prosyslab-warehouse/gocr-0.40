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

   OCR engine (c) Joerg Schulenburg
   first engine: rule based --- numbers 0..9

*/

#include <stdlib.h>
#include <stdio.h>
// #include "pgm2asc.h"
#include "ocr0.h"
#include "ocr1.h"
#include "amiga.h"
#include "pnm.h"
#include "gocr.h"

/* only for debugging and development */
#define MX	{ out_x(box1);fprintf(stderr," mark %d\n",__LINE__); }
#define MM	{ fprintf(stderr," mark %d\n",__LINE__); }

/* extern "C"{ */

// OCR engine ;)
wchar_t ocr0n(ocr0_shared_t *sdata){
   struct box *box1=sdata->box1;
   pix  *bp=sdata->bp;
   int	d,x,y,x0=box1->x0,x1=box1->x1,y0=box1->y0,y1=box1->y1;
   int  dx=x1-x0+1,dy=y1-y0+1,cs=sdata->cs;	// size
   int  xa,xb,ya,yb, /* tmp-vars */
        i1,i2,i3,i4,i,j;
   wchar_t bc=UNKNOWN;				// bestletter
   // ad,ac will be used in future
   wchar_t ac=UNKNOWN;				// alternative char (2nd best)
   int  ad=0;		// propability 0..100
   int hchar=sdata->hchar;	// char is higher than 'e'
   int gchar=sdata->gchar;	// char has ink lower than m3
   int dots=box1->dots;
   // --- test 5 near S ---------------------------------------------------
   for(ad=d=100;dx>2 && dy>4;){     // min 3x4
      if (sdata->holes.num > 1) break; /* be tolerant */
      if( num_cross(  dx/2,  dx/2,0,dy-1,bp,cs)!=3
      &&  num_cross(5*dx/8,3*dx/8,0,dy-1,bp,cs)!=3 ) break;
      // get the upper and lower hole koords, y around dy/4 ???
      x=5*dx/8;
      y  =loop(bp,x,0,dy,cs,0,DO); if(y>dy/8) break;
      y +=loop(bp,x,y,dy,cs,1,DO); if(y>dy/4) break;
      i1 =loop(bp,x,y,dy,cs,0,DO)+y; if(i1>5*dy/8) break;
      i3=y=(y+i1)/2; // upper end can be shifted to the right for italic
      x  =loop(bp,0,y,dx,cs,0,RI); if(x>4*dx/8) break;
      x +=loop(bp,x,y,dx,cs,1,RI); if(x>5*dx/8) break;
      i1 =loop(bp,x,y,dx,cs,0,RI); i1=(i1+2*x)/2; // upper center x
      y=11*dy/16;
      x  =loop(bp,dx-1  ,y,dx,cs,0,LE); if(x>dx/4) break;
      x +=loop(bp,dx-1-x,y,dx,cs,1,LE); if(x>dx/2) break;
      i2 =loop(bp,dx-1-x,y,dx,cs,0,LE); i2=dx-1-(i2+2*x)/2; // lower center x

// MX; fprintf(stderr,"i1,i3=%d,%d i2=%d\n",i1,i3,i2); MM;

      y  =loop(bp,i1,0,dy,cs,0,DO);
      y +=loop(bp,i1,y,dy,cs,1,DO);
      y  =(3*y+i3)/4;
      if( num_cross( i1, dx-1, y, y,bp,cs)>0 ){ /* S or serif5 ? */
        y  =loop(bp,i1  ,i3,dy,cs,0,DO);
        i  =loop(bp,i1-1,i3,dy,cs,0,DO);
        if (y>i ) break; /* looks like S */
        y  =loop(bp,i1  ,i3,dy,cs,0,UP);
        i  =loop(bp,i1+1,i3,dy,cs,0,UP);
        if (i<y ) break; /* looks like S */
        x  =loop(bp,dx-1,0,dx,cs,0,LE);
        i  =loop(bp,dx-1,1,dx,cs,0,LE);
        if (x>i ) break; /* looks like S */
        if( num_cross(   0, dx/2, dy-1, dy-1,bp,cs)>1 
         && num_cross( dx/2,dx-1,    0,    0,bp,cs)>1 ) break; /* serifs */
        if ( loop(bp,0,dy-1,dx,cs,0,RI)==0 ) break;

      }

      for(y=dy/4;y<3*dy/4;y++) // right gap?
      if( num_cross(i1,dx-1,dy/4,y,bp,cs)==0 ) break;
      if( y==3*dy/4 ) break;

      for(y=dy/4;y<=11*dy/16;y++) // left gap?
      if( num_cross(0,i2,y,11*dy/16,bp,cs)==0 ) break;
      if( y>11*dy/16 ) break;

      // if( num_hole( x0, x1, y0, y1, box1->p,cs,NULL) > 0 ) break;
      if (sdata->holes.num>0) break;

      // sS5 \sl z  left upper v-bow ?
      for(x=dx,i=y=dy/4;y<dy/2;y++){
        j=loop(bp,0,y,dx,cs,0,RI); if(j<x) { x=j; i=y; }
      } y=i;
      i1=loop(bp,0,   dy/16     ,dx,cs,0,RI);
      i2=loop(bp,0,(y+dy/16)/2  ,dx,cs,0,RI);
      i =loop(bp,0,(y+dy/16)/2+1,dx,cs,0,RI); if( i>i2 ) i2=i;
      i3=loop(bp,0, y  ,dx,cs,0,RI);
      i =loop(bp,0, y-1,dx,cs,0,RI); if( i<i3 ) i3=i;
      if( 2*i2+1+dx/16 < i1+i3 ) break;
      
      if( dy>=20 && dx<16 ) /* tall S */
      if(  loop(bp,0,   dy/5     ,dx,cs,0,RI)
         ==loop(bp,0,   dy/4     ,dx,cs,0,RI)
         &&
           loop(bp,0,   dy/10    ,dx,cs,0,RI)
          >loop(bp,0,   dy/4     ,dx,cs,0,RI)
         &&
           loop(bp,0,       1    ,dx,cs,0,RI)
          >loop(bp,0,   dy/4     ,dx,cs,0,RI)+1
         &&
           loop(bp,dx-1,    0    ,dx,cs,0,LE)
          >loop(bp,dx-1,    1    ,dx,cs,0,LE) ) break;

      if( dy>=30 && dx>15 ) /* large S */
      if(   loop(bp,dx/4,3*dy/10,dy,cs,1,DO)>0 ) // check start
      if(   loop(bp,dx-2,3*dy/4 ,dy,cs,1,UP)>0 ) // check end
      if( num_cross(dx/4,dx-2,3*dy/10,3*dy/4,bp,cs)==1 ) break; // connected?

      if( dy>17 && dx>9 ) /* S */
      if(   loop(bp,   0,dy/2   ,dx,cs,0,RI)<dx/2
        ||  loop(bp,   0,dy/2-1 ,dx,cs,0,RI)<dx/2 )
      if(   loop(bp,dx/4,3*dy/10,dy,cs,1,DO)>0 ) // check start
      if(   loop(bp,dx-2,2*dy/3 ,dy,cs,1,UP)>0 ) // check end
      if( num_cross(dx/4,dx-2,3*dy/10,2*dy/3,bp,cs)==1 ) ad=80; // connected?
      if( dx>7 && loop(bp,dx-1-dx/8,0,dy,cs,0,DO)>dy/8 ) ad=90*ad/100;

      if ( gchar) ad=98*ad/100;
      if (!hchar) ad=98*ad/100;
      setac(box1,(wchar_t)'5',ad);
      if (ad==100) return '5';
      break;
      
   }
   // --- test 1 ---------------------------------------------------
   for(ad=d=100;dy>4 && dy>dx && 2*dy>box1->m3-box1->m2;){     // min 3x4
      if( dots==1 ) break;
      if (sdata->holes.num > 1) break; /* be tolerant */

      if( num_cross(0, dx-1, 0  , 0  ,bp,cs) != 1
       && num_cross(0, dx-1, 1  , 1  ,bp,cs) != 1 ) break;
      if( num_cross(0, dx-1,dy/2,dy/2,bp,cs) != 1 ) break;
      if( num_cross(0, dx-1,dy-1,dy-1,bp,cs) != 1
       && num_cross(0, dx-1,dy-2,dy-2,bp,cs) != 1 ) break;

      i4=0; // human font
      if( num_cross(0, dx-1,3*dy/4,3*dy/4,bp,cs) != 2 ) { // except ocr-a
        for( y=1; y<dy/2; y++ ){
          if( num_cross(0, dx-1, y  , y  ,bp,cs) == 2 ) break;
        } if (y>=dy/2) ad=98*ad/100;
        for( i=dy/8,y=7*dy/16;y<dy-1 && i;y++ ){
          if( num_cross(0, dx-1, y  , y  ,bp,cs) != 1 ) i--;
        } if( !i ) break;
      } else {  // ocr-a-1
       /*  @@@..
           ..@..
           ..@..
           ..@..
           ..@.@
           ..@.@
           @@@@@  */
        i=  loop(bp,dx/2,0,dy,cs,0,DO);
        if (loop(bp,dx/2,i,dy,cs,1,DO)<dy-1) break;
        i=  loop(bp,dx  -1,dy-1-dy/16,dx,cs,0,LE);
        if (loop(bp,dx-i-1,dy-1-dy/16,dx,cs,1,LE)<dx-1) break;
        i=  loop(bp,0,dy/16,dx,cs,0,RI);
        if (loop(bp,i,dy/16,dx,cs,1,RI)<dx/2) break;
        i4=1;
      }

      if( num_cross(0, dx-1, 0  , 0  ,bp,cs) > 1
       && num_cross(0, dx-1, 1  , 1  ,bp,cs) > 1 ) break; // ~/it_7

      // calculate upper and lower mass center (without lower serif)

      x =loop(bp,0,7*dy/8-1,dx,cs,0,RI);   i2=x;
      x+=loop(bp,x,7*dy/8-1,dx,cs,1,RI)-1; i2=(i2+x)/2;

      i1=loop(bp,dx-1  ,1+0* dy/4,dx,cs,0,LE); i1=dx-1-i1-(x-i2)/2;

      x =(i1-i2+4)/8; i1+=x; i2-=x;
      
      if( get_line2(i1,0,i2,dy-1,bp,cs,100)<95 ) { // dont work for ocr-a-1
        i1=loop(bp,dx-1  ,1+0* dy/4,dx,cs,0,LE); i1=dx-1-i1;
        if( get_line2(i1,0,i2,dy-1,bp,cs,100)<95 ) break;
      }
      // upper and lower width
      x =loop(bp,(i1+i2)/2,dy/2,dx,cs,1,RI); i=x; i3=0;
      for(y=0;y<7*dy/8;y++)
        if( loop(bp,i1+y*(i2-i1)/dy, y,dx,cs,1,RI)-i > 1+dx/8 ) break;
      if(y<7*dy/8) ad=98*ad/100; // serif or ocr-a-1 ?
      if(y<6*dy/8) break;
// out_x(box1); printf(" i12=%d %d\n",i1,i2);
      x =loop(bp,i2,dy-1,dx,cs,1,LE); j=x;
      x =loop(bp,i2,dy-2,dx,cs,1,LE); if(x>j)j=x; i=j;
      x =loop(bp,i2,dy-1,dx,cs,1,RI); j=x;
      x =loop(bp,i2,dy-2,dx,cs,1,RI); if(x>j)j=x;
      if(abs(i-j)>1+dx/8) i3|=1;
      if(i3) break;        
//       out_x(box1);printf(" 11 i=%d j=%d i2=%d dx=%d\n",i,j,i1,dx);
      for(i=dx,j=y=0;y<7*dy/16;y++){
        x =loop(bp,0,y,dx,cs,0,RI); if(x<i) { i=x;j=y; }
      }  
      if ( i1-i<7*dx/16 ) ad=ad*98/100;
      if ( i1-i<6*dx/16 ) break; // 4*dx/8 => 7*dx/16
      x =loop(bp,0,dy/2,dx,cs,0,RI);
      j =loop(bp,x,dy/2,dx,cs,1,RI); if( j>x+(dy+16)/32 ) break; // ~l
      x =loop(bp,0,0,dx,cs,0,RI); // straight line ???
      j =loop(bp,0,1,dx,cs,0,RI);               if( j>x ) break; // ~l
      if( x==j ) j =loop(bp,0,dy/8,dx,cs,0,RI); if( j>x && !i4) break;
      if( x==j ) if( loop(bp,0,dy/4,dx,cs,0,RI)>x ) break;  // ~l
      x=j;
//      j =loop(bp,0,2,dx,cs,0,RI); if( j>=x ) break; x=j; // ~l
//      j =loop(bp,0,   0,dx,cs,0,DO); if( !j  ) break; // ~7
      if( !hchar ) // ~ right part of n
      if( loop(bp,dx-1,   1,dx,cs,0,LE)-dy/6
        > loop(bp,dx-1,dy/4,dx,cs,0,LE)
       || get_bw(x1+1,x1+2,y0,y0+dy/8,box1->p,cs,1)==1 ) break; // Mai00
      if( loop(bp,dx-1,3*dy/4,dx,cs,0,LE) > dx/2
       && get_bw(x1-dx/4,x1,y1-1,y1,box1->p,cs,1)==1 ) break; // ~z Jun00

      i=loop(bp,  dx/8,0,dy,cs,0,DO);
      for (y=dy,x=dx/2;x<3*dx/4;x++){ /* get upper end */
        j=loop(bp,x,0,dy,cs,0,DO); if (j<y) { y=j; }
      }
      if(y<dy/2 && y+dy/16>=i) ad=97*ad/100; // ~\tt l ??? ocr-a_1

      if(   loop(bp,   0,  dy/8,dx,cs,0,RI)
       -(dx-loop(bp,dx-1,7*dy/8,dx,cs,0,LE)) > dx/4 ) break; // ~/

      i=    loop(bp,   0,      0,dy,cs,0,DO); // horizontal line?
      if(dy>=12 && i>dy/8 && i<dy/2){
        if(   loop(bp,dx-1,3*dy/16,dx,cs,0,LE)-dx/8
             >loop(bp,dx-1,      i,dx,cs,0,LE) 
         ||   loop(bp,dx-1,3*dy/16,dx,cs,0,LE)-dx/8
             >loop(bp,dx-1,    i+1,dx,cs,0,LE)       ) break; // ~t,~f
        if(   loop(bp,   0,dy-1-dy/32,dx,cs,0,RI)-dx/8
             >loop(bp,   0,    3*dy/4,dx,cs,0,RI) 
         &&   loop(bp,dx-1,    3*dy/4,dx,cs,0,LE)-dx/8
             >loop(bp,dx-1,dy-1-dy/32,dx,cs,0,LE)    ) break; // ~t
        if(   loop(bp,   0,i-1,dx,cs,0,RI)>1 && dx<6) {
          ad=99*ad/100; 
          if ( loop(bp,dx-1,i-1,dx,cs,0,LE)>1 ) break; // ~t
        }
      }

      if (dx>8){
        if (loop(bp,0,3*dy/4,dx,cs,0,RI)-
            loop(bp,0,dy/2-1,dx,cs,0,RI)>dx/4) ad=95*ad/100; // ~3
        if (loop(bp,dx-1,dy/2-1,dx,cs,0,LE)-
            loop(bp,dx-1,3*dy/4,dx,cs,0,LE)>dx/8) ad=95*ad/100; // ~3
        if (loop(bp,dx-1, dy/16,dx,cs,0,LE)-
            loop(bp,dx-1,  dy/4,dx,cs,0,LE)>dx/8) ad=95*ad/100; // ~23
      }
      /* font 5x9 "2" recognized as "1" */
      i=loop(bp,dx-1-dx/8,dy-1,dy,cs,0,UP);
      if (i<=dy/4) {
        i+=loop(bp,dx-1-dx/8,dy-1-i,dy,cs,1,UP);
        if (i<=dy/4) {
          i=loop(bp,dx-1-dx/8,dy-1-i,dy,cs,0,LE);
          if (2*i>=dx && loop(bp,dx/4,0,dy,cs,0,DO)<dy/2) {
            if (dx<17) ad=98*ad/100;
            if (dx<9)  ad=97*ad/100;
          }
        }
      }
      
      // looking for  ###
      //              ..# pattern
      for (i=dx,y=0;y<dy/2;y++) {
        j=loop(bp,0,y,dx,cs,0,RI); if (j<i) i=j;
        if (j>i) break;
      } if (y>=dy/2) ad=98*ad/100;
      
      if (box1->m3 && !hchar) ad=98*ad/100;
      if (gchar) ad=98*ad/100;

      setac(box1,(wchar_t)'1',ad);
      break;
   }
   // --- test 2 ---------------------------------------------------
   for(ad=d=100;dx>2 && dy>4;){     // min 3x4
      if (sdata->holes.num > 1) break; /* be tolerant */
      if( get_bw(x0+dx/2, x0+dx/2 , y1-dy/5, y1     ,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0+dx/2, x0+dx/2 , y0     , y0+dy/5,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0+dx/8, x1-dx/3 , y1-dy/3, y1-dy/3,box1->p,cs,1) != 1 ) break;

      if( get_bw(x1-dx/3, x1      , y0+dy/3 , y0+dy/3,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0     , x0+dx/ 8, y1-dy/16, y1     ,box1->p,cs,1) != 1 ) break;
      if( num_cross(x0, x1-dx/8, y0+dy/2, y0+dy/2,box1->p,cs) != 1 ) break;
      if( get_bw(x0, x0+dx/9 , y0       , y0       ,box1->p,cs,1) == 1
       && get_bw(x0, x0+dx/2 ,y0+3*dy/16,y0+3*dy/16,box1->p,cs,1) == 1 ) break;
      if( get_bw(x0, x0+dx/9 , y0       , y0       ,box1->p,cs,1)
       != get_bw(x1-dx/9, x1 , y0       , y0       ,box1->p,cs,1) ) break;
      // out_x(box1);

      for( x=x0+dx/4;x<x1-dx/6;x++ )		// C
      if( num_cross( x, x, y0, y1,box1->p,cs) == 3 ) break;
      if( x>=x1-dx/6 ) break;

      for(i=1,y=y0;y<y0+dy/2;y++ )
      if( num_cross( x0, x1, y, y,box1->p,cs) == 2 ) i=0;
      if( i ) ad=99*ad/100; // ToDo: ocr-a-2 should have 100%

      for(i=1,y=y0+dy/5;y<y0+3*dy/4;y++ )
      if( get_bw( x0, x0+dx/3, y, y,box1->p,cs,1) == 0 ) i=0;
      if( i ) break;

      x=x1-dx/3,y=y1; /* center bottom */
      turmite(box1->p,&x,&y,x0,x1,y0,y1,cs,UP,ST); if( y<y1-dy/5 ) break;
      turmite(box1->p,&x,&y,x0,x1,y0,y1,cs,ST,UP); if( y<y1-dy/4 ) ad=99*ad/100;
                                                   if( y<y1-dy/3 ) break;
      turmite(box1->p,&x,&y,x0,x1,y0,y1,cs,UP,ST); if( y<y0+dy/3 ) break; y++;
      turmite(box1->p,&x,&y,x0,x1,y0,y1,cs,RI,ST);
      if( x<x1 ){ x--; // hmm thick font and serifs
        turmite(box1->p,&x,&y,x0,x1,y0,y1,cs,UP,ST); if( y<y0+dy/2 ) break; y++;
        turmite(box1->p,&x,&y,x0,x1,y0,y1,cs,RI,ST);
        if( x<x1 ) break;
      }

      // test ob rechte Kante ansteigend
      for(x=0,y=dy/18;y<=dy/3;y++){ // rechts abfallende Kante/Rund?
        i=loop(box1->p,x1,y0+y,dx,cs,0,LE); // use p (not b) for broken chars
        if( i<x ) break;	// rund
        if( i>x ) x=i;
      }
      if (y>dy/3 ) break; 	// z

      // hole is only allowed in beauty fonts
      // if( num_hole( x0, x1,      y0,      y1,box1->p,cs,NULL) > 0 ) // there is no hole
      // if( num_hole( x0, x0+dx/2, y0, y0+dy/2,box1->p,cs,NULL) == 0 ) // except in some beauty fonts
      if (sdata->holes.num>0)
      if (sdata->holes.hole[0].x1 >= dx/2 || sdata->holes.hole[0].y1 >= dy/2)
        break;

      i1=loop(bp,dx-1-dx/16,0,dy,cs,0,DO);  // Jul00
      i2=loop(bp,     dx/ 2,0,dy,cs,0,DO); if( i2+dy/32>=i1 ) break; // ~z
      i1=loop(bp,dx-1,dy-3*dy/16,dx,cs,0,LE);
      i2=loop(bp,   0,dy-3*dy/16,dx,cs,0,RI); if( i2>i1 ) ad=98*ad/100; // ~i
      if (dots) ad=98*ad/100; // i
      if (loop(bp,dx-1,dy-1-dy/16,dx,cs,0,LE)>dx/4) ad=96*ad/100; // \it i

      if ((!hchar) && box1->m4!=0) ad=80*ad/100;
      setac(box1,(wchar_t)'2',ad);
      if (ad==100) return '2';
      break;
   }
   // --- test 3 -------
   for(ad=d=100;dx>4 && dy>3;){	// dy<=dx nicht perfekt! besser mittleres
				// min-suchen fuer m
      if (sdata->holes.num > 1) break; /* be tolerant */
      // if( get_bw(x0+dx/2,x0+dx/2,y0,y0+dy/4,box1->p,cs,1) == 0 ) break; // ~4
      // if( get_bw(x0+dx/2,x0+dx/2,y1-dy/8,y1,box1->p,cs,1) == 0 ) break; // ~4
      // if( num_cross(x0+dx/2,x0+dx/2,y0     ,y1,box1->p,cs) < 2 ) break;
      // if( num_cross(x0+dx/4,x0+dx/4,y1-dy/2,y1,box1->p,cs) == 0 ) break;
      if( get_bw(dx/2,dx/2,        0,dy/6,bp,cs,1) == 0 ) break; // ~4
      if( get_bw(dx/2,dx-1,     dy/6,dy/6,bp,cs,1) == 0 ) break; // ~j
      if( get_bw(dx/2,dx/2,dy-1-dy/8,dy-1,bp,cs,1) == 0 ) break; // ~4
      if( num_cross(dx/2,dx/2,0        ,dy-1,bp,cs) < 2 ) break;
      if( num_cross(dx/4,dx/4,dy-1-dy/2,dy-1,bp,cs) == 0 ) break;
      if( loop(bp,dx/2,  0   ,dy,cs,0,DO)>dy/4 ) break;
      if( loop(bp,dx/2,  dy-1,dy,cs,0,UP)>dy/4 ) break;
      if( loop(bp,dx-1,  dy/3,dy,cs,0,LE)>dy/4 /* 3 with upper bow */
       && loop(bp,dx-1,  dy/8,dy,cs,0,LE)>dy/4 /* 3 with horizontal line */
       && loop(bp,dx/4,  dy/8,dy,cs,1,RI)<dy/2 ) break;
      if( loop(bp,dx-1,2*dy/3,dy,cs,0,LE)>dy/4 ) break;
      for( i3=x=0,i1=y=dy/5;y<dy/2;y++ ){ // suche erstes >
        i=loop(bp,0,y,dx,cs,0,RI);
        if( i>x ) { i3=x=i;i1=y; }
      } i3--; if( i3<dx/2 ) break;
      for( i4=x=0,i2=y=dy-1-dy/8;y>dy/2;y-- ){
        i=loop(bp,0,y,dx,cs,0,RI);
        if( i>x ) { i4=x=i;i2=y; }
      } i4--; if( i4<dx/2 ) break;
      for( x=xa=0,ya=y=dy/4;y<3*dy/4;y++ ){  // right gap
        i=loop(bp,dx-1,y,dx,cs,0,LE);
        if( i>=xa ) { xa=i;ya=y;x=xa+loop(bp,dx-1-xa,y,dx,cs,1,LE); }
      } if(dy>3*dx) if( xa<2 && x-xa<dx/2 ) break; // ~]
      if( get_bw(i3,i3,i1,i2  ,bp,cs,1) != 1 ) break;
      if( get_bw(i4,i4,i1,i2  ,bp,cs,1) != 1 ) break;
      if( get_bw(i3,i3,0 ,i1  ,bp,cs,1) != 1 ) break;
      if( get_bw(i4,i4,i1,dy-1,bp,cs,1) != 1 ) break;  // m like
      // hole is only allowed in beauty fonts
      // if( num_hole( x0, x1,      y0,      y1,box1->p,cs,NULL) > 0 ) // there is no hole
      // if( num_hole( x0, x0+dx/2, y0, y0+dy/2,box1->p,cs,NULL) == 0 ) // except in some beauty fonts
      if (sdata->holes.num>0)
      if (sdata->holes.hole[0].x1 >= dx/2 || sdata->holes.hole[0].y1 >= dy/2)
        break;
      setac(box1,(wchar_t)'3',ad);
      if (ad==100) return '3';
      break;
   }
   // --- test 4 -------
   for(ad=d=100;dx>3 && dy>5;){     // dy>dx
      if (sdata->holes.num > 2) break; /* be tolerant */
      // upper raising or vertical line
      if( loop(bp,0   ,3*dy/16,dx,cs,0,RI)
        < loop(bp,0   ,2*dy/4 ,dx,cs,0,RI)-dx/8 ) break;
      // search for a vertical line on lower end
      for (y=0;y<dy/4;y++)
       if( loop(bp,0   ,dy-1-y,dx,cs,0,RI)
         + loop(bp,dx-1,dy-1-y,dx,cs,0,LE) >= dx/2 ) break;
      if (y>=dy/4) break;
      if( loop(bp,0   ,dy-1-dy/8,dx,cs,0,RI) <  dx/4 ) break;
      // --- follow line from (1,0) to (0,.7)
      y=0; x=loop(bp,0,0,dx,cs,0,RI);
      if (x<=dx/4) {  // ocr-a-4
        i=loop(bp,0,dy/4,dx,cs,0,RI); if (i>dx/4) break;
        i=loop(bp,i,dy/4,dx,cs,1,RI); if (i>dx/2) break;
        j=loop(bp,i,dy/4,dy,cs,0,DO)+dy/4; if (j>7*dy/8) break;
      }
      turmite(bp,&x,&y,0,dx-1,0,dy-1,cs,DO,LE); if( x>=0 ) break;

      y=loop(bp,0,0,dy,cs,0,DO);
      if( (y+loop(bp,0,y,dy,cs,1,DO)) < dy/2 ) break;
      if( get_bw(x0     , x0+3*dx/8, y1-dy/6, y1-dy/6,box1->p,cs,1) == 1 ) break;
      if( get_bw(x0+dx/2, x1     , y1-dy/3, y1-dy/3,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0+dx/2, x0+dx/2, y0+dy/3, y1-dy/5,box1->p,cs,1) != 1 ) break;
      i=loop(bp,bp->x-1,  bp->y/4,dx,cs,0,LE);
      if( i > loop(bp,bp->x-1,2*bp->y/4,dx,cs,0,LE)+1
       && i > loop(bp,bp->x-1,3*bp->y/8,dx,cs,0,LE)+1 ) break;
      if (loop(bp,0,0,dx,cs,0,RI)>dx/4) {
        for(i=dx/8+1,x=0;x<dx && i;x++){
          if( num_cross(x ,x   ,0  ,dy-1, bp,cs) == 2 ) i--;
        } if( i ) break;
      }
      for(i=dy/6+1,y=dy/4;y<dy && i;y++){
        if( num_cross(0 ,dx-1,y  ,y   , bp,cs) == 2 ) i--;
      } if( i ) break;
      for(i=dy/10+1,y=dy-1-dy/4;y<dy && i;y++){
        if( num_cross(0   ,dx-1,y ,y  , bp,cs) == 1 )
        if( num_cross(dx/2,dx-1,y ,y  , bp,cs) == 1 ) i--;
      } if( i ) break;
      // i4 = num_hole ( x0, x1, y0, y1,box1->p,cs,NULL);
      i4 = sdata->holes.num;
      if( gchar ) {  // ~q
        i = loop(bp,0,dy/16,dx,cs,0,RI);
        if (i < dx/2  && dx<10 && dy<10 ) ad=ad*99/100;
        if (! hchar) ad=99*ad/100;
        if (i < dx/2  &&  i4 > 0) break; // hole?
        if ( loop(bp,     0,0,dy,cs,0,UP)
            -loop(bp,dx/8+1,0,dy,cs,0,UP)>dy/16) ad=97*ad/100;
      }
      for (j=y=0;y<dy/6;y++) {
        i=loop(bp,dx-1  ,y,dx,cs,0,LE);
        i=loop(bp,dx-1-i,y,dx,cs,1,LE); if (i>j) j=i;
      }
      if (j>=dx/2) ad=98*ad/100; // ~q handwritten a
      // ToDo: check y of masscenter of the hole q4
      
      if( i4 ) if( dx > 15 )
      if( loop(bp,  dx/2,   0,dy,cs,0,DO)<dy/16
       && loop(bp,  dx/4,   0,dy,cs,0,DO)<dy/8
       && loop(bp,3*dx/4,   0,dy,cs,0,DO)<dy/8
       && loop(bp,  dx/4,dy-1,dy,cs,0,UP)<dy/8
       && loop(bp,  dx/2,dy-1,dy,cs,0,UP)<dy/8
       && loop(bp,3*dx/4,dy-1,dy,cs,0,UP)<dy/4 ) break; // ~9

      i =loop(bp,dx-1  ,dy-1,dx,cs,0,LE); // ~9
      i+=loop(bp,dx-1-i,dy-1,dx,cs,1,LE);
      if( i>3*dx/4
       && i-loop(bp,dx-1,dy-1-dy/8,dx,cs,0,LE)>dx/4 ) break;
       
      i =loop(bp,dx-1-dx/4,dy-1,dx,cs,0,UP);
      if (i>  dy/2) ad=97*ad/100;
      if (i>3*dy/4) ad=97*ad/100;  /* handwritten n */

      if( num_cross(0 ,dx-1,dy/16 ,dy/16  , bp,cs) == 2 // ~9
       && loop(bp,dx-1,dy/16        ,dx,cs,0,LE)>
          loop(bp,dx-1,dy/16+1+dy/32,dx,cs,0,LE) ) break;
      if (gchar && !hchar) ad=98*ad/100; // ~q
      setac(box1,(wchar_t)'4',ad);      
      if (ad>99) bc='4';
      break;
   }
   // --- test 6 ------- ocr-a-6 looks like a b  :(
   for(ad=d=100;dx>3 && dy>4;){     // dy>dx
      if (sdata->holes.num > 2) break; /* be tolerant */
      if( loop(bp,   0,  dy/4,dx,cs,0,RI)>dx/2          // ocr-a=6
       && loop(bp,dx-1,     0,dy,cs,0,DO)>dy/4 ) break; // italic-6
      if( loop(bp,   0,  dy/2,dx,cs,0,RI)>dx/4 ) break;
      if( loop(bp,   0,3*dy/4,dx,cs,0,RI)>dx/4 ) break;
      if( loop(bp,dx-1,3*dy/4,dx,cs,0,LE)>dx/2 ) break;
      if( num_cross(x0+  dx/2,x0+  dx/2,y0     ,y1     ,box1->p,cs) != 3
       && num_cross(x0+5*dx/8,x0+5*dx/8,y0     ,y1     ,box1->p,cs) != 3 ) {
        if( num_cross(x0+  dx/2,x0+  dx/2,y0+dy/4,y1     ,box1->p,cs) != 2
         && num_cross(x0+5*dx/8,x0+5*dx/8,y0+dy/4,y1     ,box1->p,cs) != 2 ) break;
        // here we have the problem to decide between ocr-a-6 and b
        if ( loop(box1->p,(x0+x1)/2,y0,dy,cs,0,DO)<dy/2 ) break;
        ad=99*ad/100;
      } else {
        if (loop(box1->p,x0+dx/2,y0,dx,cs,0,DO)>dy/8
         && loop(box1->p,x1-dx/4,y0,dx,cs,0,DO)>dy/8 ) break;
      }
      if( num_cross(x0     ,x1     ,y1-dy/4,y1-dy/4,box1->p,cs) != 2 ) break;
      for( y=y0+dy/6;y<y0+dy/2;y++ ){
        x =loop(box1->p,x1    ,y  ,dx,cs,0,LE); if( x>dx/2 ) break;
        x+=loop(box1->p,x1-x+1,y-1,dx,cs,0,LE); if( x>dx/2 ) break;
      } if( y>=y0+dy/2 ) break;
      if (loop(box1->p,x0,y1-dy/3,dx,cs,0,RI)>dx/4 ) break;
      if (loop(box1->p,x1,y1-dy/3,dx,cs,0,LE)>dx/4 ) break;

      if (sdata->holes.num != 1) break;
      if (sdata->holes.hole[0].y1 < dy/2) ad=95*ad/100; // whats good for?
      if (sdata->holes.hole[0].y0 < dy/4) break;
//      if( num_hole ( x0, x1, y0, y0+dy/2,box1->p,cs,NULL) >  0 ) ad=95*ad/100; 
//      if( num_hole ( x0, x1, y0+dy/4, y1,box1->p,cs,NULL) != 1 ) break; 
//      if( num_hole ( x0, x1, y0     , y1,box1->p,cs,NULL) != 1 ) break; 
//    out_x(box1); printf(" x0 y0 %d %d\n",x0,y0);
      i1=loop(bp,0,dy/8     ,dx,cs,0,RI);
      i3=loop(bp,0,dy-1-dy/8,dx,cs,0,RI);
      i2=loop(bp,0,dy/2     ,dx,cs,0,RI); if(i1+i3-2*i2<1 && i1+i2+i3>0) break;  // convex from left
      for( x=dx,y=0;y<dy/4;y++ ){	// ~ b (serife?)
        i1=loop(bp,0,y,dx,cs,0,RI); if (i1>=dx-1) break;
        if(i1<x) x=i1; else if (i1>x) break;
      } if( y<dy/4 ) break;
      // ~& (with open upper loop)
      for( i=0,y=dy/2;y<dy;y++){
        if( num_cross(dx/2,dx-1,y,y,bp,cs) > 1 ) i++; if( i>dy/8 ) break;
      } if( y<dy ) break;
      if ( gchar) ad=99*ad/100;
      if (!hchar) ad=98*ad/100;
      if ( box1->dots ) ad=98*ad/100;
      setac(box1,(wchar_t)'6',ad);
      bc='6';
      break;
   }
   // --- test 7 ---------------------------------------------------
   if(dy>box1->m3-box1->m2) // too small
   for(ad=d=100;dx>2 && dy>4;){     // dx>1 dy>2*dx
      if (sdata->holes.num > 1) break; /* be tolerant */
      if( loop(bp,dx/2,0,dy,cs,0,DO)>dy/8 ) break;
      if( num_cross(0,dx-1,3*dy/4,3*dy/4,bp,cs) != 1 ) break; // preselect
      for( yb=xb=y=0;y<dy/2;y++){ // upper h-line and gap
        j=loop(bp,0,y,dx,cs,0,RI);if(xb>0 && j>dx/4) break; // gap after h-line
        j=loop(bp,j,y,dx,cs,1,RI);if(j>xb){ xb=j;yb=y; }  // h-line
      } if( xb<dx/4 || y==dy/2 ) break;
      j=loop(bp,0,dy/2,dx,cs,0,RI);
      j=loop(bp,j,dy/2,dx,cs,1,RI); if(xb<2*j) break; // minimum thickness
      for(x=0;y<dy;y++){	// one v-line?
        if( num_cross(0,dx-1,y,y,bp,cs) != 1 ) break;
        j=loop(bp,dx-1,y,dx,cs,0,LE); if( j<x ) break; if( j-1>x ) x=j-1;
      } if( y<dy || x<dx/3 ) break;
      j =loop(bp,dx-1,0,dy,cs,0,DO);  // ~T
      j+=loop(bp,dx-1,j,dy,cs,1,DO)+dy/16;
      i =loop(bp,dx-1,j,dx,cs,0,LE); if(j<dy/2) {
       if (i>j) break;
       j=loop(bp,   0,j,dx,cs,0,RI);
       if(j>dx/4 && j<=i+dx/16) break; // tall T
      }

      if(   loop(bp,   0,dy/2,dx,cs,0,RI)
          <=loop(bp,dx-1,dy/2,dx,cs,0,LE)+2 ) ad=ad*98/100; // l
      if( num_cross(0,dx-1,dy/4,dy/4,bp,cs) == 1
       && loop(bp,0,dy/4,dx,cs,0,RI) < dx/2 ) ad=ad*96/100; // J

      if (dy>3*dx) ad=99*ad/100; // )
      if ( gchar)  ad=98*ad/100; // J
      if (!hchar)  ad=95*ad/100;
      setac(box1,(wchar_t)'7',ad);
      break;
   }
   // --- test 8 ---------------------------------------------------
   // last change: May15th,2000 JS
   for(ad=d=100;dx>2 && dy>4;){     //    or we need large height
      if (sdata->holes.num != 2) break;
      if( num_cross(x0,x1,y0  +dy/4,y0  +dy/4,box1->p,cs) != 2 ) break; // ~gr (glued)
      if( num_cross(x0,x1,y1  -dy/4,y1  -dy/4,box1->p,cs) != 2
       && num_cross(x0,x1,y1-3*dy/8,y1-3*dy/8,box1->p,cs) != 2 ) break;
      if( get_bw(x0,x0+dx/4,y1-dy/4,y1-dy/4,box1->p,cs,1) == 0 ) break; // ~9
      if( get_bw(x0,x0+dx/2,y0+dy/4,y0+dy/4,box1->p,cs,1) == 0 ) break;
      if( get_bw(x0+dx/2,x0+dx/2,y0+dy/4,y1-dy/4,box1->p,cs,1) == 0 ) break; // ~0
// MX; printf(" x0 y0 %d %d\n",x0,y0);
      for( x=0,i=y=y0+dy/3;y<=y1-dy/3;y++){	// linke Kerbe
	j=loop(box1->p,x0,y,dx,cs,0,RI); if( j>x ) { x=j; i=y; }
      } if(i>y0+dy/2+dy/20 || x<dx/8 || x>dx/2) break;	// no gB
      j =   loop(box1->p,x1,y1-  dy/4,dx,cs,0,LE);
      if( j>loop(box1->p,x1,y1-  dy/5,dx,cs,0,LE)
       && j>loop(box1->p,x1,y1-2*dy/5,dx,cs,0,LE) ) break;	// &
      // check for upper hole
      for (j=0;j<sdata->holes.num;j++) {
        if (sdata->holes.hole[j].y1 < i-y0+1   ) break;
        if (sdata->holes.hole[j].y1 < i-y0+dy/8) break;
      } if (j==sdata->holes.num) break;  // not found
      // if( num_hole(x0,x1,y0,i+1   ,box1->p,cs,NULL)!=1 )
      // if( num_hole(x0,x1,y0,i+dy/8,box1->p,cs,NULL)!=1 ) break;	// upper hole
      // check for lower hole
      for (j=0;j<sdata->holes.num;j++) {
        if (sdata->holes.hole[j].y0 > i-y0-1   ) break;
      } if (j==sdata->holes.num) break;  // not found
      // if( num_hole(x0,x1,i-1,y1,box1->p,cs,NULL)!=1 ) break; 
      i1=i;
      for( x=0,i2=i=y=y0+dy/3;y<=y1-dy/3;y++){
	j=loop(box1->p,x1,y,dx,cs,0,LE); if( j>=x ) i2=y;
        if( j>x ) { x=j; i=y; }
      }
      if( i>y0+dy/2+dy/10 ) break;
      // if( x<dx/8 ) break;
      if( x>dx/2 ) break;
      if( num_cross(x0,x1, i      , i      ,box1->p,cs) != 1
       && num_cross(x0,x1, i+1    , i+1    ,box1->p,cs) != 1
       && num_cross(x0,x1,(i+i2)/2,(i+i2)/2,box1->p,cs) != 1 ) break; // no g
      if(abs(i1-i)>(dy+5)/10) ad=99*ad/100; // distance nick-node
      if(abs(i1-i)>(dy+4)/8) break;
      // ~B ff
      for(i=dx,y=0;y<dy/8+2;y++){
        j=loop(bp,0,y,dx,cs,0,RI); if( j<i ) i=j; if( j>i+dx/16 ) break;
      } if( y<dy/8+2 ) break;
      for(i=dx,y=0;y<dy/8+2;y++){
        j=loop(bp,0,dy-1-y,dx,cs,0,RI);
        if( j<i ) i=j; if( j>i+dx/16 ) break;
      } if( y<dy/8+2 ) break;
      if(  dy>16 && num_cross(0,dx-1,dy-1,dy-1,bp,cs) > 1
        && loop(bp,0,dy-1,dx,cs,0,RI) <dx/8+1 ) break; // no fat serif S
      for( i=0,y=dy/2;y<dy;y++){
        if( num_cross(0,dx-1,y,y,bp,cs) > 2 ) i++; if( i>dy/8 ) break;
      } if( y<dy ) break;
      if ( loop(bp,dx-1,0,dx,cs,0,LE)==0 ) ad=99*ad/100;
      if (num_cross(0,dx-1,dy-1,dy-1,bp,cs) > 1) ad=98*ad/100; // &
      if (num_cross(dx-1,dx-1,dy/2,dy-1,bp,cs) > 1) ad=98*ad/100; // &
      if (num_cross(0,dx-1,   0,   0,bp,cs) > 1) ad=98*ad/100;
      if (num_cross(0,dx-1,   1,   1,bp,cs) > 1) ad=98*ad/100;
      /* if m1..4 is unsure ignore hchar and gchar ~ga */
      if (!hchar) {
        if ((box1->m2-box1->y0)*8>=dy) ad=98*ad/100;
        else                           ad=99*ad/100;
      }
      if ( gchar
         && (box1->y1-box1->m3)*8>=dy) ad=99*ad/100;
      setac(box1,(wchar_t)'8',ad);
      break;
   }
   // --- test 9 \it g ---------------------------------------------------
   for(ad=d=100;dx>2 && dy>4;){     // dx>1 dy>2*dx
      if (sdata->holes.num > 2) break; /* be tolerant */
      if( num_cross(x0+  dx/2,x0+  dx/2,y0,y1-dy/4,box1->p,cs) != 2 // pre select
       && num_cross(x0+  dx/2,x0+  dx/2,y0,     y1,box1->p,cs) != 3 // pre select
       && num_cross(x0+3*dx/8,x0+3*dx/8,y0,y1,box1->p,cs) != 3
       && num_cross(x0+  dx/4,x1  -dx/4,y0,y1,box1->p,cs) != 3 ) break;
      if( num_cross(x0+  dx/2,x0  +dx/2,y0,y0+dy/4,box1->p,cs) < 1 ) break;
      if( num_cross(x0+  dx/2,x1, y0+dy/2 ,y0+dy/2,box1->p,cs) < 1 ) break;
      if( num_cross(x0,x1, y0+  dy/4 ,y0+  dy/4,box1->p,cs) != 2 
       && num_cross(x0,x1, y0+3*dy/8 ,y0+3*dy/8,box1->p,cs) != 2 ) break;
      for( x=0,i=y=y0+dy/2;y<=y1-dy/4;y++){	// find notch (suche kerbe)
	j=loop(box1->p,x0,y,dx,cs,0,RI); 
        if( j>x ) { x=j; i=y; }  
      } if (x<1 || x<dx/8) break; y=i;
 //      fprintf(stderr," debug 9: %d %d\n",x,i-y0);
      if( x<dx/2 ) {  /* big bow? */
        j=loop(box1->p,x0+x-1,y,dy/8+1,cs,0,DO)/2; y=i=y+j;
        j=loop(box1->p,x0+x-1,y,dx/2  ,cs,0,RI);   x+=j;
        if (x<dx/2) break;
      }
      if( num_cross(x0+dx/2,x1,i,y1,box1->p,cs) != 1 ) break;

      if (sdata->holes.num < 1) { /* happens for 5x7 font */
        if (dx<8) ad=98*ad/100; else break; }
      else {
        if (sdata->holes.hole[0].y1 >= i+1) break;
        if (sdata->holes.hole[0].y0 >  i-1) break;
        if (sdata->holes.num > 1)
        if (sdata->holes.hole[1].y0 >  i-1) break;
      // if( num_hole(x0,x1,y0,i+1,box1->p,cs,NULL)!=1 ) break;
      // if( num_hole(x0,x1,i-1,y1,box1->p,cs,NULL)!=0 ) break;
      }
      if( loop(box1->p,x0,y1  ,dy,cs,0,RI)>dx/3 &&
          loop(box1->p,x0,y1-1,dy,cs,0,RI)>dx/3) ad=98*ad/100; // no q OR ocr-a-9
      for( x=0,i=y=y0+dy/3;y<=y1-dy/3;y++){	// suche kerbe
	j=loop(box1->p,x1,y,dx,cs,0,LE); 
        if( j>x ) { x=j; i=y; }
      } if( x>dx/2 ) break;		// no g
      i1=loop(bp,dx-1,dy/8     ,dx,cs,0,LE); if(i1>dx/2) break;
      i3=loop(bp,dx-1,dy-1-dy/8,dx,cs,0,LE);
      i2=loop(bp,dx-1,dy/2     ,dx,cs,0,LE); if(i1+i3-2*i2<0) break; // konvex 
      i1=loop(bp,dx-1,dy/4     ,dx,cs,0,LE); if(i1>dx/2) break;
      i3=loop(bp,dx-1,dy-1-dy/8,dx,cs,0,LE);
      for(y=dy/4;y<dy-1-dy/4;y++){
        i2=loop(bp,dx-1,y,dx,cs,0,LE);
        if(i1+i3-2*i2<-1) break;  // konvex from right ~g ~3
      } if(y<dy-1-dy/4) break;
      x=loop(bp,dx  -1,6*dy/8,dx,cs,0,LE); if(x>0){
        x--; // robust
        y=loop(bp,dx-x-1,  dy-1,dy,cs,0,UP);
        if(y<dy/8) break; // ~q (serif!)
      }
      if( gchar) ad=97*ad/100;  /* unsure */
      if(!hchar) ad=90*ad/100;  /* unsure */
      setac(box1,(wchar_t)'9',ad);
      break;
   }
   // 0 is same as O !?
   // --- test 0 (with a hole in it ) -----------------------------
   for(d=ad=100;dx>2 && dy>3;){     // min 3x4
      if (sdata->holes.num > 1) break; /* be tolerant */
      if( get_bw(x0      , x0+dx/3,y0+dy/2 , y0+dy/2,box1->p,cs,1) != 1 ) break;
      if( get_bw(x1-dx/3 , x1     ,y0+dy/2 , y0+dy/2,box1->p,cs,1) != 1 ) break;
      /* could be an O, unless we find a dot in the center */
      if( get_bw(x0      , x1     ,y0+dy/2 , y0+dy/2,box1->p,cs,1) != 3 ) ad=99;
      if( get_bw(x0+dx/2 , x0+dx/2,y1-dy/3 , y1,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0+dx/2 , x0+dx/2,y0      , y0+dy/3,box1->p,cs,1) != 1 ) break;
      /* accept 0 with dot in center, accept \/0 too ... */
      if( get_bw(x0+dx/2 , x0+dx/2,y0+dy/3 , y1-dy/3,box1->p,cs,1) != 0 ) break;

      if( num_cross(x0+dx/2,x0+dx/2,y0      , y1     ,box1->p,cs)  != 2 ) break;
      if( num_cross(x0+dx/3,x1-dx/3,y0      , y0     ,box1->p,cs)  != 1 ) // AND
      if( num_cross(x0+dx/3,x1-dx/3,y0+1    , y0+1   ,box1->p,cs)  != 1 ) break;
      if( num_cross(x0+dx/3,x1-dx/3,y1      , y1     ,box1->p,cs)  != 1 ) // against "rauschen"
      if( num_cross(x0+dx/3,x1-dx/3,y1-1    , y1-1   ,box1->p,cs)  != 1 ) break;
      if( num_cross(x0     ,x0     ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 )
      if( num_cross(x0+1   ,x0+1   ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 ) break;
      if( num_cross(x1     ,x1     ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 )
      if( num_cross(x1-1   ,x1-1   ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 ) break;
      // if( num_hole(x0,x1,y0,y1,box1->p,cs,NULL) != 1 ) break;
      if (sdata->holes.num != 1) break;
      
      i= loop(bp,0   ,0   ,x1-x0,cs,0,RI)-
         loop(bp,0   ,2   ,x1-x0,cs,0,RI);
      if (i<0) break;
      if (i==0) {
        if( loop(bp,dx-1,0   ,x1-x0,cs,0,LE)>
            loop(bp,dx-1,2   ,x1-x0,cs,0,LE)  ) ad=98*ad/100;
        ad=98*ad/100;
      }
      
      x=loop(bp,dx-1,dy-1-dy/3,x1-x0,cs,0,LE);	// should be minimum
      for( y=dy-1-dy/3;y<dy;y++ ){
        i=loop(bp,dx-1,y,x1-x0,cs,0,LE);
        if( i<x ) break; x=i;
      }
      if( y<dy ) break;

      // ~D (but ocr-a-font)
      i=      loop(bp,   0,     dy/16,dx,cs,0,RI)
         +    loop(bp,   0,dy-1-dy/16,dx,cs,0,RI)
         -  2*loop(bp,   0,     dy/2 ,dx,cs,0,RI);
      j=      loop(bp,dx-1,     dy/16,dx,cs,0,LE)
         +    loop(bp,dx-1,dy-1-dy/16,dx,cs,0,LE)
         <= 2*loop(bp,dx-1,     dy/2 ,dx,cs,0,LE);
      if (i<-dx/8 || i+dx/8<j) break; // not konvex

      if( loop(bp,dx-1,     dy/16,dx,cs,0,LE)>dx/8 )
      if( loop(bp,0   ,     dy/16,dx,cs,0,RI)<dx/16 ) break;
      if( loop(bp,dx-1,dy-1-dy/16,dx,cs,0,LE)>dx/8 )
      if( loop(bp,0   ,dy-1-dy/16,dx,cs,0,RI)<dx/16 ) break;
      if( get_bw(x1-dx/32,x1,y0,y0+dy/32,box1->p,cs,1) == 0
       && get_bw(x1-dx/32,x1,y1-dy/32,y1,box1->p,cs,1) == 0
       && ( get_bw(x0,x0+dx/32,y0,y0+dy/32,box1->p,cs,1) == 1
         || get_bw(x0,x0+dx/32,y1-dy/32,y1,box1->p,cs,1) == 1 ) ) {
         if (dx<32) ad=ad*98/100; else break; // ~D
      }


       // italic a
      for(i=0,y=6*dy/8;y<dy;y++)
        if( num_cross(0,dx-1,y,y,bp,cs) > 2 ) i++; else i--;
      if(i>0) break; // ~'a' \it a

      if (!hchar) ad=95*ad/100;
      if (ad>99) ad=99; /* we can never be sure having a O,
                           let context correction decide */
      setac(box1,(wchar_t)'0',ad);
      break;
   } 
   // --- test 0 with a straight line in it -------------------
   for(ad=100;dx>4 && dy>5;){  /* v0.3.1+ */
      if (sdata->holes.num > 3) break; /* be tolerant */
      if (sdata->holes.num < 1) break;
      if (sdata->holes.num != 2) ad=95*ad/100;
      if( get_bw(x0      , x0+dx/2,y0+dy/2 , y0+dy/2,box1->p,cs,1) != 1 ) break;
      if( get_bw(x1-dx/2 , x1     ,y0+dy/2 , y0+dy/2,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0+dx/2 , x0+dx/2,y1-dy/2 , y1,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0+dx/2 , x0+dx/2,y0      , y0+dy/2,box1->p,cs,1) != 1 ) break;
      if( get_bw(x0+dx/2 , x0+dx/2,y0+dy/3 , y1-dy/3,box1->p,cs,1) != 1 ) break;
      // out_x(box1); printf(" x0 y0 %d %d\n",x0,y0);
      if( num_cross(x0+dx/2,x0+dx/2,y0      , y1     ,box1->p,cs)  != 3 ) break;
      if( num_cross(x0+dx/3,x1-dx/3,y0      , y0     ,box1->p,cs)  != 1 ) // AND
      if( num_cross(x0+dx/3,x1-dx/3,y0+1    , y0+1   ,box1->p,cs)  != 1 ) break;
      if( num_cross(x0+dx/3,x1-dx/3,y1      , y1     ,box1->p,cs)  != 1 ) // against "rauschen"
      if( num_cross(x0+dx/3,x1-dx/3,y1-1    , y1-1   ,box1->p,cs)  != 1 ) break;
      if( num_cross(x0     ,x0     ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 )
      if( num_cross(x0+1   ,x0+1   ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 ) break;
      if( num_cross(x1     ,x1     ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 )
      if( num_cross(x1-1   ,x1-1   ,y0+dy/3 , y1-dy/3,box1->p,cs)  != 1 ) break;
      // if( num_hole(x0,x1,y0,y1,box1->p,cs,NULL) != 2 ) break;
      if (sdata->holes.num != 2) ad=85*ad/100;
      
      if( loop(bp,0   ,        0,x1-x0,cs,0,RI)<=
          loop(bp,0   ,  2+dy/32,x1-x0,cs,0,RI)  ) break;
      x=  loop(bp,0   ,dy/2  ,x1-x0,cs,0,RI);
      i=  loop(bp,0   ,dy/2-1,x1-x0,cs,0,RI); if (i>x) x=i;
      i=  loop(bp,0   ,dy/2-2,x1-x0,cs,0,RI); if (i>x && dy>8) x=i;
      if( loop(bp,0   ,  dy/4,x1-x0,cs,0,RI)<x ) break; // ~8
      x=  loop(bp,dx-1,dy/2  ,x1-x0,cs,0,LE);
      i=  loop(bp,dx-1,dy/2-1,x1-x0,cs,0,LE); if(i>x) x=i;
      i=  loop(bp,dx-1,dy/2-1,x1-x0,cs,0,LE); if(i>x && dy>8) x=i;
      if( loop(bp,dx-1,3*dy/4,x1-x0,cs,0,LE)<x) break; // ~8

      x=loop(bp,dx-1,dy-1-dy/3,x1-x0,cs,0,LE);	// should be minimum
      for( y=dy-1-dy/3;y<dy;y++ ){
        i=loop(bp,dx-1,y,x1-x0,cs,0,LE);
        if (i<x-dx/16) break; 
        if (i>x) x=i;
      }
      if( y<dy ) break;
      
      /* test for straight line */
      y =loop(bp,dx/2,dy-1  ,y1-y0,cs,0,UP); if(y>dy/4) break;
      y+=loop(bp,dx/2,dy-1-y,y1-y0,cs,1,UP); if(y>dy/3) break; if (y>dy/4) ad=ad*99/100;
      y+=loop(bp,dx/2,dy-1-y,y1-y0,cs,0,UP); if(3*y>2*dy) break;
      x =loop(bp,dx/2,dy-y,dx/2,cs,0,RI);    if(x==0) break;
      // MM; fprintf(stderr," y=%d x=%d\n",y-1,x);
      if( loop(bp,dx/2+x-1-dx/16,dy-y,y1-y0,cs,0,UP)==0 ) break;
       // $
      for(i=0,y=dy/4;y<dy-dy/4-1;y++)
      if( loop(bp,   0,y,dx-1,cs,0,RI) > dx/4
       || loop(bp,dx-1,y,dx-1,cs,0,LE) > dx/4 ) break;
      if( y<dy-dy/4-1 ) break;

      // ~D
      if(     loop(bp,0,     dy/16,dx,cs,0,RI)
         +    loop(bp,0,dy-1-dy/16,dx,cs,0,RI)
         <= 2*loop(bp,0,     dy/2 ,dx,cs,0,RI)+dx/8 ) break; // not konvex

      if( loop(bp,dx-1,     dy/16,dx,cs,0,LE)>dx/8 )
      if( loop(bp,0   ,     dy/16,dx,cs,0,RI)<dx/16 ) break;
      if( loop(bp,dx-1,dy-1-dy/16,dx,cs,0,LE)>dx/8 )
      if( loop(bp,0   ,dy-1-dy/16,dx,cs,0,RI)<dx/16 ) break;
      if( get_bw(x1-dx/32,x1,y0,y0+dy/32,box1->p,cs,1) == 0
       && get_bw(x1-dx/32,x1,y1-dy/32,y1,box1->p,cs,1) == 0
       && ( get_bw(x0,x0+dx/32,y0,y0+dy/32,box1->p,cs,1) == 1
         || get_bw(x0,x0+dx/32,y1-dy/32,y1,box1->p,cs,1) == 1 ) ) break; // ~D

      /* 5x9 font "9" is like "0" */
      if (dx<16)
      if ( num_cross(x0,x0,y0,y1,box1->p,cs)  != 1 ) ad=98*ad/100;

       // italic a
      for(i=0,y=6*dy/8;y<dy-dy/16;y++)
      if( num_cross(0,dx-1,y,y,bp,cs) > 2 ) i++; else i--;
      if(i>0) ad=ad*98/100; // ~'a' \it a
      if( !hchar ) ad=90*ad/100;
      setac(box1,(wchar_t)'0',ad);
      break;
   } 
   if(box1->ac==UNKNOWN)
     box1->ac=ac;
   return box1->c;
}
