/* readpic.c, read source pictures                                          */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "global.h"

#include <lav_io.h>
#include <editlist.h>

extern EditList el;

#define MAX_JPEG_LEN (512*1024)

static unsigned char jpeg_data[MAX_JPEG_LEN];

static char roundadj[4] = { 0, 0, 1, 2 };
 
unsigned char *filter_buf;
int noise_filt;
int drop_lsb;

int readframe(fname,frame)
char *fname;
unsigned char *frame[];
{
   int numframe, len, res;

   if(MAX_JPEG_LEN < el.max_frame_size)
   {
      fprintf(stderr,"Max size of JPEG frame = %d: too big\n",el.max_frame_size);
      exit(1);
   }

   numframe = atoi(fname);

   len = el_get_video_frame(jpeg_data, numframe, &el);

   res = decode_jpeg_raw(jpeg_data,len,el.video_inter,
                         chroma_format,width,height,
                         frame[0],frame[1],frame[2]);
   if(res) 
	 { 
	   fprintf(stderr,"Warning: Decoding of Frame %d failed\n",numframe);
	   /* TODO: Selective exit here... */
	   return 1;
	 }
   
   
 
   if( drop_lsb )
	 {
	   int cwidth, cheight;
	   int *p, *end, c;
	   int mask;
	   int round;

	   for( c = 0; c < sizeof(int); ++c )
		 {
		   ((char *)&mask)[c] = (0xff << drop_lsb) & 0xff;
		   ((char *)&round)[c] = roundadj[drop_lsb];
		 }
	   
		 
	   switch ( chroma_format ) 
		 {
		 case CHROMA420 :
		   cwidth = width / 2;
		   cheight = height / 2; 
		   break;
		 case CHROMA422 :
		   cwidth = width / 2;
		   cheight = height;
		   break;
		 case CHROMA444 :
		   cwidth = width;
		   cheight = height;
		 default :
		   abort();
		 }

	   /* N.b. we know width is multiple of 16 so doing lsb dropping
		  int-wise will work for sane (32-bit or 64-bit) machines
	   */
	   
	   for( c = 0; c < 3 ; ++c )
		 {
		   p = (int *)frame[c];
		   if( c == 0 )
			 { 
			   end = (int *)(frame[c]+ width*height);
			 }
		   else
			 {
			   end = (int *)(frame[c] + cwidth*cheight);
			 }
		   while( p++ < end )
			 {
			   *p = (*p & mask) + round;
			 }
		 }
	 }

   if( noise_filt )
	 {
	   unsigned char *bp;
	   unsigned char *p = frame[0]+width+1;
	   unsigned char *end = frame[0]+width*(height-1);

	   bp = filter_buf + width+1;
	   if( noise_filt == 1 )
		 {
		   for( p = frame[0]+width+1; p < end; ++p )
			 {
			   register int f=(p[-width-1]+p[-width]+p[-width+1]+
								p[-1] + p[1] +
								p[width-1]+p[width]+p[width+1]);
			   /* f = f + (f<<1) + (*p << 3);
				*bp = (f + (1 << 4)) >> (3+2); */
			   f = f + (*p<<3);
			   *bp = (f + (1 << 2)) >> (3 + 1);
			   ++bp;
			 }
		 }
	   else
		 {
		   for( p = frame[0]+width+1; p < end; ++p )
			 {
			   register int f=(p[-width-1]+p[-width]+p[-width+1]+
								p[-1] + p[1] +
								p[width-1]+p[width]+p[width+1]);
			   /* f = f + (f<<1) + (*p << 3);
				*bp = (f + (1 << 4)) >> (3+2); */
			   f = f + (*p<<3);
			   *bp = (f + (1 << 2)) >> (3 + 1);
			   ++bp;
			 }
		 }
		  
	   bp = filter_buf + width+1;
	   for( p = frame[0]+width+1; p < end; ++p )
		 {
		   *p = *bp;
		   ++bp;
		 }
	   
	   
	 }
   return 0;

}
