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
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "global.h"

static char roundadj[4] = { 0, 0, 1, 2 };
 
unsigned char *filter_buf;
int noise_filt;
int drop_lsb;

static int frames_read = 0;
static int last_frame = -1;
static unsigned char ***frame_buffers;

static int cwidth, cheight;

static void do_drop_lsb(unsigned char *frame[])
{
   int *p, *end, c;
   int mask;
   int round;

   for( c = 0; c < sizeof(int); ++c )
   {
      ((char *)&mask)[c] = (0xff << drop_lsb) & 0xff;
      ((char *)&round)[c] = roundadj[drop_lsb];
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

static void do_noise_filt(unsigned char *frame[])
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
		   *bp = (f + 8) >> (3 + 1);
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

   unsigned char magic[6];

   s = frames_read % (2*N);

   for(n=s;n<s+N;n++)
   {
      if(piperead(0,magic,6)!=6) goto EOF_MARK;
      if(strncmp(magic,"FRAME\n",6))
      {
         fprintf(stderr,"\n\nStart of new frame is not \"FRAME<NL>\"\n");
         fprintf(stderr,"Exiting!!!!\n");
         exit(1);
      }
      v = vertical_size;
      h = horizontal_size;
      for(i=0;i<v;i++)
         if(piperead(0,frame_buffers[n][0]+i*width,h)!=h) goto EOF_MARK;

      border_extend(frame_buffers[n][0],h,v,width,height);

      v = chroma_format==CHROMA420 ? vertical_size/2 : vertical_size;
      h = chroma_format!=CHROMA444 ? horizontal_size/2 : horizontal_size;
      for(i=0;i<v;i++)
         if(piperead(0,frame_buffers[n][1]+i*cwidth,h)!=h) goto EOF_MARK;
      for(i=0;i<v;i++)
         if(piperead(0,frame_buffers[n][2]+i*cwidth,h)!=h) goto EOF_MARK;

      border_extend(frame_buffers[n][1],h,v,cwidth,cheight);
      border_extend(frame_buffers[n][2],h,v,cwidth,cheight);

      if( drop_lsb ) do_drop_lsb(frame_buffers[n]);
      if( noise_filt ) do_noise_filt(frame_buffers[n]);

      frames_read++;
   }
   return;

   EOF_MARK:
   last_frame = frames_read-1;
   nframes = frames_read;
}

int readframe(fname,frame)
char *fname;
unsigned char *frame[];
{
   int n, num_frame;

   if(frames_read == 0)
   {
      /* some initializations */

      /* Calculate width and height of chroma components
         (at the moment only CHROMA420 is used) */

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

      /* Allocate frame buffers for a complete GOP */

      frame_buffers = (unsigned char ***) malloc(2*N*sizeof(unsigned char**));
      if(!frame_buffers) { fprintf(stderr,"malloc failed\n"); exit(1); }

      for(n=0;n<2*N;n++)
      {
         frame_buffers[n] = (unsigned char **) malloc(3*sizeof(unsigned char*));
         if(!frame_buffers[n]) { fprintf(stderr,"malloc failed\n"); exit(1); }
         frame_buffers[n][0] = (unsigned char *) malloc(width*height);
         frame_buffers[n][1] = (unsigned char *) malloc(cwidth*cheight);
         frame_buffers[n][2] = (unsigned char *) malloc(cwidth*cheight);
         if(!frame_buffers[n][0] || !frame_buffers[n][1] || !frame_buffers[n][2])
         { fprintf(stderr,"malloc failed\n"); exit(1); }
      }

      /* Read first + second GOP */

      read_gop();
      read_gop();
   }

   num_frame = atoi(fname);

   if(num_frame < frames_read - 2*N)
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

   n = num_frame % (2*N);

   memcpy(frame[0],frame_buffers[n][0],width*height);
   memcpy(frame[1],frame_buffers[n][1],cwidth*cheight);
   memcpy(frame[2],frame_buffers[n][2],cwidth*cheight);

   /* Read next GOP if this was the last frame of current one */

   if(num_frame%N == N-1) read_gop();

   return 0;
}
