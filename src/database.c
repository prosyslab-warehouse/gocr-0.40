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

#include <stdio.h>
#include <stdlib.h>
#include "gocr.h"
#include "pnm.h"
#include "pgm2asc.h"
#include <string.h>
#include <time.h>

#define Blen  256

// load database into boxlist
// this is added in version v0.2.4 
//   as alternate engine comparing chars with database
int load_db(void) {
  FILE *f1;
  char s1[Blen+1],
       s2[Blen+1] = "./db/", /* replace by constant! defined by configure */
       *s3;
  int i, j, ii, i2;
  struct box *box1;
  pix *pp;

  if( JOB->cfg.db_path ) strncpy(s2,JOB->cfg.db_path,Blen-1);
  i2=strlen(s2);
  if (JOB->cfg.verbose)
    fprintf(stderr, "# load database %s %s ... ",s2,JOB->cfg.db_path);

  strncpy(s2+i2,"db.lst",Blen-i2);s2[Blen]=0;
  f1 = fopen(s2, "r");
  if (!f1) {
    fprintf(stderr, " DB %s not found\n",s2);
    return 1;
  }

  for (ii = 0; !feof(f1); ii++) {
/* bbg: should write a better input routine */
    if (!fgets(s1, Blen, f1)) break;
    j = strlen(s1);
    if (j == 0)
      continue;
    /* copy file name */
    for (i = 0; i < j && i+i2 < Blen && strchr(" \t,;",s1[i]) == 0; i++)
      s2[i2 + i] = s1[i];
    s2[i2+i]=0;
    /* skip spaces */
    for (; i < j && strchr(" \t",s1[i]) != 0; i++);
/* by now: read pix, fill box, goto next ??? */
    pp = (pix *)malloc(sizeof(pix));
    if( !pp ) fprintf(stderr,"malloc error in load_db pix\n");

    readpgm(s2, pp, 0 * JOB->cfg.verbose);

    box1 = (struct box *)malloc(sizeof(struct box));
    if(!box1) fprintf(stderr,"malloc error in load_db box1\n");
    box1->x0 = 0;
    box1->x1 = pp->x-1;       // white border 1 pixel width
    box1->y0 = 0;
    box1->y1 = pp->y-1;
    box1->x = 1;
    box1->y = 1;
    box1->dots = 0;
    box1->c = 0;
    box1->obj = NULL;
    if (s1[i]=='"'){
      j=strchr(s1+i+1,'"')-(s1+i+1);
      if (j>=1 && j<40) {
        s3=(char *)malloc(j+1);
        if (s3) {
          memcpy(s3,s1+i+1,j);
          s3[j]=0;
          box1->obj=s3;
          // fprintf(stderr,"\nstring=%s",s3);
        }
      }
    } else {
      box1->c = s1[i];      /* try to interpret as ASCII */
      s3=s1+i;
      j=strtol( s1+i, &s3, 16); /* try to read 4 to 8 digit hex unicode */
      if( j && i+3<=Blen && s3-s1-i>3 ) box1->c = j;
      // fprintf(stderr,"\nhexcode=%04x=%04x %d",(int)j,(int)box1->c,s3-s1-i);
    }
    box1->ac = '_';
    box1->num = 0;
    box1->line = -1;
    box1->m1 = 0;  /* should be given too in the database! change it */
    box1->m2 = 0;
    box1->m3 = 0;
    box1->m4 = 0;
    box1->p = pp;
    box1->num_ac=0;
    list_app(&JOB->tmp.dblist, box1);	// append to list
#if 0
     out_x(box1);
#endif
  }
  fclose(f1);
  if (JOB->cfg.verbose)
    fprintf(stderr, " %d chars loaded\n", ii);
  return 0;
}

// expand database from box/boxlist name=db_$utime.pbm
// this is added in version v0.3.3
int store_db(struct box *box1) {
  FILE *f1;
  char s2[Blen+1] = "./db/", s3[Blen+1];
  int i2, dx, dy;
  pix b;	/* temporary mini page */

  if( JOB->cfg.db_path ) strncpy(s2,JOB->cfg.db_path,Blen-1);
  i2=strlen(s2);
  if (JOB->cfg.verbose)
    fprintf(stderr, "# expand database %s ... ",s2);

  sprintf(s3,"db_%lu.pbm",(unsigned long)time(NULL));
  strncpy(s2+i2,"db.lst",Blen-i2);s2[Blen]=0;
  f1 = fopen(s2, "a");
  if (!f1) {
    fprintf(stderr, " could not access %s\n",s2);
    return 1;
  }
  strncpy(s2+i2,s3,strlen(s3));s2[i2+strlen(s3)]=0;
  /* store image and infos about the char */
  
  if (JOB->cfg.verbose)
    fprintf(stderr, "add file %s to database",s3);
  dx=box1->x1-box1->x0+1;
  dy=box1->y1-box1->y0+1;
  b.p = malloc( dx * dy * sizeof(unsigned char) );
  if( !b.p ){
    fprintf( stderr, "\nFATAL: malloc failed, skip store_db" );
    return 2;
  }
  if (copybox(box1->p, box1->x0, box1->y0, dx, dy, &b, dx * dy))
    return -1;
                          
  writepbm(s2,&b);
  free(b.p);
  
  // some infos about box1->m1,m2,m3,m4 should added (base line, high etc.)
  if (box1->obj) {
    fprintf(f1, "%s \"%s\"\n",s3,box1->obj);
  } else {
    if( (box1->c >= '0' && box1->c <= '9')
     || (box1->c >= 'A' && box1->c <= 'Z')
     || (box1->c >= 'a' && box1->c <= 'z') )
      fprintf(f1, "%s %c\n",s3,(char)box1->c);
    else {
      if (((box1->c)>>16)>>16)
        fprintf(f1, "%s %08x\n",s3,(unsigned int)box1->c);
      else
        fprintf(f1, "%s %04x\n",s3,(unsigned int)box1->c);
    }
  }
  
  fclose(f1);

  if (JOB->cfg.verbose)
    fprintf(stderr, " char stored\n");
  return 0;
}

/*
// second variant, for database (with slightly other behaviour)
// new variant
//  look at the environment of the pixel too (contrast etc.)
//   detailed analysis only of diff pixels!
//
// 100% * distance, 0 is best fit
// = similarity of 2 chars for recognition of noisy chars
//   weigth of pixels with only one same neighbour set to 0
//   look at contours too!
   ToDo: especially on small boxes distance should only be 0 if
       characters are 100% identical! 
*/
// #define DEBUG 2
int distance2( pix *p1, struct box *box1,
               pix *p2, struct box *box2, int cs){
   int rc=0,x,y,v1,v2,i1,i2,rgood=0,rbad=0,
       x1,y1,x2,y2,dx,dy,dx1,dy1,dx2,dy2,tx,ty;
#if DEBUG == 2
  if(JOB->cfg.verbose)
    fprintf(stderr," DEBUG: distance2\n");
#endif
   x1=box1->x0;y1=box1->y0;x2=box2->x0;y2=box2->y0;
   dx1=box1->x1-box1->x0+1; dx2=box2->x1-box2->x0+1; dx=((dx1>dx2)?dx1:dx2);dx=dx1;
   dy1=box1->y1-box1->y0+1; dy2=box2->y1-box2->y0+1; dy=((dy1>dy2)?dy1:dy2);dy=dy1;
   if(abs(dx1-dx2)>1+dx/16 || abs(dy1-dy2)>1+dy/16) rbad++; // how to weight?
   // compare relations to baseline and upper line
   if(box1->m4>0 && box2->m4>0){  // used ???
     if(2*box1->y1>box1->m3+box1->m4 && 2*box2->y1<box2->m3+box2->m4) rbad+=128;
     if(2*box1->y0>box1->m1+box1->m2 && 2*box2->y0<box2->m1+box2->m2) rbad+=128;
   }
   tx=dx/16; if(dx<17)tx=1; // raster
   ty=dy/32; if(dy<33)ty=1;
   // compare pixels
   for( y=0;y<dy;y+=ty )
   for( x=0;x<dx;x+=tx ) {	// try global shift too ???
     v1=((pixel(p1,x1+x*dx1/dx,y1+y*dy1/dy)<cs)?1:0); i1=8;	// better gray?
     v2=((pixel(p2,x2+x*dx2/dx,y2+y*dy2/dy)<cs)?1:0); i2=8;	// better gray?
     if(v1==v2) { rgood+=16; continue; } // all things are right!
     // what about different pixel???
     // test overlapp of surounding pixels ???
     v1=1; rbad+=4;
     v1=-1;
     for(i1=-1;i1<2;i1++)
     for(i2=-1;i2<2;i2++)if(i1!=0 || i2!=0){
       if( ((pixel(p1,x1+x*dx1/dx+i1*(1+dx1/32),y1+y*dy1/dy+i2*(1+dy1/32))<cs)?1:0)
         !=((pixel(p2,x2+x*dx2/dx+i1*(1+dx2/32),y2+y*dy2/dy+i2*(1+dy2/32))<cs)?1:0) ) v1++;
     }
     if(v1>0)
     rbad+=16*v1;
   }
   if(rgood+rbad) rc= 100*rbad/(rgood+rbad); else rc=99;
   /* if width/high is not correct add badness */
   rc += ( abs(dx1*dy2-dx2*dy1) * 10 ) / (dy1*dy2);
   if (rc>100) rc=100;
   if(/* rc<10 && */ JOB->cfg.verbose /* &1024 */){
#if DEBUG == 2
     fprintf(stderr," distance2 rc=%d rgood=%d rbad=%d\n",rc,rgood,rbad);
//     out_b(NULL,p1,box1->x0,box1->y0,box1->x1-box1->x0+1,
//                                box1->y1-box1->y0+1,cs);
//     out_b(NULL,p2,box2->x0,box2->y0,box2->x1-box2->x0+1,
//                                box2->y1-box2->y0+1,cs);
     out_x(box1);
     out_x(box2);
#endif
   }
   return rc;
}

wchar_t ocr_db(struct box *box1) {
  int dd = 1000, j, dist = 1000;
  wchar_t c = UNKNOWN;
  char buf[40], *s2;
  Box *box2, *box3;
  
  if (!list_empty(&JOB->tmp.dblist)){ 
    box3 = (Box *)list_get_header(&JOB->tmp.dblist);
    if(JOB->cfg.verbose)
      fprintf(stderr,"\n DEBUG: ocr_db");

    for_each_data(&JOB->tmp.dblist) {
      box2 = (Box *)list_get_current(&JOB->tmp.dblist);
      /* do preselect!!! distance() slowly */
      dd = distance2( box2->p, box2, box1->p, box1, JOB->cfg.cs);
      if (dd < dist) {
        dist = dd;
        c = box2->c;
        box3 = box2;
      }
      if (dd<=0) break; /* speedup if found */
    } end_for_each(&JOB->tmp.dblist);
    if (JOB->cfg.verbose)
      fprintf(stderr, " dist=%4d c= %c 0x%02x %s", dist,
         ((c>32 && c<127)?(char)c:'~'), (int)c, ((box3->obj)?box3->obj:""));
    if (dist > 5) { /* 5% deviation of the pattern is tolerated */
      setac(box1, c, 100-dist);
      c = UNKNOWN;
    }
    else {
      if (c && c!=UNKNOWN)
        setac(box1, c, 100-dist);
      if (box3->obj) {
        /* its not a good solution, but better then nothing: */
        if (box1->obj) free( box1->obj);
        box1->obj = (char *) malloc( strlen(box3->obj)+1 );
        if (box1->obj)
          memcpy( box1->obj, box3->obj, strlen(box3->obj)+1 );
      }
    }
  }

  if( (JOB->cfg.mode&128) != 0 && c == UNKNOWN ) for(;;) { // prompt user
    // should the output go to stdout or stderr or special pipe???
    out_x(box1);
    fprintf(stderr,"The upper char was not recognized. Enter correct ASCII char, \"string\" or 4 to 8 digit hex unicode: ");
    buf[0]=0; j=0;
    fgets(buf,40,stdin); /* including \n */
    dd=strlen(buf); if(dd>0 && buf[dd-1]=='\n') { dd--;buf[dd]=0; }
    if ( dd>=4 && dd<=8 ) j=strtol(buf,&s2,16);
    if( ((dd>1 && dd<4) || dd>8) || (dd>4 && (s2-buf<4 || s2-buf>8)) ){
      if (dd<2 || buf[0]!='"' || buf[dd-1]!='"') {
        fprintf(stderr,"invalid input\n");
        continue;
      }
    }
    if( dd==1 || (dd>=4 && dd<=8) || (dd>2 && buf[0]=='"' && buf[dd-1]=='"')){
      if (dd>1 && buf[0]=='"' && buf[dd-1]=='"') {
        /* store a combination of chars, a string (new) */
        c=PICTURE; /* probably a bad idea? */
        s2=(char *)malloc(dd);
        if (s2) { memcpy(s2,buf+1,dd-2); s2[dd-2]='\0'; }
        box1->obj=s2;
      } else {   /* store a single char */
        if(dd==1) c=255&(wchar_t)buf[0]; else c=j;
      }
      fprintf(stderr," Store the pattern? 0=no 1=into_memory 2=to_database: ");
      fgets(buf,40,stdin);
      if( buf[0]>='1' && buf[0]<='2' ){
        box1->c=c;
        box1->num_ac=0;
        list_app(&JOB->tmp.dblist, box1);	// append to list
      }
      if( buf[0]=='2' ){
        store_db(box1);
      }
    }
    break;
  }
  
  return c;
}
