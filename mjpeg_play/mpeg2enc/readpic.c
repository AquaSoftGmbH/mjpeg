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
/* Modifications and enhancements (C) 2000/2001 Andrew Stevens */

/* These modifications are free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

/* Modifications and enhancements 
   (C) 2000/2001 Andrew Stevens, Rainer Johanni

 */

/* These modifications are free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */


#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "global.h"

 

static int frames_read = 0;
static int last_frame = -1;


/* Buffers for frame luminance means */

static int lum_mean[FRAME_BUFFER_SIZE];


static int luminance_mean(uint8_t *frame, int w, int h )
{
	uint8_t *p = frame;
	uint8_t *lim = frame + w*h;
	int sum = 0;
	while( p < lim )
	{
		sum += (p[0] + p[1]) + (p[2] + p[3]) + (p[4] + p[5]) + (p[6] + p[7]);
		p += 8;
	}
	return sum / (w * h);
}

static void border_extend(unsigned char *frame,
						  int w1, int h1, int w2, int h2)
{
  int i, j;
  unsigned char *fp;
 
  /* horizontal pixel replication (right border) */
 
  for (j=0; j<h1; j++)
  {
    fp = frame + j*w2;
    for (i=w1; i<w2; i++)
      fp[i] = fp[i-1];
  }
 
  /* vertical pixel replication (bottom border) */
 
  for (j=h1; j<h2; j++)
  {
    fp = frame + j*w2;
    for (i=0; i<w2; i++)
      fp[i] = fp[i-w2];
  }
}

static int piperead(int fd, char *buf, int len)
{
   int n, r;

   r = 0;

   while(r<len)
   {
      n = read(fd,buf+r,len-r);
      if(n==0) return r;
      r += n;
   }
   return r;
}



static void read_chunk()
{
   int n, v, h, i,j;

   unsigned char magic[7];

   for(j=0;j<READ_CHUNK_SIZE;++j)
   {
	   n = frames_read % FRAME_BUFFER_SIZE;
      if(piperead(input_fd,magic,6)!=6) goto EOF_MARK;
      if(strncmp(magic,"FRAME\n",6))
      {
		  magic[6]='\0';
         fprintf(stderr,"\n\nStart of frame %d is not \"FRAME<NL>\"\n",n);
         fprintf(stderr,"Exiting!!!!\n");
         exit(1);
      }
      v = vertical_size;
      h = horizontal_size;
      for(i=0;i<v;i++)
         if(piperead(input_fd,frame_buffers[n][0]+i*width,h)!=h) goto EOF_MARK;

      border_extend(frame_buffers[n][0],h,v,width,height);
	  
	  lum_mean[n] = luminance_mean(frame_buffers[n][0], width, height );

      v = chroma_format==CHROMA420 ? vertical_size/2 : vertical_size;
      h = chroma_format!=CHROMA444 ? horizontal_size/2 : horizontal_size;
      for(i=0;i<v;i++)
         if(piperead(input_fd,frame_buffers[n][1]+i*chrom_width,h)!=h) goto EOF_MARK;
      for(i=0;i<v;i++)
         if(piperead(input_fd,frame_buffers[n][2]+i*chrom_width,h)!=h) goto EOF_MARK;

      border_extend(frame_buffers[n][1],h,v,chrom_width,chrom_height);
      border_extend(frame_buffers[n][2],h,v,chrom_width,chrom_height);


      frames_read++;
   }
   return;

   EOF_MARK:
   last_frame = frames_read-1;
   nframes = frames_read;
}

static void load_frame( int num_frame, int look_ahead )
{

   if( frames_read == 0)
   {
      /* Read first + second look-ahead buffer loads */
	   while( frames_read < look_ahead )
		   read_chunk();
   }

   if(last_frame>=0 && num_frame>last_frame &&num_frame<nframes)
   {
      fprintf(stderr,"readframe: internal error reading beyond end of frames\n");
      exit(1);
   }

   /* Read in chunk(s) of frames if we have insufficient look-ahead margin
	*/

   while(frames_read - num_frame <= look_ahead && frames_read < nframes ) 
   {
	   read_chunk();
   }

   /* We aren't allowed to go too far behind the last read
	  either... */

   if(num_frame < frames_read - FRAME_BUFFER_SIZE)
   {
      fprintf(stderr,"readframe: internal error - buffer flusshed too soon\n");
      exit(1);
   }

	
}

int readframe( int num_frame,
               unsigned char *frame[]
	       	)
{
   int n;

   load_frame( num_frame, 1 ); 
   n = num_frame % FRAME_BUFFER_SIZE;

   frame[0] = frame_buffers[n][0];
   frame[1] = frame_buffers[n][1];
   frame[2] = frame_buffers[n][2];

   return 0;
}

int frame_lum_mean( int num_frame )
{
	int n = num_frame;
	if(  frames_read > 0 && num_frame > frames_read-1 )
	{
		n = frames_read-1;
	}
	load_frame( n, 1 );
	return lum_mean[n% FRAME_BUFFER_SIZE];
}


void read_stream_params( int *hsize, int *vsize, int *frame_rate_code )
{
	const int PARAM_LINE_MAX=256;
	int n;
	char param_line[PARAM_LINE_MAX];
	for(n=0;n<PARAM_LINE_MAX;n++)
	{
		if(!read(input_fd,param_line+n,1))
		{
			fprintf(stderr,"Error reading header from input stream\n");
			exit(1);
		}
		if(param_line[n]=='\n') break;
	}
	if(n==PARAM_LINE_MAX)
	{
		fprintf(stderr,"Didn't get linefeed in first %d characters of input stream\n",
				PARAM_LINE_MAX);
		exit(1);
	}
	param_line[n] = 0; /* Replace linefeed by end of string */

	if(strncmp(param_line,"YUV4MPEG",8))
	{
		fprintf(stderr,"Input starts not with \"YUV4MPEG\"\n");
		fprintf(stderr,"This is not a valid input for me\n");
		exit(1);
	}


	if( sscanf(param_line+8,"%d %d %d",hsize,vsize,frame_rate_code) != 3)
	{
		fprintf(stderr,"Could not get image dimenstion and frame rate from input stream\n");
		exit(1);
	}
}
