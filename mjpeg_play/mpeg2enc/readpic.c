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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "global.h"

 

static int frames_read = 0;
static int last_frame = -1;


static void border_extend(frame,w1,h1,w2,h2)
unsigned char *frame;
int w1,h1,w2,h2;
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

int piperead(int fd, char *buf, int len)
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



static void read_gop()
{
   int n, v, h, i, s;

   unsigned char magic[7];

   s = frames_read % (2*READ_LOOK_AHEAD);

   for(n=s;n<s+READ_LOOK_AHEAD/2;n++)
   {
      if(piperead(input_fd,magic,6)!=6) goto EOF_MARK;
      if(strncmp(magic,"FRAME\n",6))
      {
         fprintf(stderr,"\n\nStart of new frame is not \"FRAME<NL>\"\n");
         fprintf(stderr,"Exiting!!!!\n");
         exit(1);
      }
      v = vertical_size;
      h = horizontal_size;
      for(i=0;i<v;i++)
         if(piperead(input_fd,frame_buffers[n][0]+i*width,h)!=h) goto EOF_MARK;

      border_extend(frame_buffers[n][0],h,v,width,height);

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

int readframe( int num_frame,
               unsigned char *frame[]
	       	)
{
   int n;

   if( frames_read == 0)
   {
      /* Read first + second look-ahead buffer loads */

      read_gop();
      read_gop();
   }


   if(num_frame < frames_read - 2*READ_LOOK_AHEAD)
   {
      fprintf(stderr,"readframe: internal error 1\n");
      exit(1);
   }

   if(num_frame >= frames_read)
   {
      fprintf(stderr,"readframe: internal error 2\n");
      exit(1);
   }

   if(last_frame>=0 && num_frame>last_frame)
   {
      fprintf(stderr,"readframe: internal error 3\n");
      exit(1);
   }

   n = num_frame % (2*READ_LOOK_AHEAD);

   frame[0] = frame_buffers[n][0];
   frame[1] = frame_buffers[n][1];
   frame[2] = frame_buffers[n][2];
   frame[3] = frame_buffers[n][3];


   /* Read next look ahead chunk if this was the last 
	  frame of current one */

   /* We read fairly frequently... */
   if(num_frame%(READ_LOOK_AHEAD/2) == (READ_LOOK_AHEAD/2-1)) 
   {
	   read_gop();
   }


   return 0;
}
