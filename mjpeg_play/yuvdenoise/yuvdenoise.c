/*
 * Yuv-Denoiser to be used with Andrew Stevens mpeg2enc
 *
 * (C)2001 S.Fendt 
 *
 * Licensed and protected by the GNU-General-Public-License version 2 
 * (or if you prefer any later version of that license). See the file 
 * LICENSE for detailed information.
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "mjpeg_types.h"

/* #define DEBUG */ 
#define BLOCKSIZE   8
#define BLOCKOFFSET (BLOCKSIZE-(BLOCKSIZE/2))/2

#define MAGIC "YUV4MPEG"

/* we need several buffers. defineing them here is not
   very nice coding as it wastes memory but it's fast
   and doesn't produce any leaks (and modern systems
   should really have enough memory anyways :-} ...)

   HINT: This of course will go away if the filter is
   more stable ... bet for it ! But at present there are
   other problems to solve than an elegant memory 
   management...
*/

uint8_t version[] = "0.0.63\0";

uint8_t YUV1[(int) (768 * 576 * 1.5)];
uint8_t YUV2[(int) (768 * 576 * 1.5)];
uint8_t YUV3[(int) (768 * 576 * 1.5)];
uint8_t SUBO2[(int) (768 * 576 * 1.5)];
uint8_t SUBA2[(int) (768 * 576 * 1.5)];
uint8_t SUBO4[(int) (768 * 576 * 1.5)];
uint8_t SUBA4[(int) (768 * 576 * 1.5)];
uint8_t SUBO8[(int) (768 * 576 * 1.5)];
uint8_t SUBA8[(int) (768 * 576 * 1.5)];
uint8_t DEL0[(int) (768 * 576 * 1.5)];
uint8_t DEL1[(int) (768 * 576 * 1.5)];
uint8_t DEL2[(int) (768 * 576 * 1.5)];
uint8_t DEL3[(int) (768 * 576 * 1.5)];
uint8_t DEL4[(int) (768 * 576 * 1.5)];
uint8_t DEL5[(int) (768 * 576 * 1.5)];
uint8_t DEL6[(int) (768 * 576 * 1.5)];
uint8_t DEL7[(int) (768 * 576 * 1.5)];
uint8_t DEL8[(int) (768 * 576 * 1.5)];
uint8_t AVRG[(int) (768 * 576 * 1.5)];
uint32_t framenr = 0;

int width;			/* width of the images */
int height;			/* height of the images */
int u_offset;			/* offset of the U-component from beginning of the image */
int v_offset;			/* offset of the V-component from beginning of the image */
int uv_width;			/* width of the UV-components */
int uv_height;			/* height of the UV-components */
int best_match_x[4];		/* best block-match's X-coordinate in half-pixels */
int best_match_y[4];		/* best block-match's Y-coordinate in half-pixels */
uint32_t SAD[4];		/* best block-match's sum of absolute differences */
uint32_t CSAD;	   	        /* best block-match's sum of absolute differences */
uint32_t SQD;			/* best block-match's sum of absolute differences */
uint32_t YSQD;			/* best block-match's sum of absolute differences */
uint32_t USQD;			/* best block-match's sum of absolute differences */
uint32_t VSQD;			/* best block-match's sum of absolute differences */
uint32_t CQD;			/* center block-match's sum of absolute differences */
uint32_t init_SQD = 0;
int lum_delta;
int cru_delta;
int crv_delta;
int search_radius = 64;	        /* initial search-radius in half-pixels */
int border = -1;
double block_quality;
float sharpen=0;

float a[16];
int i;
float v;

void denoise_image ();
void detect_black_borders ();
void mb_search_88 (int x, int y);
void mb_search (int x, int y);
void subsample_reference_image2 ();
void subsample_averaged_image2 ();
void subsample_reference_image4 ();
void subsample_averaged_image4 ();
void delay_images ();
void time_average_images ();
void display_greeting ();
void display_help ();
void copy_filtered_block(int x, int y);

void noise_it()
{ /* TEST-Routine um Bilder gezielt und stark zu verrauschen...*/

    int b;
    int v;

    for(b=0;b<(width*height*1.5);b++)
    {
	v=(YUV1[b]+((float)rand()/(float)RAND_MAX)*32-16);
	v=(v>255)? 255:v;
	v=(v<0  )? 0  :v;

	YUV1[b]=v;
    }
}

void test_line()
{
    static int x;
    int y;

    int xx,yy;

    for(yy=0;yy<height;yy++)
    for(xx=0;xx<width;xx++)
    {
	YUV1[xx+yy*width]=0;
	YUV1[xx/2+yy/2*uv_width+u_offset]=128;
	YUV1[xx/2+yy/2*uv_width+v_offset]=128;
    }

    for(y=0;y<(height/2);y++)
    {
	YUV1[0+x+y*width]=155;
	YUV1[3+x+y*width]=155;
	YUV1[4+x+y*width]=155;
	YUV1[5+x+y*width]=155;
	YUV1[8+x+y*width]=155;
    }

    x+=2;
    x=(x>(width/2))? 0:x;
}

int
main (int argc, char *argv[])
{
  int norm;
  uint8_t magic[256];
  char c;

  display_greeting ();

  while ((c = getopt (argc, argv, "b:m:s:h")) != -1)
    {
      switch (c)
	{
	case 'b':
	  border = atoi (optarg);
	  fprintf (stderr,
		   "---  Border-Automation deactivated. Border set to %i.\n",
		   border);
	  break;
	case 'm':
	  init_SQD = atoi (optarg);
	  fprintf (stderr, "---  Initial mean-SQD set to %i.\n", init_SQD);
	  break;
	case 's':
	  sharpen = (float)(atoi (optarg))/100;
	  fprintf (stderr, "---  Sharpen-filter used\n");
	  break;
	case 'h':
	  display_help ();
	  break;
	}
    }

  /* read the YUV4MPEG header */

  if (0 == (fscanf (stdin, "%s %i %i %i", &magic[0], &width, &height, &norm)))
    {
      fprintf (stderr, "### yuvdenoise ### : Error, no header found\n");
      exit (-1);
    }
  else
    {
      if (0 != strcmp (magic, MAGIC))
	{
	  fprintf (stderr,
		   "### yuv3Dfilter ### : Error, magic number is not 'YUV4MPEG'.\n");
	  exit (-1);
	}
    }
  getc (stdin);			/* read newline character */

  /* write new header */

  fprintf (stdout, "YUV4MPEG %3i %3i %1i \n", width, height, norm);

  /* setting global variables */
  u_offset = width * height;
  v_offset = width * height * 1.25;
  uv_width = width >> 1;
  uv_height = height >> 1;

  /* read frames until they're all done... */

  while (fread (magic, 6, 1, stdin) && strcmp (magic, "FRAME"))
    {
      fprintf (stderr, "---  [FRAME]  --------------------------------\n");

      /* read one frame */
      if (0 == fread (YUV1, (width * height * 1.5), 1, stdin))
	{
	  fprintf (stderr,
		   "### yuv3Dfilter ### : Error, unexpected end of input!\n");
	  exit (-1);
	}

      /* Do whatever ... */
      /* noise_it(); */
      /* test_line(); */
      denoise_image ();
      framenr++;

      /* write one frame */
      fprintf (stdout, "FRAME\n");
      fwrite (YUV2, (width * height * 1.5), 1, stdout);
    }
  return (0);
}

void
display_greeting ()
{
  fprintf (stderr,
	   "\n\n\n-----------------------------------------------------------------\n");
  fprintf (stderr,
	   "---      YUV4MPEG  Motion-Compensating-Movie-Denoiser         ---\n");
  fprintf (stderr,
	   "---      Version %6s (this is a developer's version)       ---\n",
	   version);
  fprintf (stderr,
	   "-----------------------------------------------------------------\n");
  fprintf (stderr, "\n");
  fprintf (stderr, " 2001 by Stefan Fendt <stefan@lionfish.ping.de>\n");
  fprintf (stderr,
	   " Use this at your own risk. Your milage may vary. You have been\n");
  fprintf (stderr,
	   " warned. If it totally corrupts your movies... That's life.\n");
  fprintf (stderr, "\n");
}

void
display_help ()
{
  fprintf (stderr, "\n\nOptions:\n");
  fprintf (stderr, "--------\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "-h         This message. :)\n\n");
  fprintf (stderr, "-b [num]   Set frame-border to [num] lines.\n");
  fprintf (stderr,
	   "           These lines from top (respectively from bottom) of the\n");
  fprintf (stderr,
	   "           processed frames turn into pure black to be encoded better.\n\n");
  fprintf (stderr, "-m [num]   Set initial mean-SQD \n");
  fprintf (stderr,
	   "           This is most useful if you have to start directly in a new\n");
  fprintf (stderr, "           scene with quite heavy motion.\n\n");
  exit (0);
}

/* OLD */
#if 0
void
time_average_images ()
{
  int x, y;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
	AVRG[x + y * width] =
	  (uint16_t) (DEL1[x + y * width] +
		      DEL2[x + y * width] +
		      DEL3[x + y * width] +
		      DEL4[x + y * width] +
		      DEL5[x + y * width] +
		      DEL6[x + y * width] +
		      DEL7[x + y * width] + DEL8[x + y * width]) >> 3;
      }
  for (y = 0; y < uv_height; y++)
    for (x = 0; x < uv_width; x++)
      {
	AVRG[x + y * uv_width + u_offset] =
	  (uint16_t) (DEL1[x + y * uv_width + u_offset] +
		      DEL2[x + y * uv_width + u_offset] +
		      DEL3[x + y * uv_width + u_offset] +
		      DEL4[x + y * uv_width + u_offset] +
		      DEL5[x + y * uv_width + u_offset] +
		      DEL6[x + y * uv_width + u_offset] +
		      DEL7[x + y * uv_width + u_offset] +
		      DEL8[x + y * uv_width + u_offset]) >> 3;
	AVRG[x + y * uv_width + v_offset] =
	  (uint16_t) (DEL1[x + y * uv_width + v_offset] +
		      DEL2[x + y * uv_width + v_offset] +
		      DEL3[x + y * uv_width + v_offset] +
		      DEL4[x + y * uv_width + v_offset] +
		      DEL5[x + y * uv_width + v_offset] +
		      DEL6[x + y * uv_width + v_offset] +
		      DEL7[x + y * uv_width + v_offset] +
		      DEL8[x + y * uv_width + v_offset]) >> 3;
      }
}
#endif
void
time_average_images ()
{
  int x, y;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
	AVRG[x + y * width] =
	    (
		YUV2[x + y * width]*3+
		YUV1[x + y * width]*1
		)/4;
      }
  for (y = 0; y < uv_height; y++)
    for (x = 0; x < uv_width; x++)
      {
	AVRG[x + y * uv_width + u_offset] =
	    ( YUV2[x + y * uv_width + u_offset]*3+
	      YUV1[x + y * uv_width + u_offset]*1)/4;

	AVRG[x + y * uv_width + v_offset] =
	    ( YUV2[x + y * uv_width + v_offset]*3+
	      YUV1[x + y * uv_width + v_offset]*1)/4;
      }
}

void
subsample_image (uint8_t* dest, uint8_t* source)
{
  int x, y;

  /* subsample Y */
  for (y = 0; y < height; y += 2)
    for (x = 0; x < width; x += 2)
      {
	dest[(x/2) + (y/2) * width] =
	  (source[(x) + (y) * width] +
	   source[(x + 1) + (y) * width] +
	   source[(x) + (y + 1) * width] +
	   source[(x + 1) + (y + 1) * width]) >> 2;
      }

  for (y = 0; y < uv_height; y+=2)
    for (x = 0; x < uv_width; x+=2)
      {
	/* U */
	dest[(x/2) + (y/2) * uv_width + u_offset] =
	    ( source[(x) + (y) * uv_width + u_offset] +
	      source[(x+1) + (y) * uv_width + u_offset] +
	      source[(x) + (y+1) * uv_width + u_offset] +
	      source[(x+1) + (y+1) * uv_width + u_offset] ) >>2;

	/* V */
	dest[(x/2) + (y/2) * uv_width + v_offset] =
	    ( source[(x) + (y) * uv_width + v_offset]+
	      source[(x+1) + (y) * uv_width + v_offset]+
	      source[(x) + (y+1) * uv_width + v_offset]+
	      source[(x+1) + (y+1) * uv_width + v_offset] )>>2;
      }
}
#if 0
subsample_image4 (uint8_t* dest, uint8_t* source)
{
  int x, y;

  /* subsample Y */
  for (y = 0; y < height; y += 4)
    for (x = 0; x < width; x += 4)
      {
	dest[(x/4) + (y/4) * width] =
	  (source[(x + 0) + (y + 0) * width] +
	   source[(x + 1) + (y + 0) * width] +
	   source[(x + 2) + (y + 0) * width] +
	   source[(x + 3) + (y + 0) * width] +
	   source[(x + 0) + (y + 1) * width] +
	   source[(x + 1) + (y + 1) * width] +
	   source[(x + 2) + (y + 1) * width] +
	   source[(x + 3) + (y + 1) * width] +
	   source[(x + 0) + (y + 2) * width] +
	   source[(x + 1) + (y + 2) * width] +
	   source[(x + 2) + (y + 2) * width] +
	   source[(x + 3) + (y + 2) * width] +
	   source[(x + 0) + (y + 3) * width] +
	   source[(x + 1) + (y + 3) * width] +
	   source[(x + 2) + (y + 3) * width] +
	   source[(x + 3) + (y + 3) * width] )>>4;
      }

  for (y = 0; y < uv_height; y+=4)
    for (x = 0; x < uv_width; x+=4)
      {
	/* U */
	dest[(x/4) + (y/4) * uv_width + u_offset] =
	  (source[(x + 0) + (y + 0) * uv_width + u_offset] +
	   source[(x + 1) + (y + 0) * uv_width + u_offset] +
	   source[(x + 2) + (y + 0) * uv_width + u_offset] +
	   source[(x + 3) + (y + 0) * uv_width + u_offset] +
	   source[(x + 0) + (y + 1) * uv_width + u_offset] +
	   source[(x + 1) + (y + 1) * uv_width + u_offset] +
	   source[(x + 2) + (y + 1) * uv_width + u_offset] +
	   source[(x + 3) + (y + 1) * uv_width + u_offset] +
	   source[(x + 0) + (y + 2) * uv_width + u_offset] +
	   source[(x + 1) + (y + 2) * uv_width + u_offset] +
	   source[(x + 2) + (y + 2) * uv_width + u_offset] +
	   source[(x + 3) + (y + 2) * uv_width + u_offset] +
	   source[(x + 0) + (y + 3) * uv_width + u_offset] +
	   source[(x + 1) + (y + 3) * uv_width + u_offset] +
	   source[(x + 2) + (y + 3) * uv_width + u_offset] +
	   source[(x + 3) + (y + 3) * uv_width + u_offset] )>>4;

	/* V */
	dest[(x/4) + (y/4) * uv_width + v_offset] =
	  (source[(x + 0) + (y + 0) * uv_width + v_offset] +
	   source[(x + 1) + (y + 0) * uv_width + v_offset] +
	   source[(x + 2) + (y + 0) * uv_width + v_offset] +
	   source[(x + 3) + (y + 0) * uv_width + v_offset] +
	   source[(x + 0) + (y + 1) * uv_width + v_offset] +
	   source[(x + 1) + (y + 1) * uv_width + v_offset] +
	   source[(x + 2) + (y + 1) * uv_width + v_offset] +
	   source[(x + 3) + (y + 1) * uv_width + v_offset] +
	   source[(x + 0) + (y + 2) * uv_width + v_offset] +
	   source[(x + 1) + (y + 2) * uv_width + v_offset] +
	   source[(x + 2) + (y + 2) * uv_width + v_offset] +
	   source[(x + 3) + (y + 2) * uv_width + v_offset] +
	   source[(x + 0) + (y + 3) * uv_width + v_offset] +
	   source[(x + 1) + (y + 3) * uv_width + v_offset] +
	   source[(x + 2) + (y + 3) * uv_width + v_offset] +
	   source[(x + 3) + (y + 3) * uv_width + v_offset] )>>4;

      }
}
#endif

/* preparing to use MMX ;-) */
uint32_t 
calc_SAD(char* frm, char* ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
    int dx,dy,Y;
    uint32_t d=0;

    for (dy = 0; dy < BLOCKSIZE/div; dy++)
	for (dx = 0; dx < BLOCKSIZE/div; dx++)
	{
	    Y =
		*( frm + frm_offs + (dx) + (dy) * width ) -
		*( ref + ref_offs + (dx) + (dy) * width ) ;

	    d += (Y > 0) ? Y : -Y;
	}
    return d;
}
uint32_t 
calc_SAD_uv(uint8_t *frm, uint8_t *ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
    int dx=0,dy=0,Y;
    uint32_t d=0;
    static uint32_t old_frm_offs;
    static uint32_t old_d;

    if( old_frm_offs != frm_offs )
    {
	for (dy = 0; dy < (BLOCKSIZE/2)/div; dy++)
	    for (dx = 0; dx < (BLOCKSIZE/2)/div; dx++)
	    {
		Y =
		    *( frm + frm_offs + (dx) + (dy) * uv_width ) -
		    *( ref + ref_offs + (dx) + (dy) * uv_width ) ;
		
		d += (Y > 0) ? Y : -Y;
	    }
	old_d=d;
	return d;
    }
    else
    {
	return old_d;
    }
}
uint32_t 
calc_SQD(char* frm, char* ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
    int dx,dy,Y;
    uint32_t d=0;

    for (dy = 0; dy < BLOCKSIZE/div; dy++)
	for (dx = 0; dx < BLOCKSIZE/div; dx++)
	{
	    Y =
		*( frm + frm_offs + (dx) + (dy) * width ) -
		*( ref + ref_offs + (dx) + (dy) * width ) ;

	    d += Y*Y;
	}
    return d;
}
uint32_t 
calc_SQD_uv(uint8_t *frm, uint8_t *ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
    int dx=0,dy=0,Y;
    uint32_t d=0;
    static uint32_t old_frm_offs;
    static uint32_t old_d;

    if( old_frm_offs != frm_offs )
    {
	for (dy = 0; dy < (BLOCKSIZE/2)/div; dy++)
	    for (dx = 0; dx < (BLOCKSIZE/2)/div; dx++)
	    {
		Y =
		    *( frm + frm_offs + (dx) + (dy) * uv_width ) -
		    *( ref + ref_offs + (dx) + (dy) * uv_width ) ;
		
		d += Y*Y;
	    }
	old_d=d;
	return d;
    }
    else
    {
	return old_d;
    }
}

void
mb_search_44 (int x, int y)
{
  int dy, dx, qy, qx;
  uint32_t d;
  int Y, U, V;

  d  = calc_SAD( SUBA4,
		 SUBO4,
		 (x)/4+(y)/4*width,
		 (x)/4+(y)/4*width,
		 4 );

  d += calc_SAD_uv(SUBA4+u_offset,
		   SUBO4+u_offset,
		   (x)/8+(y)/8*uv_width,
		   (x)/8+(y)/8*uv_width,
		   4 );
  d += calc_SAD_uv(SUBA4+v_offset,
		   SUBO4+v_offset,
		   (x)/8+(y)/8*uv_width,
		   (x)/8+(y)/8*uv_width,
		   4 );

  CSAD=d;

  SAD[0] = 10000000;
  SAD[1] = 10000000;
  SAD[2] = 10000000;
  SAD[3] = 10000000;
  best_match_x[0]=
      best_match_y[0]=
      best_match_x[1]=
      best_match_y[1]=
      best_match_x[2]=
      best_match_y[2]=
      best_match_x[3]=
      best_match_y[3]=0;

  for (qy = -(search_radius >> 1); qy <= +(search_radius >> 1); qy+=4)
    for (qx = -(search_radius >> 1); qx <= +(search_radius >> 1); qx+=4)
      {
	  d  = calc_SAD( SUBA4,
			 SUBO4,
			 (x+qx-BLOCKOFFSET)/4+(y+qy-BLOCKOFFSET)/4*width,
			 (x   -BLOCKOFFSET)/4+(y   -BLOCKOFFSET)/4*width,
			 4 );

	  d += calc_SAD_uv(SUBA4+u_offset,
			   SUBO4+u_offset,
			   (x+qx-BLOCKOFFSET)/8+(y+qy-BLOCKOFFSET)/8*uv_width,
			   (x   -BLOCKOFFSET)/8+(y   -BLOCKOFFSET)/8*uv_width,
			   4 );
	  d += calc_SAD_uv(SUBA4+v_offset,
			   SUBO4+v_offset,
			   (x+qx-BLOCKOFFSET)/8+(y+qy-BLOCKOFFSET)/8*uv_width,
			   (x   -BLOCKOFFSET)/8+(y   -BLOCKOFFSET)/8*uv_width,
			   4 );

	  d += (qx>0)? qx/2:-qx/2;
	  d += (qy>0)? qy/2:-qy/2;

	  if (d < CSAD)
	  {
	      if (d < SAD[0])
	      {
		  best_match_x[1] = best_match_x[0];
		  best_match_x[2] = best_match_x[1];
		  best_match_x[3] = best_match_x[2];
		  SAD[1]=SAD[0];
		  SAD[2]=SAD[1];
		  SAD[3]=SAD[2];

		  best_match_x[0] = qx * 2;
		  best_match_y[0] = qy * 2;
		  SAD[0] = d;
	      }
	      else
		  if (d < SAD[1])
		  {
		      best_match_x[2] = best_match_x[1];
		      best_match_x[3] = best_match_x[2];
		      SAD[2]=SAD[1];
		      SAD[3]=SAD[2];

		      best_match_x[1] = qx * 2;
		      best_match_y[1] = qy * 2;
		      SAD[1] = d;
		  }
		  else
		      if (d < SAD[2])
		      {
			  best_match_x[3] = best_match_x[2];
			  SAD[3]=SAD[2];

			  best_match_x[2] = qx * 2;
			  best_match_y[2] = qy * 2;
			  SAD[2] = d;
		      }
		      else
			  if (d < SAD[3])
			  {
			      best_match_x[3] = qx * 2;
			      best_match_y[3] = qy * 2;
			      SAD[3] = d;
			  }
	  }
      }
}

void
mb_search_22 (int x, int y)
{
  int dy, dx, qy, qx;
  uint32_t d;
  int Y, U, V;
  int bx;
  int by;
  int i;

  SAD[0] = 10000000;
  SAD[1] = 10000000;
  SAD[2] = 10000000;
  SAD[3] = 10000000;

  for( i=0; i<4; i++)
  {
      bx=best_match_x[i]/2;
      by=best_match_y[i]/2;

      for (qy = (by - 8); qy <= (by + 8); qy+=2)
	  for (qx = (bx - 8); qx <= (bx + 8); qx+=2)
	  {
	      d  = calc_SAD( SUBA2,
			     SUBO2,
			     (x+qx-BLOCKOFFSET)/2+(y+qy-BLOCKOFFSET)/2*width,
			     (x   -BLOCKOFFSET)/2+(y   -BLOCKOFFSET)/2*width,
			     2 );
	      d += calc_SAD_uv(SUBA2+u_offset,
			       SUBO2+u_offset,
			       (x+qx-BLOCKOFFSET)/4+(y+qy-BLOCKOFFSET)/4*uv_width,
			       (x   -BLOCKOFFSET)/4+(y   -BLOCKOFFSET)/4*uv_width,
			       2 );
	      d += calc_SAD_uv(SUBA2+v_offset,
			       SUBO2+v_offset,
			       (x+qx-BLOCKOFFSET)/4+(y+qy-BLOCKOFFSET)/4*uv_width,
			       (x   -BLOCKOFFSET)/4+(y   -BLOCKOFFSET)/4*uv_width,
			       2 );

	      if ( qx==qy==0 )
	      {
		  CSAD=d;
	      }

	      if (d < SAD[i])
	      {
		  best_match_x[i] = qx * 2;
		  best_match_y[i] = qy * 2;
		  SAD[i] = d;
	      }
	  }
      if(CSAD<=SAD[i])
      {
	  best_match_x[i] = bx * 2;
	  best_match_y[i] = by * 2;
	  SAD[i] = CSAD;
      }
  }
}

void
mb_search (int x, int y)
{
  int dy, dx, qy, qx;
  uint32_t d, du, dv;
  int Y, U, V;
  int bx;
  int by;
  int i;

  SAD[0] = 10000000;
  SAD[1] = 10000000;
  SAD[2] = 10000000;
  SAD[3] = 10000000;

  for (i=0; i<4; i++)
  {
      bx=best_match_x[i]/2;
      by=best_match_y[i]/2;

      for (qy = (by - 4); qy <= (by + 4); qy++)
	  for (qx = (bx - 4); qx <= (bx + 4); qx++)
	  {
	      d  = calc_SAD( AVRG,
			     YUV1,
			     (x+qx-BLOCKOFFSET)+(y+qy-BLOCKOFFSET)*width,
			     (x   -BLOCKOFFSET)+(y   -BLOCKOFFSET)*width,
			     1 );

	      d += calc_SAD_uv(AVRG+u_offset,
			       YUV1+u_offset,
			       (x+qx-BLOCKOFFSET)/2+(y+qy-BLOCKOFFSET)/2*uv_width,
			       (x   -BLOCKOFFSET)/2+(y   -BLOCKOFFSET)/2*uv_width,
			       1 );

	      d += calc_SAD_uv(AVRG+v_offset,
			       YUV1+v_offset,
			       (x+qx-BLOCKOFFSET)/2+(y+qy-BLOCKOFFSET)/2*uv_width,
			       (x   -BLOCKOFFSET)/2+(y   -BLOCKOFFSET)/2*uv_width,
			       1 );

	      if ( qx==qy==0 )
	      {
		  CSAD=d;
	      }
	      
	      if (d < SAD[i])
	      {
		  best_match_x[i] = qx * 2;
		  best_match_y[i] = qy * 2;
		  SAD[i] = d;
	      }
	  }
      if(CSAD<=SAD[i])
      {
	  best_match_x[i] = bx * 2;
	  best_match_y[i] = by * 2;
	  SAD[i] = CSAD;
      }
  }
}

void
mb_search_half (int x, int y)
{
  int dy, dx, qy, qx, xx, yy, x0, x1, y0, y1;
  uint32_t d;
  int Y, U, V;
  int bx;
  int by;
  float sx, sy;
  float a0, a1, a2, a3;
  uint32_t old_SQD;
  int i;

  SAD[0] = 10000000;
  SAD[1] = 10000000;
  SAD[2] = 10000000;
  SAD[3] = 10000000;

  for(i=0; i<4; i++)
  {
      bx=best_match_x[i];
      by=best_match_y[i];

      for (qy = (by - 1); qy <= (by + 1); qy++)
	  for (qx = (bx - 1); qx <= (bx + 1); qx++)
	  {
	      sx = (qx - (qx & ~1)) * 0.5;
	      sy = (qy - (qy & ~1)) * 0.5;
	      
	      a0 = (1 - sx) * (1 - sy);
	      a1 = (sx) * (1 - sy);
	      a2 = (1 - sx) * (sy);
	      a3 = (sx) * (sy);
	      
	      xx = x + qx / 2;	/* Keeps some calc. out of the MB-search-loop... */
	      yy = y + qy / 2;
	      
	      d = 0;
	      for (dy = -2; dy < 6; dy++)
		  for (dx = -2; dx < 6; dx++)
		  {
		      x0 = xx + dx;
		      x1 = x0 - 1;
		      y0 = (yy + dy)*width;
		      y1 = (yy + dy - 1)*width;
		      
		      /* we only need Y for half-pels... */
		      Y = (AVRG[(x0) + (y0)] * a0 +
			   AVRG[(x1) + (y0)] * a1 +
			   AVRG[(x0) + (y1)] * a2 +
			   AVRG[(x1) + (y1)] * a3) -
			  YUV1[(x + dx) + (y + dy) * width];

		      d += (Y<0)? -Y:Y ; /* doch mal einen SAD... nur testweise ...*/
		  }

	      if (d < SAD[i])
	      {
		  best_match_x[i] = qx;
		  best_match_y[i] = qy;
		  SAD[i] = d;
	      }
	  }
  }
}

void
copy_filtered_block (int x, int y)
{
  int dx, dy;
  int qx = best_match_x[0];
  int qy = best_match_y[0];
  int xx, yy, x2, y2;
  float sx, sy;
  float a0, a1, a2, a3;

  sx = (qx - (qx & ~1)) * 0.5;
  sy = (qy - (qy & ~1)) * 0.5;
  qx /= 2;
  qy /= 2;

  a0 = (1 - sx) * (1 - sy);
  a1 = (sx) * (1 - sy);
  a2 = (1 - sx) * (sy);
  a3 = (sx) * (sy);

  for (dy = 0; dy < (BLOCKSIZE/2); dy++)
    for (dx = 0; dx < (BLOCKSIZE/2); dx++)
      {
	xx = x + dx;
	yy = y + dy;
	x2 = xx >> 1;
	y2 = yy / 2 * uv_width;

	/* Y */
	if (sx != 0 || sy != 0)
	  YUV2[(xx) + (yy) * width] =
	    (AVRG[(xx + qx - 0) + (yy + qy - 0) * width] * a0 +
	     AVRG[(xx + qx - 1) + (yy + qy - 0) * width] * a1 +
	     AVRG[(xx + qx - 0) + (yy + qy - 1) * width] * a2 +
	     AVRG[(xx + qx - 1) + (yy + qy - 1) * width] * a3) * (1-block_quality) +
	    YUV1[(xx) + (yy) * width] * block_quality;

	else
	  YUV2[(xx) + (yy) * width] =
	      AVRG[(xx + qx) + (yy + qy) * width] * (1-block_quality) +
	      YUV1[(xx) + (yy) * width] * block_quality;

	/* U */
	YUV2[(x + dx) / 2 + (y + dy) / 2 * uv_width + u_offset] =
	    AVRG[(x + dx + qx) / 2 + (y + dy + qy) / 2 * uv_width + u_offset] * (1-block_quality) +
	    YUV1[(x2) + (y2) + u_offset] * block_quality;

	/* V */
	YUV2[(x2) + (y2) + v_offset] =
	    AVRG[(x + dx + qx) / 2 + (y + dy + qy) / 2 * uv_width + v_offset] * (1-block_quality) +
	    YUV1[(x2) + (y2) + v_offset] * block_quality;
      }
}

#if 0
void
copy_reference_block (int x, int y)
{
  int dx, dy;
  int xx, yy;

  for (dy = 0; dy < (BLOCKSIZE/2); dy++)
    for (dx = 0; dx < (BLOCKSIZE/2); dx++)
      {
	xx = x + dx;
	yy = (y + dy) * width;

	/* Y */
	YUV2[(xx) + (yy)] =
	    YUV1[(xx) + (yy)];

	/* U */
	xx = (x + dx) / 2 + u_offset;
	yy = (y + dy) / 2 * uv_width;

	YUV2[(xx) + (yy)] = 
	    YUV1[(xx) + (yy)];

	xx += u_offset >> 2;

	YUV2[(xx) + (yy)] =
	    YUV1[(xx) + (yy)];
      }
}
#endif
void
copy_reference_block (int x, int y)
{
  int dx, dy;
  int xx, yy;

  for (dy = 0; dy < (BLOCKSIZE/2); dy++)
  {
      /* Y */
      memcpy(YUV2+x+(y+dy)*width,
	     YUV1+x+(y+dy)*width,BLOCKSIZE/2);
      
      xx=x/2;
      yy=(y+dy)/2;

      /* U */
      memcpy(YUV2+u_offset+xx+yy*uv_width,
	     YUV1+u_offset+xx+yy*uv_width,BLOCKSIZE/4);

      /* V */
      memcpy(YUV2+v_offset+xx+yy*uv_width,
	     YUV1+v_offset+xx+yy*uv_width,BLOCKSIZE/4);
      }
}

void
detect_black_borders ()
{
  int x, y, z;
  static int tb = 0;

  {
    if (border != -1)
      tb = border;
    if (border == 0)
      tb = 0;

    if (tb != 0)
      for (y = 0; y < tb; y++)
	for (x = 0; x < width; x++)
	  {
	    YUV2[x + y * width] = 16;
	    YUV2[x / 2 + y / 2 * uv_width + u_offset] = 128;
	    YUV2[x / 2 + y / 2 * uv_width + v_offset] = 128;

	    z = height - y;
	    YUV2[x + z * width] = 16;
	    YUV2[x / 2 + z / 2 * uv_width + u_offset] = 128;
	    YUV2[x / 2 + z / 2 * uv_width + v_offset] = 128;
	  }
  }
}


void
antiflicker_reference ()
{
  int x, y;
  int value;

  if(sharpen!=0)
      for (y = 1; y < (height-1); y++)
	  for (x = 1; x < (width-1); x++)
	  {
	      value=
		  YUV1[x + y * width]+
		  (
		      YUV1[x + y * width]-
		      (
			  YUV1[(x-1) + (y-1) * width] +
			  YUV1[(x+0) + (y-1) * width] +
			  YUV1[(x+1) + (y-1) * width] +
			  YUV1[(x-1) + (y+0) * width] +
			  YUV1[(x+0) + (y+0) * width] +
			  YUV1[(x+1) + (y+0) * width] +
			  YUV1[(x-1) + (y+1) * width] +
			  YUV1[(x+0) + (y+1) * width] +
			  YUV1[(x+1) + (y+1) * width] 
			  )/9
		      )*sharpen;
	      value=(value>248)? 248:value;
	      value=(value<16 )?  16:value;

	      YUV1[x + y * width]=value;
	  }

}

int
block_change(int x, int y, uint32_t mean)
{
    int d;
    d = calc_SAD( SUBA4,
		  SUBO4,
		  (x-BLOCKOFFSET)/4+(y-BLOCKOFFSET)/4*width,
		  (x-BLOCKOFFSET)/4+(y-BLOCKOFFSET)/4*width,
		  1 );
    d += calc_SAD_uv( SUBA4+u_offset,
		      SUBO4+u_offset,
		      (x-BLOCKOFFSET)/8+(y-BLOCKOFFSET)/8*uv_width,
		      (x-BLOCKOFFSET)/8+(y-BLOCKOFFSET)/8*uv_width,
		      1 );
    d += calc_SAD_uv( SUBA4+v_offset,
		      SUBO4+v_offset,
		      (x-BLOCKOFFSET)/8+(y-BLOCKOFFSET)/8*uv_width,
		      (x-BLOCKOFFSET)/8+(y-BLOCKOFFSET)/8*uv_width,
		      1 );
    d = (d<128)? 0:1 ;
    return d;
}

void
delay_image ()
{
    memcpy(DEL0, YUV1, (size_t)width*height*1.5);
}


void
denoise_image ()
{
  int sy,sx;
  uint32_t div;
  int x, y;
  uint32_t min_SQD=0;
  uint32_t avg_SQD=0;
  uint32_t min_Y_SQD=0;
  uint32_t min_U_SQD=0;
  uint32_t min_V_SQD=0;
  uint32_t Smin=0;
  static uint32_t mean_SQD = -1;
  static uint32_t mean_Y_SQD = -1;
  static uint32_t mean_U_SQD = -1;
  static uint32_t mean_V_SQD = -1;
  int y_start, y_end;

/* ---------------- */
/* TEST BLOCK !!!!! */
/* ---------------- */
#if 0
      for(y=0;y<128;y++)
	  for(x=0;x<128;x++)
	  {
	      YUV1[(x+0)+(y+0)*width]=
	      AVRG[(x+0)+(y+0)*width]=
		  0;
	      YUV1[(x+0)/2+(y+0)/2*uv_width+u_offset]=
	      AVRG[(x+0)/2+(y+0)/2*uv_width+u_offset]=
		  0;
	      YUV1[(x+0)/2+(y+0)/2*uv_width+v_offset]=
	      AVRG[(x+0)/2+(y+0)/2*uv_width+v_offset]=
		  0;
	  }

      for(y=0;y<16;y++)
	  for(x=0;x<16;x++)
	  {
	      YUV1[(x+0)+(y+0)*width]=
	      AVRG[(x+5)+(y+8)*width]=
		  x*y*10;
	      YUV1[(x+0)/2+(y+0)/2*uv_width+u_offset]=
	      AVRG[(x+5)/2+(y+8)/2*uv_width+u_offset]=
		  x+y;
	      YUV1[(x+0)/2+(y+0)/2*uv_width+v_offset]=
	      AVRG[(x+5)/2+(y+8)/2*uv_width+v_offset]=
		  x+y;
	  }
#endif
/* ---------------- */
/* TEST BLOCK !!!!! */
/* ---------------- */

  if(framenr==0)
  {
      memcpy(AVRG, YUV1, (size_t)width*height*1.5);
      memcpy(DEL0, YUV1, (size_t)width*height*1.5);
      memcpy(YUV2, YUV1, (size_t)width*height*1.5);
  }
  else
  {
      /* subsample the reference image */
      subsample_image (SUBO2,YUV1);
      subsample_image (SUBO4,SUBO2);

      /* subsample the averaged image */
      subsample_image (SUBA2,AVRG);
      subsample_image (SUBA4,SUBA2);

      /* reset the minimum SQD-counter */
      min_SQD = 10000;
      avg_SQD = 0;
      div = 0;

      for (y = 0; y < height; y += (BLOCKSIZE/2))
	  for (x = 0; x < width; x += (BLOCKSIZE/2))
	  {
	      div++;

	      if( block_change(x+0,y+0,mean_SQD))
	      {
		  /* subsampled full-search, first subsampled 4x4 then subsampled 2x2 pixels */
		  mb_search_44 (x, y);
#ifdef DEBUG
		  fprintf(stderr,"mb_search_44:\n");
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[0],best_match_y[0],SAD[0]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[1],best_match_y[1],SAD[1]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[2],best_match_y[2],SAD[2]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[3],best_match_y[3],SAD[3]);
#endif
		  mb_search_22 (x, y);
#ifdef DEBUG
		  fprintf(stderr,"mb_search_22:\n");
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[0],best_match_y[0],SAD[0]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[1],best_match_y[1],SAD[1]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[2],best_match_y[2],SAD[2]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[3],best_match_y[3],SAD[3]);
#endif
		  /* full-pel search, 16x16 pixels, with range [+/-1 ; +/-1] */
		  mb_search (x, y);
#ifdef DEBUG
		  fprintf(stderr,"mb_search:\n");
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[0],best_match_y[0],SAD[0]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[1],best_match_y[1],SAD[1]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[2],best_match_y[2],SAD[2]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[3],best_match_y[3],SAD[3]);
#endif
		  /* half-pel search, 16x16 pixels, with range [+/-0.5 ; +/-0.5] */
/*	          mb_search_half (x, y); 
 */
		  if(SAD[1]<SAD[0] &&
		     SAD[1]<SAD[2] &&
		     SAD[1]<SAD[3] )
		  {
		      best_match_x[0]=best_match_x[1];
		      best_match_y[0]=best_match_y[1];
		  }
		  else
		      if(SAD[2]<SAD[0] &&
			 SAD[2]<SAD[1] &&
			 SAD[2]<SAD[3] )
		      {
			  best_match_x[0]=best_match_x[2];
			  best_match_y[0]=best_match_y[2];
		      }
		      else
			  if(SAD[3]<SAD[0] &&
			     SAD[3]<SAD[1] &&
			     SAD[3]<SAD[2] )
			  {
			      best_match_x[0]=best_match_x[3];
			      best_match_y[0]=best_match_y[3];
			  }
#ifdef DEBUG
		  fprintf(stderr,"sorted:\n");
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[0],best_match_y[0],SAD[0]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[1],best_match_y[1],SAD[1]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[2],best_match_y[2],SAD[2]);
		  fprintf(stderr,"bx: %3i  by: %3i SAD:%i\n",best_match_x[3],best_match_y[3],SAD[3]);
#endif
	      }
	      else
	      {
		  best_match_x[0]=0;
		  best_match_y[0]=0;
	      }

	      /* eleminate block_matches outside the image */
	      if( (best_match_x[0]+x+BLOCKSIZE/2)>width  || 
		  (best_match_y[0]+y+BLOCKSIZE/2)>height ||
		  (best_match_x[0]+x)<0                  ||
		  (best_match_y[0]+y)<0                  )
	      {
		  best_match_x[0]=best_match_y[0]=0;
	      }
	      /* calculate SQD for 4x4(!!!) Block */
	      SQD = calc_SAD( AVRG,
			      YUV1,
			      (x)+(y)*width,
			      (x)+(y)*width,
			      2 );
	      SQD += calc_SAD_uv( AVRG+u_offset,
	                          YUV1+u_offset,
				  (x+best_match_x[0]/2)/2+(y+best_match_y[0]/2)/2*uv_width,
				  (x)/2+(y)/2*uv_width,
				  2 );
	      SQD += calc_SAD_uv( AVRG+v_offset,
				  YUV1+v_offset,
				  (x+best_match_x[0]/2)/2+(y+best_match_y[0]/2)/2*uv_width,
				  (x)/2+(y)/2*uv_width,
				  2 );

	      avg_SQD+=SQD;

	      if ( SQD  <= (mean_SQD   * 2) )
	      {
		  if(SQD<mean_SQD)
		  {
		      block_quality=0.0;
		  }
		  else
		  {
		      block_quality=(SQD-mean_SQD)/(mean_SQD+1);
		  }
		  copy_filtered_block (x, y);
	      }
	      else
	      {
		  copy_reference_block (x, y);
	      }
	  }
  }

  fprintf (stderr, "---  mean_SQD   :%i -- %f \n", mean_SQD, (float)mean_SQD/(BLOCKSIZE*BLOCKSIZE/4)/3);

  avg_SQD /= div;
  mean_SQD   = (float)mean_SQD *   0.95   + (float)avg_SQD * 0.05;
  if((avg_SQD)<mean_SQD)
      mean_SQD=avg_SQD;

  /* calculate the time averaged image */
  time_average_images ();
  delay_image();
}



