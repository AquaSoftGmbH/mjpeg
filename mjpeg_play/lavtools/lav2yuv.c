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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <lav_io.h>
#include <editlist.h>


int decode_jpeg_raw(unsigned char *jpeg_data, int len,
                    int itype, int ctype, int width, int height,
                    unsigned char *raw0, unsigned char *raw1,
                    unsigned char *raw2);
EditList el;

#define MAX_JPEG_LEN (512*1024)

int out_fd;
static unsigned char jpeg_data[MAX_JPEG_LEN];

int	active_x = 0;
int	active_y = 0;
int	active_width = 0;
int	active_height = 0;

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
static int param_mono = 0;
int output_width, output_height;

int	luma_size;
int	luma_offset;
int	luma_top_size;
int	luma_bottom_size;

int	luma_left_size;
int	luma_right_size;
int	luma_right_offset;


int	chroma_size;
int	chroma_offset;
int	chroma_top_size;
int	chroma_bottom_size;

int	chroma_output_width;
int	chroma_output_height;
int	chroma_height;
int	chroma_width;

int	chroma_left_size;
int	chroma_right_size;
int	chroma_right_offset;

unsigned char *frame_buf[3]; /* YUV... */
unsigned char *read_buf[3];
unsigned char *median_buf[3];

unsigned char luma_blank[720 * 480];
unsigned char chroma_blank[720 * 480];

int	verbose = 2;

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
  printf("   -a widthxhight+x+y\n");
  printf("   -s num     Special output format option:\n");
  printf("                 0 output like input, nothing special\n");
  printf("                 1 create half height/width output from interlaced input\n");
  printf("                 2 create 480 wide output from 720 wide input (for SVCD)\n");
  printf("                 3 create 352 wide output from 720 wide input (for vcd)\n");
  printf("   -d num     Drop lsbs of samples [0..3] (default: 0)\n");
  printf("   -n num     Noise filter (low-pass) [0..2] (default: 0)\n");
  printf("   -m         Force mono-chrome\n");
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

	int	chrom_top;
	int	chrom_bottom;
	
	luma_offset = active_y * output_width;
	luma_size = output_width * active_height;
	luma_top_size = active_y * output_width;
	luma_bottom_size = (output_height - (active_y + active_height)) * output_width;

	luma_left_size = active_x;
	luma_right_offset = active_x + active_width;
	luma_right_size = output_width - luma_right_offset;

	chroma_width = (chroma_format!=CHROMA420) ? active_width : active_width/2;
	chroma_height = (chroma_format!=CHROMA420) ? active_height : active_height/2;

	chrom_top = (chroma_format!=CHROMA420) ? active_y : active_y/2;
	chrom_bottom =(chroma_format!=CHROMA420) ? output_height - (chroma_height + chrom_top) : output_height/2 - (chroma_height + chrom_top);

	chroma_size = (chroma_format!=CHROMA420) ? luma_size : luma_size/4;
	chroma_offset = (chroma_format!=CHROMA420) ? luma_offset : luma_offset/4;
	chroma_top_size = (chroma_format!=CHROMA420) ? luma_top_size : luma_top_size/4;
	chroma_bottom_size = (chroma_format!=CHROMA420) ? luma_bottom_size : luma_bottom_size/4;

	chroma_output_width = (chroma_format!=CHROMA420) ? output_width : output_width/2;
	chroma_output_height = (chroma_format!=CHROMA420) ? output_height : output_height/2;
	chroma_left_size = (chroma_format!=CHROMA420) ? active_x : active_x/2;
	chroma_right_offset = chroma_left_size + chroma_width;
	chroma_right_size = chroma_output_width - chroma_right_offset;

	memset(luma_blank, 0, sizeof(luma_blank));
	memset(chroma_blank, 0x80, sizeof(chroma_blank));

  for (i=0; i<3; i++)
  {
	if (i==0)
		size = (output_width*output_height);
	else
		size = chroma_output_width*chroma_output_height;

    read_buf[i] = bufalloc(size);
    median_buf[i] = bufalloc(size);
  }

  filter_buf =  bufalloc( output_width*output_height );
}


int readframe(int numframe, unsigned char *frame[])
{
   int len, i, res;

   frame[0] = read_buf[0];
   frame[1] = read_buf[1];
   frame[2] = read_buf[2];

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
			   end = (int *)(frame[c] + chroma_output_width *chroma_output_height);
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

   if( param_mono )
   {
	   for( i =0; i < chroma_output_width *chroma_output_height; ++i )
	   {
		   frame[1][i] = frame[2][i] = 0x80;
	   }
   }
   return 0;

}

int median_cmp(const void *p1, const void *p2)
{
	return *(unsigned char *)p1 - *(unsigned char *)p2;
}


void median_filter_area(unsigned char *input, unsigned char *output, 
			int width, int height, int span)
{
	unsigned char	list[9];
	unsigned char	*ptr;
	unsigned char	*ptr1;
	unsigned char	*ptr2;
	unsigned char	*ptr3;
	int		y;
	int		x;

	for(y=1; y < height -1; y++) {
		ptr1 = &input[(y-1) * span];
		ptr2 = ptr1 + span;
		ptr3 = ptr2 + span;
		ptr  = &output[y * span];

		for(x=1;  x < width-1; x++) {
			list[0] = ptr1[0];
			list[1] = ptr1[1];
			list[2] = ptr1++[2];
			list[3] = ptr2[0];
			list[4] = ptr2[1];
			list[5] = ptr2++[2];
			list[6] = ptr3[0];
			list[7] = ptr3[1];
			list[8] = ptr3++[2];
			qsort(list, 9, 1, median_cmp);
			*ptr++ = list[4];
		}
	}

}

void median_filter(void)
{
	median_filter_area(&frame_buf[0][luma_offset+luma_left_size], &median_buf[0][luma_offset+luma_left_size], 
		active_width, active_height, output_width);

	median_filter_area(&frame_buf[1][chroma_offset+chroma_left_size], 
		&median_buf[1][chroma_offset+chroma_left_size], 
		chroma_width, chroma_height, chroma_output_width);

	median_filter_area(&frame_buf[2][chroma_offset+chroma_left_size], 
		&median_buf[2][chroma_offset+chroma_left_size], 
		chroma_width, chroma_height, chroma_output_width);


	frame_buf[0] = median_buf[0];
	frame_buf[1] = median_buf[1];
	frame_buf[2] = median_buf[2];
}

/*
  Raw write does *not* guarantee to write the entire buffer load if it
  becomes possible to do so.  This does...
 */

static size_t do_write( int fd, void *buf, size_t count )
{
	char *cbuf = buf;
	size_t count_left = count;
	size_t written;
	while( count_left > 0 )
	{
		written = write( fd, cbuf, count_left );
		if( written < 0 )
		{
			perror( "Error writing: " );
			exit(1);
		}
		count_left -= written;
		cbuf += written;
	}
	return count;
}

void writeoutYUV4MPEGheader()
{
	 char str[256];

	 /* RJ: Framerate specification should be based on
		something more reliable than height */
	 
	 sprintf(str,"YUV4MPEG %d %d %d\n",
			 output_width,output_height,el.video_norm == 'n' ? 4 : 3);
	 do_write(out_fd,str,strlen(str));
}

void writeoutframeinYUV4MPEG(unsigned char *frame[])
{
  int	n=0;
  int	i;
  char	*ptr;

  do_write(out_fd,"FRAME\n",6);

	for(i=0; i < active_height; i++) {
		ptr = &frame[0][luma_offset + (i * output_width)];
		if (luma_left_size) {
			memset(ptr, 0x00, luma_left_size);
		}
		if (luma_right_size) {
			memset(&ptr[luma_right_offset], 0x00, luma_right_size);
		}
	}

	for(i=0; i < chroma_height; i++) {
		ptr = &frame[1][chroma_offset + (i * chroma_output_width)];
		if (chroma_left_size) {
			memset(ptr, 0x80, chroma_left_size);
		}

		if (chroma_right_size) {
			memset(&ptr[chroma_right_offset], 0x80, chroma_right_size);
		}

		ptr = &frame[2][chroma_offset + (i * chroma_output_width)];
		if (chroma_left_size) {
			memset(ptr, 0x80, chroma_left_size);
		}

		if (chroma_right_size) {
			memset(&ptr[chroma_right_offset], 0x80, chroma_right_size);
		}

	}

	if (luma_top_size) {
		n +=do_write(out_fd, luma_blank, luma_top_size);
	}
	n += do_write(out_fd, &frame[0][luma_offset], luma_size);
	if (luma_bottom_size) {
		n += do_write(out_fd, luma_blank, luma_bottom_size);
	}


	if (chroma_top_size) {
		n += do_write(out_fd, chroma_blank, chroma_top_size);
	}
	n += do_write(out_fd,&frame[1][chroma_offset], chroma_size);
	if (chroma_bottom_size) {
		n += do_write(out_fd, chroma_blank, chroma_bottom_size);
	}

	if (chroma_top_size) {
		n += do_write(out_fd, chroma_blank, chroma_top_size);
	}
	n += do_write(out_fd,&frame[2][chroma_offset], chroma_size);
	if (chroma_bottom_size) {
		n += do_write(out_fd, chroma_blank, chroma_bottom_size);
	}
}


void streamout()
{
	int framenum;
	writeoutYUV4MPEGheader();
	for( framenum = 0; framenum < el.video_frames ; ++framenum )
	{
		readframe(framenum,frame_buf);
#ifdef	NEVER
		median_filter();
#endif
		writeoutframeinYUV4MPEG(frame_buf);
	}
}

int main(argc,argv)
int argc;
char *argv[];
{
  int n, nerr = 0;
  char	*geom;
  char  *end;

  while( (n=getopt(argc,argv,"v:a:s:d:n:")) != EOF)
  {
    switch(n) {

      case 'a':
	geom = optarg;
	active_width = strtol(geom, &end, 10);
	if (*end != 'x' || active_width < 100) {
		fprintf(stderr, "Bad width parameter\n");
		nerr++;
		break;
	}

	geom = end + 1;
	active_height = strtol(geom, &end, 10);
	if ((*end != '+' && *end != '\0') || active_height < 100) {
		fprintf(stderr, "Bad height parameter\n");
		nerr++;
		break;
	}

	if (*end == '\0')
		break;

	geom = end + 1;
	active_x = strtol(geom, &end, 10);
	if ((*end != '+' && *end != '\0') || active_x > 720) {
		fprintf(stderr, "Bad x parameter\n");
		nerr++;
		break;
	}


	geom= end + 1;
	active_y = strtol(geom, &end, 10);
	if (*end != '\0' || active_y > 240) {
		fprintf(stderr, "Bad y parameter\n");
		nerr++;
		break;
	}
	break;
	

      case 's':
        param_special = atoi(optarg);
        if(param_special<0 || param_special>3)
        {
          fprintf(stderr,"-s option requires arg 0, 1, 2 or 3\n");
          nerr++;
        }
        break;

      case 'v':
	verbose = atoi(optarg);
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
	case 'm' :
		param_mono = 1;
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

  if(param_special==3)
  {
    if(el.video_width == 720)
    {
      output_width  = 352;
    }
    else
    {
      fprintf(stderr,"-s 3 may only be set for 720 pixel wide video sources\n");
      Usage(argv[0]);
    }
  }

  /* Round sizes to multiples of 16 ... */
  output_width = ((output_width + 15) / 16) * 16;
  output_height = ((output_height + 15) / 16) * 16;

  if (active_width == 0) {
	active_width = output_width;
  }

  if (active_height == 0) {
	active_height = output_height;
  }


  if (active_width + active_x > output_width) {
	fprintf(stderr, "active offset + acitve width > image size\n");
	Usage(argv[0]);
  }

  if (active_height + active_y > output_height) {
	fprintf(stderr, "active offset + active height > image size\n");
	Usage(argv[0]);
  }

  out_fd = 1; /* stdout */
  init();
  streamout();

  return 0;
}
