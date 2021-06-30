/*
 the following code was send by Ryan Dibble <dibbler@umich.edu>
 
  The algorithm is very simple but works good hopefully.
 
  Compare the grayscale histogram with a mass density diagram:
  I think the algorithm is a kind of
  divide a body into two parts in a way that the mass 
  centers have the largest distance from each other,
  the function is weighted in a way that same masses have a advantage
  
  TODO:
    RGB: do the same with all colors (CMYG?) seperately 

    test: hardest case = two colors
       bbg: test done, using a two color gray file. Output:
       # OTSU: thresholdValue = 43 gmin=43 gmax=188

 my changes:
   - float -> double
   - debug option added (vvv & 1..2)
   - **image => *image,  &image[i][1] => &image[i*cols+1]

     Joerg.Schulenburg@physik.uni-magdeburg.de

 ToDo:
   - measure contrast
   - detect low-contrast regions

 */

#include <stdio.h>
#include <string.h>

/*======================================================================*/
/* OTSU global thresholding routine                                     */
/*   takes a 2D unsigned char array pointer, number of rows, and        */
/*   number of cols in the array. returns the value of the threshold    */
/*======================================================================*/
int
otsu (unsigned char *image, int rows, int cols, int x0, int y0, int dx, int dy, int vvv) {

  unsigned char *np;    // pointer to position in the image we are working with
  int thresholdValue=1; // value we will threshold at
  int ihist[256];       // image histogram

  int i, j, k;          // various counters
  int n, n1, n2, gmin, gmax;
  double m1, m2, sum, csum, fmax, sb;

  // zero out histogram ...
  memset(ihist, 0, sizeof(ihist));

  gmin=255; gmax=0; k=dy/512+1;
  // generate the histogram
  for (i =  0; i <  dy ; i+=k) {
    np = &image[(y0+i)*cols+x0];
    for (j = 0; j < dx ; j++) {
      ihist[*np]++;
      if(*np > gmax) gmax=*np;
      if(*np < gmin) gmin=*np;
      np++; /* next pixel */
    }
  }

  // set up everything
  sum = csum = 0.0;
  n = 0;

  for (k = 0; k <= 255; k++) {
    sum += (double) k * (double) ihist[k];  /* x*f(x) mass moment */
    n   += ihist[k];                        /*  f(x)    mass      */
  }

  if (!n) {
    // if n has no value we have problems...
    fprintf (stderr, "NOT NORMAL thresholdValue = 160\n");
    return (160);
  }

  // do the otsu global thresholding method

  fmax = -1.0;
  n1 = 0;
  for (k = 0; k < 255; k++) {
    n1 += ihist[k];
    if (!n1) { continue; }
    n2 = n - n1;
    if (n2 == 0) { break; }
    csum += (double) k *ihist[k];
    m1 = csum / n1;
    m2 = (sum - csum) / n2;
    sb = (double) n1 *(double) n2 *(m1 - m2) * (m1 - m2);
    /* bbg: note: can be optimized. */
    if (sb > fmax) {
      fmax = sb;
      thresholdValue = k;
    }
  }

  // at this point we have our thresholding value

  // debug code to display thresholding values
  if ( vvv & 1 )
  fprintf(stderr,"# OTSU: thresholdValue = %d gmin=%d gmax=%d\n",
     thresholdValue, gmin, gmax);

  return(thresholdValue);
}

/*======================================================================*/
/* thresholding the image  (set threshold to 128+32=160=0xA0)           */
/*   now we have a fixed thresholdValue good to recognize on gray image */
/*   - so lower bits can used for other things (bad design?)            */
/*======================================================================*/
int
thresholding (unsigned char *image, int rows, int cols, int x0, int y0, int dx, int dy, int thresholdValue) {

  unsigned char *np;    // pointer to position in the image we are working with

  int i, j;          // various counters
  int gmin=255,gmax=0;

  // calculate min/max (twice?)
  for (i = y0 + 1; i < y0 + dy - 1; i++) {
    np = &image[i*cols+x0+1];
    for (j = x0 + 1; j < x0 + dx - 1; j++) {
      if(*np > gmax) gmax=*np;
      if(*np < gmin) gmin=*np;
      np++; /* next pixel */
    }
  }
  
  if (thresholdValue<gmin || thresholdValue>=gmax){
    thresholdValue=(gmin+gmax)/2;
    fprintf(stderr,"# thresholdValue out of range %d..%d, reset to %d\n",
     gmin, gmax, thresholdValue);
  }

  /* b/w: min=0,tresh=0,max=1 */
  // actually performs the thresholding of the image...
  //  later: grayvalues should also be used, only rescaling threshold=160=0xA0
  for (i = y0; i < y0+dy; i++) {
    np = &image[i*cols+x0];
    for (j = x0; j < x0+dx; j++) {
      *np = (unsigned char) (*np > thresholdValue ?
         (255-(gmax - *np)* 80/(gmax - thresholdValue + 1)) :
         (  0+(*np - gmin)*150/(thresholdValue - gmin + 1)) );
      np++;
    }
  }

  return(thresholdValue);
}

