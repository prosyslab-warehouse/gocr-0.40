// test routines - faster to compile
#include <stdlib.h>
#include <stdio.h>
#include "pgm2asc.h"
#include "unicode.h"
#include "amiga.h"
#include "gocr.h"

// for learn_mode/analyze_mode high, with, yoffset, num of pattern_i,
//  - holes (center,radius in relative coordinates) etc. => cluster analyze
// num_hole => min-volume, tolerance border
// pattern:  @@ @. @@
//           .@ @. ..
// regular filter for large resolutions to make edges more smooth (on boxes)
// extra-filter (only if not recognized?)
//   map + same color to (#==change)
//       - anti color
//       . not used
// strongest neighbour pixels (3x3) => directions
// second/third run with more and more tolerance!?

/* FIXME jb: following is unused */
#if 0
struct lobj {	// line-object (for fitting to near lines)
	int x0,y0;	// starting point (left up)
        int x1,y1;      // end point      (right down)
        int mt;		// minimum thickness
	int q;		// quality, overlapp
};

/* FIXME jb global */
struct lobj obj1;
#endif

// that is the first draft of feature extraction 
// detect main lines and bows
// seems bad implemented, looking for better algorithms (ToDo: use autotrace)
#define MAXL 10
void ocr2(pix *b,int cs){
  int x1,y1,x2,y2,l,i,j,xa[MAXL],ya[MAXL],xb[MAXL],yb[MAXL],ll[MAXL];
  for(i=0;i<MAXL;i++)xa[i]=ya[i]=xb[i]=yb[i]=ll[i]=0;
  for(x1=0;x1<b->x;x1++)		// very slowly, but simple to program
  for(y1=0;y1<b->y;y1++)         // brute force
  for(x2=0;x2<b->x;x2++)
  for(y2=y1+1;y2<b->y;y2++)
  {
    if( get_line2(x1,y1,x2,y2,b,cs,100)>99 )
    {  // line ???
      l=(x2-x1)*(x2-x1)+(y2-y1)*(y2-y1);  // len
      for(i=0;i<MAXL;i++)
      {  // remove similar lines (same middle point) IMPROVE IT !!!!!! ???
        if(
            abs(x1+x2-xa[i]-xb[i])<1+b->x/2
         && abs(y1+y2-ya[i]-yb[i])<1+b->y/2
         && abs(y1-ya[i])<1+b->y/4
         && abs(x1-xa[i])<1+b->x/4
          )
        {
          if( l>ll[i] )
          {
            for(j=i;j<MAXL-1;j++)
            {  // shift table
              xa[j]=xa[j+1];ya[j]=ya[j+1];
              xb[j]=xb[j+1];yb[j]=yb[j+1];ll[j]=ll[j+1];
            }
            ll[MAXL-1]=0;
          }
          else break; // forget it if shorter
        }
        if( l>ll[i] ){ // insert if larger
          for(j=MAXL-1;j>i;j--){  // shift table
            xa[j]=xa[j-1];ya[j]=ya[j-1];
            xb[j]=xb[j-1];yb[j]=yb[j-1];ll[j]=ll[j-1];
          }
          xa[i]=x1;ya[i]=y1;xb[i]=x2;yb[i]=y2;ll[i]=l;
          break;
        }
      }
    }
  }
  for(i=0;i<MAXL;i++){
    printf(" %2d %2d %2d %2d %3d\n",xa[i],ya[i],xb[i],yb[i],ll[i]);
  }  
}

#define MM fprintf(stderr," L%d",__LINE__)

// here is the test part for new variants (faster compilation) ;)
wchar_t ocr1(struct box *box1, pix  *b, int cs){
   pix *p=(box1->p);
   int	d,x,y,x0=box1->x0,x1=box1->x1,y0=box1->y0,y1=box1->y1;	// d=besterror (not used)
   int  dx=x1-x0+1,dy=y1-y0+1;	// size
   int i,j,i0,i1,i2,i3,i4;
   wchar_t bc = UNKNOWN;
   int hchar,gchar,ad=0,dots=box1->dots;
   /*FIXME jb static*/static int vertical_detected=0;    /* trick for l/-detection */
   
   // out_x(box1);ocr2(b,cs);  // test other engine
   hchar=0;if( 2*y0<=2*box1->m2-(box1->m2-box1->m1) ) hchar=1;
   gchar=0;if( 2*y1>=2*box1->m3+(box1->m4-box1->m3) ) gchar=1;
//      out_x(box1);
   if(dots>5)fprintf(stderr,"# hmm, Something was going wrong.\n");

// recognize a "l" is a never ending problem, because there are lots of
// variants and the char is not very unique (under construction)
   // --- test italic l ---------------------------------------------------
   // --- test l ~italic (set flag-italic) --------------------------------
   // if unsure d should be multiplied by 80..90%
   for(ad=d=100; dy>dx && dy>6;){     // min 3x4
      if( dots>0 ) break;
      if( num_cross(0, dx-1,dy/2,dy/2,b,cs) != 1
       || num_cross(0, dx-1,dy/4,dy/4,b,cs) != 1 ) break;
      // mesure thickness
      for(i1=0,i2=dx,y=dy/4;y<dy-dy/4;y++){
        j = loop(b,0,y,dx,cs,0,RI);
        j = loop(b,j,y,dx,cs,1,RI);
        if( j>i1 ) { i1=j; } // thickest
        if( j<i2 ) { i2=j; } // thinnest
      }
      if ( i1>2*i2 ) break;
      if(box1->m3 && dy<=box1->m3-box1->m2) ad=94*ad/100;
      if( box1->m2-box1->m1>1 &&  y0>=box1->m2 ) ad=94*ad/100;
      for(i0=0,i3=0,y=0;y<dy/4;y++){
        j = loop(b,0,y,dx,cs,0,RI);
        if( j>i3 ) { i3=j; }      // widest space
        j = loop(b,j,y,dx,cs,1,RI);
        if( j>i0 ) { i0=j;i3=0; } // thickest
      }
      if ( i0>4*i2 || 3*i3>2*dx)
      if ( loop(b,dx-1,dy-1,dx,cs,0,LE)>3*dx/8
        || loop(b,   0,dy-1,dx,cs,0,RI)>3*dx/8) break; // ~7
// out_x(box1);
// MM;

      // detect serifs 
      x =loop(b,0,   0,dx,cs,0,RI);
      i3=loop(b,x,   0,dx,cs,0,RI);
      x =loop(b,0,   1,dx,cs,0,RI);
      x =loop(b,x,   1,dx,cs,0,RI); if(x>i3) i3=x;
      x =loop(b,0,dy-1,dx,cs,0,RI);
      i4=loop(b,x,dy-1,dx,cs,0,RI);
      x =loop(b,0,dy-2,dx,cs,0,RI);
      x =loop(b,x,dy-2,dx,cs,0,RI); if(x>i4) i4=x;
      if( i3>i1+dx/8+1 && i4>i1+dx/8+1 ) break; // ~I 
// MM;

      for(i=dx,j=0,y=1;y<dy/4;y++){
        x=loop(b,dx-1,y,dx,cs,0,LE); if(x>i+1) break; i=x;
        if( num_cross(0,dx-1,y        ,y        ,b,cs)==2
         && num_cross(0,dx-1,y+1+dy/32,y+1+dy/32,b,cs)==2 ) j=1;
      } if ( y<dy/4 ) break;
// MM;
      if(j){	// if loop at the upper end, look also on bottom
        for(y=3*dy/4;y<dy;y++){
          if( num_cross(0,dx-1,y        ,y        ,b,cs)==2
           && num_cross(0,dx-1,y-1-dy/32,y-1-dy/32,b,cs)==2 ) break;
        } if ( y==dy ) break;
      }
// MM;

      // if( get_bw(x0,x1,y0,y1,p,cs,2) == 0 ) break; // unsure !I|

      if(dx>3)
      if( get_bw(dx-1-dx/8,dx-1,0,dy/6,b,cs,1) != 1 )
      if( get_bw(dx-1-dx/8,dx-1,0,dy/2,b,cs,1) == 1 ) break;
// MM;

      if( get_bw(dx-1-dx/8,dx-1,dy/4,dy/3,b,cs,1) != 1 ) // large I ???
      if( get_bw(0        ,dx/8,dy/4,dy/3,b,cs,1) != 1 )
      if( get_bw(dx-1-dx/8,dx-1,0   ,dy/8,b,cs,1) == 1 )
      if( get_bw(0        ,dx/8,0   ,dy/8,b,cs,1) == 1 ) break;
// MM;
      if( get_bw(dx-1-dx/8,dx-1,dy/2,dy-1,b,cs,1) != 1 ) // r ???
      if( get_bw(0        ,dx/8,dy/2,dy-1,b,cs,1) == 1 )
      if( get_bw(dx-1-dx/8,dx-1,0   ,dy/3,b,cs,1) == 1 )
      if( get_bw(0        ,dx/8,0   ,dy/3,b,cs,1) == 1 ) break;

// MM;
      for( y=1;y<12*dy/16;y++ )
      if( num_cross(0, dx-1, y  , y  ,b,cs) != 1 	// sure ?
       && num_cross(0, dx-1, y-1, y-1,b,cs) != 1 ) break;
      if( y<12*dy/16 ) break;

// MM;
      if(dx>3){
        for( y=dy/2;y<dy-1;y++ )
        if( get_bw(dx/4,dx-1-dx/4,y,y,b,cs,1) != 1 ) break;
        if( y<dy-1 ) break;
      }
      // test ob rechte Kante gerade
      for(x=dx,y=b->y-1-5*dy/16;y>=dy/5;y--){ // rechts abfallende Kante/Knick?
        i=loop(b,b->x-1,y,x1-x0,cs,0,LE);
        if( i-2-dx/16>=x ) break;
        if( i<x ) x=i;
      }
      if (y>=dy/5 ) break; 
// MM;

      // test ob linke Kante gerade
      for(x=0,y=b->y-1-dy/5;y>=dy/5;y--){ // rechts abfallende Kante/Knick?
        i=loop(b,0,y,x1-x0,cs,0,RI);
        if( i+2+dx/16<x ) break;
        if( i>x ) x=i;
      }
      if (y>=dy/5 ) break; 
// MM;

      if( get_bw(x0,x1,y1+1,y1+dy/3,p,cs,1) == 1 ) break; // unsure !l|
      i=loop(b,dx-1,dy/16,dx,cs,0,LE);
      j=loop(b,dx-1,dy/2 ,dx,cs,0,LE);
      if( i>3 && j>3  )
      if( get_bw(dx-1-i/2,dx-1-i/2,0,dy/2,b,cs,1) == 1 ) break; // ~t
// MM;

      for(y=5*dy/8;y<dy;y++)
      if( num_cross(0,dx-1,y,y,b,cs) == 2 ) break;
      if( y<dy ){
         i =loop(b,0,y,dx,cs,0,RI);
         i+=loop(b,i,y,dx,cs,1,RI);
         i+=loop(b,i,y,dx,cs,0,RI)/2; // middle of v-gap
         if( num_cross(0,i,5*dy/8,5*dy/8,b,cs)==0
          && num_cross(i,i,5*dy/8,     y,b,cs)==0 ) break; // ~J
      }
// MM;
      if ( dx>8
        && loop(b,   0,3*dy/4,dx,cs,0,RI)>=dx/4
        && loop(b,   0,7*dy/8,dx,cs,0,RI)<=dx/8
        && loop(b,dx-1,3*dy/4,dx,cs,0,LE)<=dx/8
        && loop(b,dx-1,7*dy/8,dx,cs,0,LE)<=dx/8 ) break; // ~J

      if ( 2*i3>5*i1 )		// hmm \tt l can look very similar to 7
      if (  loop(b,0,dy/4,dx,cs,0,RI)>dx/2
       &&   get_bw(0,dx/8,0,dy/4,b,cs,1) == 1 ) break; // ~7
// MM;
      
      if ( loop(b,dx-1,dy/2,dx,cs,0,LE)>dx/2
       &&  get_bw(3*dx/4,dx-1,3*dy/4,dy-1,b,cs,1) == 1) {
        if (loop(b,0,dy-1,dx,cs,0,RI)<dx/8) ad=99*ad/100; // ~L
        if(5*dx>2*dy) ad=99*ad/100; // ~L
        if(5*dx>3*dy) ad=99*ad/100; // ~L
      }
// MM;
      if(!hchar){	// right part (bow) of h is never a l
        if( get_bw(dx/4,dx/4,   0,dy/4,b,cs,1) == 1
         && get_bw(dx/4,dx/4,dy/2,dy-1,b,cs,1) == 0 ) break;
      }
// out_x(box1);
// MM;
      if( dx>3 && dy>3*dx )
      if( loop(b,dx/4,dy-1     ,dy,cs,0,UP)< dy/4
       && loop(b,   0,dy-1-dy/8,dx,cs,0,RI)>=dx/2
       && loop(b,dx-1,dy-1-dy/8,dx,cs,0,LE)<=dx/4 ){
         ad=98*ad/100; // ~]
        if ( loop(b,dx-1,dy/2,dx,cs,0,LE)==0 ) break;
      }
// MM;

      for(x=0;x<dx/2;x++)
      if( get_bw(   x,    x,    0,dy/4 ,b,cs,1) == 1 ) break;
      // works only for perpenticular char
      if( get_bw( x,x+dx/16,    0,dy/16,b,cs,1) == 0
       && get_bw( x,x+dx/16,dy/4 ,dy/2 ,b,cs,1) == 0
       && get_bw( x,x+dx/16,dy/16,dy/4 ,b,cs,1) == 1 ){
        for(i=dx,y=0;y<dy/4;y++){
          x=loop(b,0,y,dx,cs,0,RI);
          if( x>i ) break;
        }
        if( x>=loop(b,0,y+1,dx,cs,0,RI) )
        if(  loop(b,0      ,0,dy,cs,0,DO)>1 )
        if(  loop(b,0      ,0,dy,cs,0,DO)
           - loop(b,dx/16+1,0,dy,cs,0,DO) < dx/16+1 ) break; // ~1 Jul00,Nov00
        if( num_cross(0,dx/2,y-1,y-1,b,cs)==2 ) break; // ~1
      }
      if(dx<8 && dy<12){ // screen font
        i=  loop(b,0,0,dy,cs,0,DO);
        if( loop(b,dx/2,1,dy,cs,1,DO)>=dy-2
         && loop(b,0,dy/2,dx,cs,0,RI)>=2
         && i>1 && i<dy/2 ) break; // ~1
      }
// MM;
      if( get_bw(x1,x1,y0  ,y1  ,box1->p,cs,2) != 2
       && get_bw(x0,x1,y0  ,y0  ,box1->p,cs,2) != 2
       && get_bw(x0,x1,y1  ,y1  ,box1->p,cs,2) != 2
       && get_bw(x0,x0+dx/4,y0+1+dy/16,y1-1-dy/16,box1->p,cs,1) != 1 ) break; /* ~] */
      i=loop(b,dx-1,dy/2,dx,cs,0,LE);
      if( loop(b,   0,dy/2,dx,cs,0,RI)>=dx/2
       && (i<dx/2 || i==0) ) ad=98*ad/100; // ~]
      if( get_bw(x0,x0,y0  ,y1  ,box1->p,cs,2) != 2
       && get_bw(x0,x1,y0  ,y0  ,box1->p,cs,2) != 2
       && get_bw(x0,x1,y1  ,y1  ,box1->p,cs,2) != 2
       && get_bw(x1-dx/4,x1,y0+1+dy/16,y1-1-dy/16,box1->p,cs,1) != 1 ) break; /* ~[ */
// MM;

      x =loop(b,   0,dy/2,dx,cs,0,RI);	// konvex/konkav? ~()
      i =loop(b,dx-1,dy/2,dx,cs,0,LE);
      if( loop(b,   0,7*dy/8,dx,cs,0,RI) > x+dx/8
       && loop(b,   0,  dy/8,dx,cs,0,RI) > x+dx/8
       && loop(b,dx-1,7*dy/8,dx,cs,0,LE) < i-dx/8
       && loop(b,dx-1,  dy/8,dx,cs,0,LE) < i-dx/8 ) break; // ~(
      if( loop(b,   0,7*dy/8,dx,cs,0,RI) < x-dx/8
       && loop(b,   0,  dy/8,dx,cs,0,RI) < x-dx/8
       && loop(b,dx-1,7*dy/8,dx,cs,0,LE) > i+dx/8
       && loop(b,dx-1,  dy/8,dx,cs,0,LE) > i+dx/8 ) break; // ~)
// MM;
      
      i=    loop(b,   0,      0,dy,cs,0,DO); // horizontal line?
      if(dy>=12 && i>dy/8 && i<dy/2){
        if(   loop(b,dx-1,3*dy/16,dx,cs,0,LE)-dx/8
             >loop(b,dx-1,      i,dx,cs,0,LE) 
         ||   loop(b,dx-1,3*dy/16,dx,cs,0,LE)-dx/8
             >loop(b,dx-1,    i+1,dx,cs,0,LE)       )
        if(   loop(b,dx-1,8*dy/16,dx,cs,0,LE)-dx/8
             >loop(b,dx-1,      i,dx,cs,0,LE) 
         ||   loop(b,dx-1,8*dy/16,dx,cs,0,LE)-dx/8
             >loop(b,dx-1,    i+1,dx,cs,0,LE)       )
        if(   loop(b,   0,3*dy/16,dx,cs,0,RI)-dx/8
             >loop(b,   0,      i,dx,cs,0,RI) 
         ||   loop(b,   0,3*dy/16,dx,cs,0,RI)-dx/8
             >loop(b,   0,    i+1,dx,cs,0,RI)       )
        if(   loop(b,   0,8*dy/16,dx,cs,0,RI)-dx/8
             >loop(b,   0,      i,dx,cs,0,RI) 
         ||   loop(b,   0,8*dy/16,dx,cs,0,RI)-dx/8
             >loop(b,   0,    i+1,dx,cs,0,RI)       ) break; // ~t
        if(   loop(b,   0,i-1,dx,cs,0,RI)>1 && dx<6 ) break; // ~t
        if(   loop(b,   0,8*dy/16,dx,cs,0,RI)>dx/8
           && loop(b,   0,      i,dx,cs,1,RI)>=dx-1 
           && loop(b,dx-1,8*dy/16,dx,cs,0,LE)>dx/8
           && loop(b,dx-1,    i-1,dx,cs,0,LE)>dx/8  ) break; // ~t
      }
// MM;
      if( vertical_detected && dx>5 )
      if(     loop(b,0,   1,dx,cs,0,RI)>=dx/2
         && ( loop(b,0,dy-2,dx,cs,0,RI)<=dx/8 
           || loop(b,0,dy-1,dx,cs,0,RI)<=dx/8 ) )
      if(   ( loop(b,dx-1,   0,dx,cs,0,LE)<=dx/8
           || loop(b,dx-1,   1,dx,cs,0,LE)<=dx/8 )
         &&   loop(b,dx-1,dy-2,dx,cs,0,LE)>=dx/2 ) break; // ~/
         
      if( get_bw(x0,x1,y0,y1,box1->p,cs,2) == 0 ) ad=99*ad/100;
// MM;

      if (!hchar){
        i=loop(b,0,dy/16  ,dx,cs,0,RI);
        i=loop(b,i,dy/16  ,dx,cs,1,RI); if (i>3*dx/4) break; // ~z
        i=loop(b,0,dy/16+1,dx,cs,0,RI);
        i=loop(b,i,dy/16+1,dx,cs,1,RI); if (i>3*dx/4) break; // ~z
        i=loop(b,0,dy/16+2,dx,cs,0,RI);
        i=loop(b,i,dy/16+2,dx,cs,1,RI); if (i>3*dx/4) break; // ~z
      }
// MM;

      if(!hchar) ad=ad*80/100;
      if( gchar) ad=ad*80/100;
      if (ad==100) ad--;  /* I have to fix that:
            .@@@@.<-
            @@..@@
            ....@@
            ....@@<
            ...@@.
            ..@@@.
            ..@@..
            .@@...
            @@....
            @@@@@@<-
      */
      setac(box1,(wchar_t)'l',ad);
      if( i<100 ) break;
      
      if(   loop(b,0,   1,dx,cs,0,RI)<=dx/8
         && loop(b,0,dy/2,dx,cs,0,RI)<=dx/8 
         && loop(b,0,dy-2,dx,cs,0,RI)<=dx/8 ) vertical_detected=1;
// MM;

      bc='l';
      break;
   }
   // --- test z -------
   if ( 0 )
   for(d=100;dx>3 && dy>3;){     // dy>dx
      if( get_bw(0        ,dx/8,dy/2,dy/2,b,cs,1) == 1 ) break;
      if( get_bw(dx-1-dx/8,dx-1,dy/2,dy/2,b,cs,1) == 1 ) break;
      if( get_bw(0        ,dx/4,0   ,dy/4,b,cs,1) != 1 ) break;
      if( get_bw(dx-1-dx/4,dx-1,0   ,dy/4,b,cs,1) != 1 ) break;
      if( get_bw(0        ,dx/4,dy-1-dy/4,dy-1,b,cs,1) != 1 ) break;
      if( get_bw(dx-1-dx/4,dx-1,dy-1-dy/4,dy-1,b,cs,1) != 1 ) break;
      if( get_bw(dx/3,dx-1-dx/3,     dy/2,dy/2,b,cs,1) != 1 ) break;
      if( dx<5 && num_cross(1   ,   1,0,dy-1,b,cs) != 2 ) break;
      if( dx>8 && num_cross(dx/2,dx/2,0,dy-1,b,cs) != 3 ) break;
      if(  num_cross(0   ,dx-1,dy/2+1,dy/2-1,b,cs) != 1 ) break;
      // out_x(box1);fprintf(stderr," z:");
      for( i=1,x=0;x<=dx/2 && i;x++ ){
        if( get_bw(x0+dx/3+x,x0+dx/3+x,y0     ,y0+dy/5,p,cs,1) == 0 ) i=0;
        if( get_bw(x0+dx/8+x,x0+dx/8+x,y1-dy/5,y1     ,p,cs,1) == 0 ) i=0;
      } if( !i ) break;
      // --- line from left down to right up 0003
      for(y=dy/8;y<dy-dy/8;y++){ x=dx-1-dx*y/dy;  // straight line ? 
        if( get_bw(x-dx/8,x+dx/8,y,y,b,cs,1)!=1 ) break; // pixel around ?
      } if(y<dy-dy/8) break;
      if(  loop(b,b->x-1,dy/16,dx,cs,0,LE)
         > loop(b,b->x-1,dy/ 8,dx,cs,0,LE)+(dx+16)/32 ) break; // ~2
      x=x0,y=y1;
      turmite(p,&x,&y,x0,x1,y0,y1,cs,UP,ST); if( y<y1-dy/3 ) break;
      turmite(p,&x,&y,x0,x1,y0,y1,cs,RI,UP);	//
      if( x<x0+dx/2 || y>y0 ) break;		// upper right edge?
      // --- vertikal line at right end
      x=x1,y=y0+dy/4;	// horizontal line can be very thick on tt-font
      turmite(p,&x,&y,x0,x1,y0,y1,cs,LE,ST); if( x<x1-dx/3 ) break;
      turmite(p,&x,&y,x0,x1,y0,y1,cs,ST,LE); if( x<x1-dx/2 ) break;
      // --- vertikal line at left end
      x=x0,y=y1-dy/4;
      turmite(p,&x,&y,x0,x1,y0,y1,cs,RI,ST); if( x>x0+dx/3 ) break;
      turmite(p,&x,&y,x0,x1,y0,y1,cs,ST,RI); if( x>x0+dx/2 ) break;

      // test ob rechte Kante ansteigend
      for(x=dx,y=b->y-1-dy/2;y>=dy/4;y--){ // rechts abfallende Kante/Knick?
        i=loop(b,b->x-1,y,x1-x0,cs,0,LE);
        if( i-2>=x ) break; if( i<x ) x=i;
      } if (y>=dy/4 ) break; 

      if( num_hole (x0  , x1  , y0, y1,p,cs,NULL) != 0 ) break; 

      bc='z'; if( hchar ) bc='Z';
      setac(box1,(wchar_t)bc,90);
      break;
   }
   return bc;
}
