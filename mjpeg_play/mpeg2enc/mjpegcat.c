/* readpic.c, read source pictures                                          */

/* Copyright (C) 2000, Rainer Johanni, Andrew Stevens */
 
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lav_io.h>
#include <editlist.h>

/* These should be in the headers!! */

int el_get_video_frame(char *vbuff, long nframe, EditList *el);  
void read_video_files(char **filename, int num_files, EditList *el); 

int decode_jpeg_raw(unsigned char *jpeg_data, int len,
                    int itype, int ctype, int width, int height,
                    unsigned char *raw0, unsigned char *raw1,
                    unsigned char *raw2);
EditList el;

#define MAX_JPEG_LEN (512*1024)

static unsigned char jpeg_data[MAX_JPEG_LEN];

static char roundadj[4] = { 0, 0, 1, 2 };


/* chroma_format */
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

#define BUFFER_ALIGN 16
 
unsigned char *filter_buf;
int noise_filt;
int drop_lsb;
int chroma_format = CHROMA420;

static int param_drop_lsb   = 0;
static int param_noise_filt = 0;
static int param_special = 0;

int output_width, output_height;
int chrom_width, chrom_height;

unsigned char *frame_buf[3]; /* YUV... */



EditList el;



void error(text)
char *text;
{
  fprintf(stderr,text);
  putc('\n',stderr);
  exit(1);
}



void Usage(char *str)
{
  printf("Usage: %s [params] inputfiles\n",str);
  printf("   where possible params are:\n");
  printf("   -s num     Special output format option:\n");
  printf("                 0 output like input, nothing special\n");
  printf("                 1 create half height/width output from interlaced input\n");
  printf("                 2 create 480 wide output from 720 wide input (for SVCD)\n");
  printf("   -d num     Drop lsbs of samples [0..3] (default: 0)\n");
  printf("   -n num     Noise filter (low-pass) [0..2] (default: 0)\n");
  exit(0);
}


/*
	Wrapper for malloc that allocates pbuffers aligned to the 
	specified byte boundary and checks for failure.
	N.b.  don't try to free the resulting pointers, eh...
	BUG: 	Of course this won't work if a char * won't fit in an int....
*/

static unsigned char *bufalloc( size_t size )
{
	char *buf = malloc( size + BUFFER_ALIGN );
	int adjust;
	if( buf == NULL )
	{
	   error("malloc failed\n");
	}
	adjust = BUFFER_ALIGN-((int)buf)%BUFFER_ALIGN;
	if( adjust == BUFFER_ALIGN )
		adjust = 0;
	return (unsigned char*)(buf+adjust);
}

static void init()
{
	int size,i;
	chrom_width = (chroma_format==CHROMA444) ? output_width : output_width/2;
	chrom_height = (chroma_format!=CHROMA420) ? output_height : output_height/2;


  for (i=0; i<3; i++)
  {
	if (i==0)
		size = (output_width*output_height);
	else
		size = chrom_width*chrom_height;

    frame_buf[i] = bufalloc(size);
  }

  filter_buf =  bufalloc( output_width*output_height );
}


int readframe(int numframe, unsigned char *frame[])
{
   int len, res;

   if(MAX_JPEG_LEN < el.max_frame_size)
   {
      fprintf(stderr,"Max size of JPEG frame = %ld: too big\n",el.max_frame_size);
      exit(1);
   }

   len = el_get_video_frame(jpeg_data, numframe, &el);

   res = decode_jpeg_raw(jpeg_data,len,el.video_inter,
                         chroma_format,output_width,output_height,
                         frame[0],frame[1],frame[2]);
   if(res) 
	 { 
	   fprintf(stderr,"Warning: Decoding of Frame %d failed\n",numframe);
	   /* TODO: Selective exit here... */
	   return 1;
	 }
   
   
 
   if( drop_lsb )
	 {
	   int *p, *end, c;
	   int mask;
	   int round;
	   end = 0; /* Silence compiler */
	   for( c = 0; c < sizeof(int); ++c )
		 {
		   ((char *)&mask)[c] = (0xff << drop_lsb) & 0xff;
		   ((char *)&round)[c] = roundadj[drop_lsb];
		 }
	   
		 
	   /* we know output_width is multiple of 16 so doing lsb dropping
		  int-wise will work for sane (32-bit or 64-bit) machines
	   */
	   
	   for( c = 0; c < 3 ; ++c )
		 {
		   p = (int *)frame[c];
		   if( c == 0 )
			 { 
			   end = (int *)(frame[c]+ output_width*output_height);
			 }
		   else
			 {
			   end = (int *)(frame[c] + chrom_width*chrom_height);
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
	   unsigned char *p = frame[0]+output_width+1;
	   unsigned char *end = frame[0]+output_width*(output_height-1);

	   bp = filter_buf + output_width+1;
	   if( noise_filt == 1 )
	   	 {
		   for( p = frame[0]+output_width+1; p < end; ++p )
			 {
			   register int f=(p[-output_width-1]+p[-output_width]+p[-output_width+1]+
								p[-1] + p[1] +
								p[output_width-1]+p[output_width]+p[output_width+1]);
			   f = f + ((*p)<<3) + ((*p)<<4);   /* 8 + (8 + 16) = 32 */
			   *bp = (f + 8) >> (4 + 1);
			   ++bp;
			 }
		 }
	   else if( noise_filt == 2 )
		 {
		   for( p = frame[0]+output_width+1; p < end; ++p )
			 {
			   register int f=(p[-output_width-1]+p[-output_width]+p[-output_width+1]+
								p[-1] + p[1] +
								p[output_width-1]+p[output_width]+p[output_width+1]);

			   f = f + ((*p)<<3);
			   *bp = (f + 16) >> (3 + 1);
			   ++bp;
			 }
		 }
	   else
		 {
		   for( p = frame[0]+output_width+1; p < end; ++p )
			 {
			   register int f=(p[-output_width-1]+p[-output_width]+p[-output_width+1]+
								p[-1] + p[1] +
								p[output_width-1]+p[output_width]+p[output_width+1]);
			   /* f = f + (f<<1) + (*p << 3);
				*bp = (f + (1 << 4)) >> (3+2); */
			   f = f + (*p<<3);
			   *bp = (f + (1 << 2)) >> (3 + 1);
			   ++bp;
			 }
		 }
		  
	   bp = filter_buf + output_width+1;
	   for( p = frame[0]+output_width+1; p < end; ++p )
		 {
		   *p = *bp;
		   ++bp;
		 }
	   
	   
	 }
   return 0;

}

void writeoutYUV4MPEGheader()
{
	 char str[256];

	 /* RJ: Framerate specification should be based on
		something more reliable than height */
	 
	 sprintf(str,"YUV4MPEG %d %d %d\n",
			 output_width,output_height,(output_height>512)?3:4);
	 write(1,str,strlen(str));
}

void writeoutframeinYUV4MPEG(unsigned char *frame[])
{
  int n=0;

  write(1,"FRAME\n",6);
  n += write(1,frame[0],output_width * output_height);
  n += write(1,frame[1],chrom_width * chrom_height);
  n += write(1,frame[2],chrom_width * chrom_height);
}

void streamout()
{
	int framenum;
	writeoutYUV4MPEGheader();
	for( framenum = 0; framenum < el.video_frames ; ++framenum )
	{
		readframe(framenum,frame_buf);
		writeoutframeinYUV4MPEG(frame_buf);
	}
}

int main(argc,argv)
int argc;
char *argv[];
{
  int n, nerr = 0;

  while( (n=getopt(argc,argv,"s:d:n:")) != EOF)
  {
    switch(n) {

      case 's':
        param_special = atoi(optarg);
        if(param_special<0 || param_special>2)
        {
          fprintf(stderr,"-s option requires arg 0, 1 or 2\n");
          nerr++;
        }
        break;

      case 'd':
        param_drop_lsb = atoi(optarg);
        if(param_drop_lsb<0 || param_drop_lsb>3)
        {
          fprintf(stderr,"-d option requires arg 0..3\n");
          nerr++;
        }
		break;
		
      case 'n':
        param_noise_filt = atoi(optarg);
        if(param_noise_filt<0 || param_noise_filt>2)
        {
          fprintf(stderr,"-n option requires arg 0..2\n");
          nerr++;
        }
		break;
	default:
	  nerr++;
    }
  }

  if(optind>=argc) nerr++;

  if(nerr) Usage(argv[0]);


  /* Open editlist */

  read_video_files(argv + optind, argc - optind, &el);

  output_width  = el.video_width;
  output_height = el.video_height;

  if(param_special==1)
  {
    if(el.video_inter)
    {
      output_width  = (el.video_width/2) &0xfffffff0;
      output_height = el.video_height/2;
    }
    else
    {
      fprintf(stderr,"-s 1 may only be set for interlaced video sources\n");
      Usage(argv[0]);
    }
  }

  if(param_special==2)
  {
    if(el.video_width==720)
    {
      output_width  = 480;
    }
    else
    {
      fprintf(stderr,"-s 2 may only be set for 720 pixel wide video sources\n");
      Usage(argv[0]);
    }
  }

  /* Round sizes to multiples of 16 ... */
  output_width = ((output_width + 15) / 16) * 16;
  output_height = ((output_height + 15) / 16) * 16;

  init();
  streamout();

  return 0;
}
