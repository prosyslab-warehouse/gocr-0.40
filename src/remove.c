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
*/

#include <stdlib.h>
#include <stdio.h>
#include "pgm2asc.h"
#include "gocr.h"

/* measure mean thickness as an criteria for big chars */
int mean_thickness( struct box *box2 ){
  int mt=0, i, y, dx=box2->x1-box2->x0+1, dy;
  for (y=box2->y0+1; y<box2->y1; y++) {
    i=loop(box2->p,box2->x0+0,y,dx,JOB->cfg.cs,0,RI);
    i=loop(box2->p,box2->x0+i,y,dx,JOB->cfg.cs,1,RI);
    mt+=i;
  } 
  dy = box2->y1 - box2->y0 - 1; 
  if (dy) mt=(mt+dy/2)/dy;
  return mt;
}

/* ---- remove dust ---------------------------------
   What is dust? I think, this is a very small pixel cluster without
   neighbours. Of course not all dust clusters can be detected correct.
   This feature should be possible to switch off via option.
   -> may be, all clusters should be stored here?
   speed is very slow, I know, but I am happy that it is working well
*/
int remove_dust( job_t *job ){
  /* new dust removing  */
  /* FIXME jb:remove pp */
  pix *pp = &job->src.p;
  int i1,i,j,x,y,x0,x1,y0,y1, cs,vvv=JOB->cfg.verbose;
  struct box *box2;
#define HISTSIZE  100   /* histogramm */
  int histo[HISTSIZE];
  cs=job->cfg.cs;
  /*
   * count number of black pixels within a box and store it in .dots
   * later .dots is re-used for number of objects belonging to the character
   * should be done in the flood-fill algorithm 
   */
  for (i1=0;i1<HISTSIZE;i1++) histo[i1]=0;
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    box2->dots = 0;
    for(x=box2->x0;x<=box2->x1;x++)
      for(y=box2->y0;y<=box2->y1;y++){
        if( pixel(pp,x,y)<cs ){ box2->dots++; }
      }
    if (box2->dots<HISTSIZE) histo[box2->dots]++;
  } end_for_each(&(JOB->res.boxlist));
  
  if (job->cfg.dust_size < 0 && job->res.numC > 0) { /* auto detection */
    /* this formula is empirically, high resolution scans have bigger dust */
    /* get maximum */
    job->cfg.dust_size = ( job->res.sumX/job->res.numC )
                       * ( job->res.sumY/job->res.numC ) / 100 + 1;
    for (i=1;i+1<HISTSIZE && i<job->cfg.dust_size;i++){
      if (histo[i+1]>=histo[i]/2) break;
    }
    job->cfg.dust_size=i;
    /* what is the statistic of random dust? 
     *    if we have p pixels on a x*y image we should have
     *    (p/(x*y))^2 * (x*y) = p^2/(x*y) doublets and
     *    (p/(x*y))^3 * (x*y) = p^3/(x*y)^2 triplets
     */
    if (vvv) fprintf(stderr,"# auto dust size = %d (mX=%d,mY=%d)\n",
      job->cfg.dust_size,job->res.sumX/job->res.numC,
                         job->res.sumY/job->res.numC);
  }
  if (job->cfg.dust_size)
  { i=0;
    if(vvv){
       fprintf(stderr,"# remove dust of size %2d",job->cfg.dust_size);
       fprintf(stderr," histo=%d,%d(?=%d),%d(?=%d),...",
          histo[1],histo[2],histo[1]*histo[1]/(pp->x*pp->y),
          histo[3],histo[1]*histo[1]*histo[1]/(pp->x*pp->y*pp->x*pp->y)); 
    }
    i = 0;
    for_each_data(&(JOB->res.boxlist)) {
      box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
      x0=box2->x0;x1=box2->x1;y0=box2->y0;y1=box2->y1;	/* box */
      j=box2->dots;
      if(j<=job->cfg.dust_size)      /* remove this tiny object */
      { /* here we should distinguish dust and i-dots,
         * may be we should sort out dots to a seperate dot list and
         * after line detection decide, which is dust and which not
         * dust should be removed to make recognition easier (ToDo)
         */
#if 0
        if(get_bw((3*x0+x1)/4,(x0+3*x1)/4,y1+y1-y0+1,y1+8*(y1-y0+1),pp,cs,1)) 
            continue; /* this idea was to simple, see kscan003.jpg sample */
#endif
        /* remove from average */
        JOB->res.numC--;
        JOB->res.sumX-=x1-x0+1;
        JOB->res.sumY-=y1-y0+1;
        /* remove pixels (should only be done with dust) */
        for(x=x0;x<=x1;x++)
        for(y=y0;y<=y1;y++){ put(pp,x,y,0,255&~7); }
        /* remove from list */
	list_del(&(JOB->res.boxlist),box2);
	/* free memory */
	free(box2);
	i++; /* count as dust particle */
	continue;
      }
    } end_for_each(&(JOB->res.boxlist));
    if(vvv)fprintf(stderr," %3d cluster removed\n",i);
  }
  /* reset dots to 0 and remove white pixels (new) */
  i=0;
  for_each_data(&(JOB->res.boxlist)) {
    box2 = ((struct box *)list_get_current(&(JOB->res.boxlist)));
    box2->dots = 0;
    x0=box2->x0;x1=box2->x1;y0=box2->y0;y1=box2->y1;	/* box */
    if (x1-x0>16 && y1-y0>30) /* only on large enough chars */
    for(x=x0+1;x<=x1-1;x++)
    for(y=y0+1;y<=y1-1;y++){
      if( pixel_atp(pp,x  ,y  )>=cs
       && pixel_atp(pp,x-1,y  ) <cs 
       && pixel_atp(pp,x+1,y  ) <cs 
       && pixel_atp(pp,x  ,y-1) <cs 
       && pixel_atp(pp,x  ,y+1) <cs )  /* remove it */
      {
        put(pp,x,y,0,0); i++;  /* (x and 0) or 0 */
      }
    }
  } end_for_each(&(JOB->res.boxlist));
  if(vvv)fprintf(stderr,"# %3d white pixels removed, cs=%d\n",i,cs);
  return 0;
}

/* ---- smooth big chars ---------------------------------
 * Big chars often do not have smooth borders, which let fail
 * the engine. Here we smooth the borders of big chars (>7x16).
 * Smoothing is important for b/w scans, where we often have
 * comb like pattern on a vertikal border. I also received
 * samples with lot of white pixels (sample: 04/02/25).
 */
int smooth_borders( job_t *job ){
    pix *pp = &job->src.p;
    int ii=0,x,y,x0,x1,y0,y1,dx,dy,cs,i0,i1,i2,i3,i4,n1,n2,
        cn[8],cm,vvv=JOB->cfg.verbose; /* dust found */
    struct box *box2;
    cs=JOB->cfg.cs; n1=n2=0;
    if(vvv){ fprintf(stderr,"# smooth big chars 7x16 cs=%d",cs); }
    /* filter for each big box */
    for_each_data(&(JOB->res.boxlist)) { n2++; /* count boxes */
        box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
        /* do not touch small characters! but how we define small characters? */
        if (box2->x1-box2->x0+1<7 || box2->y1-box2->y0+1<16 ) continue;
        if (box2->c==PICTURE) continue;
        if (mean_thickness(box2)<3) continue;
        n1++; /* count boxes matching big-char criteria */
        x0=box2->x0;        y0=box2->y0;
        x1=box2->x1;        y1=box2->y1;
        dx=x1-x0+1;         dy=y1-y0-1;
        /* out_x(box2);
         * dont change to much! only change if absolutely sure!
         *             .......    1 2 3
         *       ex:   .?#####    0 * 4
         *             .......    7 6 5
         * we should also avoid removing lines by sytematic remove
         * from left end to the right, so we concern also about distance>1  
         */
        for(x=box2->x0;x<=box2->x1;x++)
         for(y=box2->y0;y<=box2->y1;y++){ /* filter out high frequencies */
           /* this is a very primitive solution, only for learning */
           cn[0]=pixel(pp,x-1,y);
           cn[4]=pixel(pp,x+1,y);   /* horizontal */
           cn[2]=pixel(pp,x,y-1);
           cn[6]=pixel(pp,x,y+1);   /* vertical */
           cn[1]=pixel(pp,x-1,y-1);
           cn[3]=pixel(pp,x+1,y-1); /* diagonal */
           cn[7]=pixel(pp,x-1,y+1);
           cn[5]=pixel(pp,x+1,y+1);
           cm=pixel(pp,x,y);
           /* check for 5 other and 3 same surrounding pixels */
           for (i0=0;i0<8;i0++)
             if ((cn[i0            ]<cs)==(cm<cs)
              && (cn[(i0+7)     & 7]<cs)!=(cm<cs)) break; /* first same */
           for (i1=0;i1<8;i1++)
             if ((cn[(i0+i1)    & 7]<cs)!=(cm<cs)) break; /* num same */
           for (i2=0;i2<8;i2++)
             if ((cn[(i0+i1+i2) & 7]<cs)==(cm<cs)) break; /* num other */
           cn[0]=pixel(pp,x-2,y);
           cn[4]=pixel(pp,x+2,y);   /* horizontal */
           cn[2]=pixel(pp,x,y-2);
           cn[6]=pixel(pp,x,y+2);   /* vertical */
           cn[1]=pixel(pp,x-2,y-2);
           cn[3]=pixel(pp,x+2,y-2); /* diagonal */
           cn[7]=pixel(pp,x-2,y+2);
           cn[5]=pixel(pp,x+2,y+2);
           /* check for 5 other and 3 same surrounding pixels */
           for (i0=0;i0<8;i0++)
             if ((cn[i0            ]<cs)==(cm<cs)
              && (cn[(i0+7)     & 7]<cs)!=(cm<cs)) break; /* first same */
           for (i3=0;i3<8;i3++)
             if ((cn[(i0+i3)    & 7]<cs)!=(cm<cs)) break; /* num same */
           for (i4=0;i4<8;i4++)
             if ((cn[(i0+i3+i4) & 7]<cs)==(cm<cs)) break; /* num other */
           if (i1<=3 && i2>=5 && i3>=3 && i4>=3) { /* change only on borders */
             ii++;             /*   white    : black */
             put(pp,x,y,0,((cm<cs)?(cs|32):cs/2)&~7);
#if 0
             printf(" x y i0 i1 i2 i3 i4 cm new cs %3d %3d"
             "  %3d %3d %3d %3d %3d  %3d %3d %3d\n",
              x-box2->x0,y-box2->y0,i0,i1,i2,i3,i3,cm,pixel(pp,x,y),cs);
#endif
           }
        }
#if 0  /* debugging */
        out_x(box2);
#endif
    } end_for_each(&(JOB->res.boxlist));
    if(vvv)fprintf(stderr," ... %3d changes in %d of %d\n",ii,n1,n2);
    return 0;
}

/* test if a corner of box1 is within box2 */
int box_nested( struct box *box1, struct box *box2){
             /* box1 in box2, +1..-1 frame for pixel-patterns */
 if (   (    ( box1->x0>=box2->x0-1 && box1->x0<=box2->x1+1 )
          || ( box1->x1>=box2->x0-1 && box1->x1<=box2->x1+1 ) )
     && (    ( box1->y0>=box2->y0-1 && box1->y0<=box2->y1+1 )
          || ( box1->y1>=box2->y0-1 && box1->y1<=box2->y1+1 ) ) )
   return 1;
 return 0;
}

/* test if box1 is within box2 */
int box_covered( struct box *box1, struct box *box2){
             /* box1 in box2, +1..-1 frame for pixel-patterns */
   if (     ( box1->x0>=box2->x0-1 && box1->x1<=box2->x1+1 )
         && ( box1->y0>=box2->y0-1 && box1->y1<=box2->y1+1 ) )
   return 1;
 return 0;
}

/* ---- remove pictures ------------------------------------------
 *   may be, not deleting or moving to another list is much better!
 */
int remove_pictures( job_t *job){
  struct box *box4,*box2;
  int j=0, j2=0, num_del=0;

  if (JOB->cfg.verbose)
    fprintf(stderr, "# remove boxes on border");

  /* output a list for picture handle scripts */
  j=0; j2=0;
  if(JOB->cfg.verbose)
  for_each_data(&(JOB->res.boxlist)) {
    box4 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if( box4->c==PICTURE ) j++; else j2++;
  } end_for_each(&(JOB->res.boxlist));
  if (JOB->cfg.verbose)
    fprintf(stderr," pictures= %d  rest= %d  boxes?= %d\n",j,j2,JOB->res.numC);

  /* remove dark-border-boxes (typical for copy of book site) */
  for(j=1;j;){ j=0; /* this is only because list_del does not work */
    for_each_data(&(JOB->res.boxlist)) {
      box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
      if( box2->c==PICTURE
       && ( (box2->x0==0            || box2->y0==0)  /* on border? */
         || (box2->x1==box2->p->x-1 || box2->y1==box2->p->y-1)
         || (box2->x1-box2->x0+1>box2->p->x/2  /* big table? */
          && box2->y1-box2->y0+1>box2->p->y/2)
          )
      ){ j=0;
        /* count boxes nested with the picture */
        for_each_data(&(JOB->res.boxlist)) {
          box4 = (struct box *)list_get_current(&(JOB->res.boxlist));
          if( box4 != box2 )  /* not count itself */
          if (box_nested(box4,box2)) j++;  /* box4 in box2 */
        } end_for_each(&(JOB->res.boxlist));
        if( j>8 ){ /* remove box if more than 8 chars are within box */
          list_del(&(JOB->res.boxlist), box2); /* does not work proper ?! */
          if (box2->obj) free(box2->obj);
          free(box2); j=1; num_del++; break;
        } else j=0;
      }
    } end_for_each(&(JOB->res.boxlist));
  }

  if (JOB->cfg.verbose)
    fprintf(stderr, " deleted= %d, within pictures ", num_del);
  num_del=0;

  /* output a list for picture handle scripts */
  j=0; j2=0;
  if(JOB->cfg.verbose)
  for_each_data(&(JOB->res.boxlist)) {
    box4 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if( box4->c==PICTURE ) j++; else j2++;
  } end_for_each(&(JOB->res.boxlist));
  if (JOB->cfg.verbose)
    fprintf(stderr," pictures= %d  rest= %d  boxes?= %d\n",j,j2,JOB->res.numC);
  
  for(j=1;j;){ j=0;  /* this is only because list_del does not work */
    if (JOB->cfg.verbose)
      fprintf(stderr, ".");  /* can be slow on gray images */
    for_each_data(&(JOB->res.boxlist)) {
      box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
      if( box2->c==PICTURE && !box2->obj)
      for(j=1;j;){ // let it grow to max before leave
        j=0; box4=NULL;
        /* find boxes nested with the picture and remove */
        /* its for pictures build by compounds */
        for_each_data(&(JOB->res.boxlist)) {
          box4 = (struct box *)list_get_current(&(JOB->res.boxlist));
          if(  box4!=box2   /* not destroy self */
           && (!box4->obj)  /* dont remove barcodes and special objects */ 
           && (/* box4->c==UNKNOWN || */
                  box4->c==PICTURE) ) /* dont remove valid chars */
          if(
             /* box4 in box2, +1..-1 frame for pixel-patterns */
               box_nested(box4,box2)
             /* or box2 in box4 */
            || box_nested(box2,box4) /* same? */
              )
          if (  box4->x1-box4->x0+1>2*JOB->res.avX
             || box4->x1-box4->x0+1<JOB->res.avX/2
             || box4->y1-box4->y0+1>2*JOB->res.avY
             || box4->y1-box4->y0+1<JOB->res.avY/2
             || box_covered(box4,box2) )   /* box4 completely within box2 */ 
            /* dont remove chars! see rotate45.fig */
          {
            // do not remove boxes in inner loop (bug?) ToDo: check why!
            // instead we leave inner loop and mark box4 as valid
            if( box4->x0<box2->x0 ) box2->x0=box4->x0;
            if( box4->x1>box2->x1 ) box2->x1=box4->x1;
            if( box4->y0<box2->y0 ) box2->y0=box4->y0;
            if( box4->y1>box2->y1 ) box2->y1=box4->y1;
            j=1;   // mark box4 as valid
            break; // and leave inner loop
          }
        } end_for_each(&(JOB->res.boxlist));
        if (j!=0 && box4!=NULL) { // check for valid box4
          list_del(&(JOB->res.boxlist), box4); /* does not work proper ?! */
          free(box4); // break;  // ToDo: necessary to leave after del???
          num_del++;
        }
        
      }
    } end_for_each(&(JOB->res.boxlist)); 
  }

  if (JOB->cfg.verbose)
    fprintf(stderr, " deleted= %d,", num_del);

  /* output a list for picture handle scripts */
  j=0; j2=0;
  if(JOB->cfg.verbose)
  for_each_data(&(JOB->res.boxlist)) {
    box4 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if( box4->c==PICTURE ) {
      fprintf(stderr,"\n# ... found picture at %4d %4d size %4d %4d",
         box4->x0, box4->y0, box4->x1-box4->x0+1, box4->y1-box4->y0+1 );
      j++;
    } else j2++;
  } end_for_each(&(JOB->res.boxlist));
  if (JOB->cfg.verbose)
    fprintf(stderr," pictures= %d  rest= %d  boxes?= %d\n",j,j2,JOB->res.numC);
  return 0;
}



  /* ---- remove melted serifs --------------------------------- v0.2.5
                >>v<<
        ##########.######## <-y0
        ###################  like X VW etc.
        ...###.......###... <-y
        ...###......###....
        j1       j2      j3
  - can generate new boxes if two characters were glued
  */
int remove_melted_serifs( pix *pp ){
  int x,y,j1,j2,j3,j4,i2,i3,i,ii,cs,x0,x1,xa,xb,y0,y1,vvv=JOB->cfg.verbose;
  struct box *box2, *box3;

  cs=JOB->cfg.cs; i=0; ii=0;
  if(vvv){ fprintf(stderr,"# searching melted serifs ..."); }
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if (box2->c != UNKNOWN) continue; /* dont try on pictures */
    x0=box2->x0; x1=box2->x1; 
    y0=box2->y0; y1=box2->y1;	// box
    // upper serifs
    for(j1=x0;j1+4<x1;){
      j1+=loop(pp,j1,y0  ,x1-x0,cs,0,RI);
      x  =loop(pp,j1,y0  ,x1-x0,cs,1,RI);	       if(j1+x>x1+1) break;
      y  =loop(pp,j1,y0+1,x1-x0,cs,1,RI); if(y>x) x=y; if(j1+x>x1+1) break;
      // measure mean thickness of serif
      for(j2=j3=j4=0,i2=j1;i2<j1+x;i2++){
        i3 =loop(pp,j1,y0   ,y1-y0,cs,0,DO); if(8*i3>y1-y0) break;
        i3+=loop(pp,j1,y0+i3,y1-y0,cs,1,DO); if(8*i3>y1-y0) break;
        if(8*i3<y1-y0){ j2+=i3; j3++; }
      } if(j3==0){ j1+=x; continue; } 
      y = y0+(j2+j3-1)/j3+(y1-y0+1)/32;

      // check if really melted serifs
      if( loop(pp,j1,y,x1-x0,cs,0,RI)<1 ) { j1+=x; continue; }
      if(num_cross(j1 ,j1+x,y,y,pp,cs) < 2 ){ j1+=x;continue; }
      j2 = j1 + loop(pp,j1,y,x1-x0,cs,0,RI);
      j2 = j2 + loop(pp,j2,y,x1-x0,cs,1,RI);
      i3 =      loop(pp,j2,y,x1-x0,cs,0,RI); if(i3<2){j1+=x;continue;}
      j2 += i3/2;
      j3 = j2 + loop(pp,j2,y  ,x1-j2,cs,0,RI);
      i3 = j2 + loop(pp,j2,y+1,x1-j2,cs,0,RI); if(i3>j3)j3=i3;
      j3 = j3 + loop(pp,j3,y  ,x1-j3,cs,1,RI);
      i3 =      loop(pp,j3,y  ,x1-j3,cs,0,RI); 
      if(i3<2 || j3>=j1+x){j1+=x;continue;}
      j3 += i3/2;

      if(x>5)
      {
	i++; // snip!
	for(y=0;y<(y1-y0+1+4)/8;y++)put(pp,j2,y0+y,255,128+64); // clear highest bit
	if(vvv&4){ 
	  fprintf(stderr,"\n"); 
	  out_x(box2);
	  fprintf(stderr,"# melted serifs corrected on %d %d j1=%d j3=%d",j2-x0,y-y0,j1-x0,j3-x0);
	}
	for(xb=0,xa=0;xa<(x1-x0+4)/8;xa++){ /* detect vertical gap */
	  i3=y1; 
	  if(box2->m3>y0 && 2*y1>box2->m3+box2->m4) i3=box2->m3; // some IJ
          if( loop(pp,j2-xa,i3,i3-y0,cs,0,UP) > (y1-y0+1)/2
           && loop(pp,j2,(y0+y1)/2,xa+1,cs,0,LE) >=xa ){ xb=-xa; break; }
          if( loop(pp,j2+xa,i3,i3-y0,cs,0,UP) > (y1-y0+1)/2 
           && loop(pp,j2,(y0+y1)/2,xa+1,cs,0,RI) >=xa ){ xb= xa; break; }
        }
	if( get_bw(j2   ,j2   ,y0,(y0+y1)/2,pp,cs,1) == 0
	 && get_bw(j2+xb,j2+xb,(y0+y1)/2,i3,pp,cs,1) == 0 )
	{ /* divide */
	  box3=malloc_box(box2);
	  box3->x1=j2-1;
	  box2->x0=j2+1; x1=box2->x1;
	  box3->num=JOB->res.numC;
	  box3->obj=NULL;
	  list_ins(&(JOB->res.boxlist),box2,box3); JOB->res.numC++; ii++; /* insert box3 before box2 */
	  if(vvv&4) fprintf(stderr," => splitted");
	  j1=x0=box2->x0; x=0; /* hopefully ok, UVW */
	}
      }
      j1+=x;
    }
    // same on lower serifs -- change this later to better function
    //   ####    ###
    //    #### v ###       # <-y
    //  #################### <-y1
    //  j1     j2     j3
    for(j1=x0;j1<x1;){
      j1+=loop(pp,j1,y1  ,x1-x0,cs,0,RI);
      x  =loop(pp,j1,y1  ,x1-x0,cs,1,RI);	       if(j1+x>x1+1) break;
      y  =loop(pp,j1,y1-1,x1-x0,cs,1,RI); if(y>x) x=y; if(j1+x>x1+1) break;
      // measure mean thickness of serif
      for(j2=j3=j4=0,i2=j1;i2<j1+x;i2++){
        i3 =loop(pp,j1,y1   ,y1-y0,cs,0,UP); if(8*i3>y1-y0) break;
        i3+=loop(pp,j1,y1-i3,y1-y0,cs,1,UP); if(8*i3>y1-y0) break;
        if(8*i3<y1-y0){ j2+=i3; j3++; }
      } if(j3==0){ j1+=x; continue; } 
      y = y1-(j2+j3-1)/j3-(y1-y0+1)/32;

      // check if really melted serifs
      if( loop(pp,j1,y,x1-x0,cs,0,RI)<1 ) { j1+=x; continue; }
      if(num_cross(j1 ,j1+x,y,y,pp,cs) < 2 ){ j1+=x;continue; }
      j2 = j1 + loop(pp,j1,y,x1-x0,cs,0,RI);
      j2 = j2 + loop(pp,j2,y,x1-x0,cs,1,RI);
      i3 =      loop(pp,j2,y,x1-x0,cs,0,RI); if(i3<2){j1+=x;continue;}
      j2 += i3/2;
      j3 = j2 + loop(pp,j2,y  ,x1-j2,cs,0,RI);
      i3 = j2 + loop(pp,j2,y-1,x1-j2,cs,0,RI); if(i3>j3)j3=i3;
      j3 = j3 + loop(pp,j3,y  ,x1-j3,cs,1,RI);
      i3 =      loop(pp,j3,y,x1-j3,cs,0,RI); 
      if(i3<2 || j3>=j1+x){j1+=x;continue;}
      j3 += i3/2;

      // y  =y1-(y1-y0+1+4)/8; 
      if(x>5)
      {
	i++; // snip!
	for(i3=0;i3<(y1-y0+1+4)/8;i3++)
	  put(pp,j2,y1-i3,255,128+64); // clear highest bit
	if(vvv&4){ 
	  fprintf(stderr,"\n");
	  out_x(box2);
	  fprintf(stderr,"# melted serifs corrected on %d %d j1=%d j3=%d",j2-x0,y-y0,j1-x0,j3-x0);
	}
	for(xb=0,xa=0;xa<(x1-x0+4)/8;xa++){ /* detect vertical gap */
          if( loop(pp,j2-xa,y0,y1-y0,cs,0,DO) > (y1-y0+1)/2
           && loop(pp,j2,(y0+y1)/2,xa+1,cs,0,LE) >=xa ){ xb=-xa; break; }
          if( loop(pp,j2+xa,y0,y1-y0,cs,0,DO) > (y1-y0+1)/2 
           && loop(pp,j2,(y0+y1)/2,xa+1,cs,0,RI) >=xa ){ xb= xa; break; }
        }
	if( get_bw(j2   ,j2   ,(y0+y1)/2,y1,pp,cs,1) == 0
	 && get_bw(j2+xb,j2+xb,y0,(y0+y1)/2,pp,cs,1) == 0 )
	{ /* divide */
	  box3=malloc_box(box2);
	  box3->x1=j2-1;
	  box2->x0=j2; x1=box2->x1;
	  box3->num=JOB->res.numC;
	  box3->obj=0;
	  list_ins(&(JOB->res.boxlist),box2,box3); JOB->res.numC++; ii++;
	  /* box3,box2 in correct order??? */
	  if(vvv&4) fprintf(stderr," => splitted");
	  j1=x0=box2->x0; x=0; /* hopefully ok, NMK */
	}
      }
      j1+=x;
    }
  } end_for_each(&(JOB->res.boxlist));
  if(vvv)fprintf(stderr," %3d cluster corrected, %d new boxes\n",i,ii);
  return 0;
}

/*  remove black borders often seen on bad scanned copies of books
    - dust around the border
 */
int remove_rest_of_dust() {
  int i = 0, j = 0, vvv = JOB->cfg.verbose, x0, x1, y0, y1;
  struct box *box2, *box4;

  if (vvv) {
    fprintf(stderr, "# detect dust2, ... ");
  }
  /* remove fragments from border */
  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if (box2->c == UNKNOWN) {
      x0 = box2->x0; x1 = box2->x1;
      y0 = box2->y0; y1 = box2->y1;	// box
      // box in char ???
      if (  2 * JOB->res.numC * (y1 - y0 + 1) < 3 * JOB->res.sumY
        && ( y1 < box2->p->y/4 || y0 > 3*box2->p->y/4 )  /* not single line */
        && JOB->res.numC > 1		/* do not remove everything */
        && ( box2->m4 == 0 ) )	/* remove this */
      {
	 JOB->res.numC--;
	 i++;
	 list_del(&(JOB->res.boxlist), box2);
	 free(box2);
      }
    }
  } end_for_each(&(JOB->res.boxlist));

  for_each_data(&(JOB->res.boxlist)) {
    box2 = (struct box *)list_get_current(&(JOB->res.boxlist));
    if (box2->c != PICTURE) {
      x0 = box2->x0; x1 = box2->x1;
      y0 = box2->y0; y1 = box2->y1;	// box
      // box in char ???
      if ( JOB->res.numC * (x1 - x0 + 1) < 4 * JOB->res.sumX
        || JOB->res.numC * (y1 - y0 + 1) < 4 * JOB->res.sumY )	/* remove this */
	for_each_data(&(JOB->res.boxlist)) {
	  box4 = (struct box *)list_get_current(&(JOB->res.boxlist));
	  if (box4 != box2 && JOB->res.numC > 1) {
	    if ( box4->x0 >= x0 && box4->x1 <= x1
	      && box4->y0 >= y0 && box4->y1 <= y1 ) {	/* remove this */
	      JOB->res.numC--;
	      j++;
	      list_del(&(JOB->res.boxlist), box4);
	      free(box4);
	    }
	  }
	} end_for_each(&(JOB->res.boxlist));
    }
  } end_for_each(&(JOB->res.boxlist));
  if (vvv)
    fprintf(stderr, " %3d + %3d boxes deleted, numC= %d\n", i, j, JOB->res.numC);

  return 0;
}
