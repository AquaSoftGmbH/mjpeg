/* lav2yuv - stream any lav input file to stdout as YUV4MPEG data */

/* Copyright (C) 2000, Rainer Johanni, Andrew Stevens */
/* added scene change detection code 2001, pHilipp Zabel */

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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <lav_io.h>
#include <editlist.h>
#include <math.h>
#include "yuv4mpeg.h"

#include "mjpeg_logging.h"

int luminance_mean(unsigned char *frame, int w, int h);
int decode_jpeg_raw(unsigned char *jpeg_data, int len,
                    int itype, int ctype, int width, int height,
                    unsigned char *raw0, unsigned char *raw1,
                    unsigned char *raw2);

void error(char *text);
void Usage(char *str);
int readframe(int numframe, unsigned char *frame[]);
void writeoutYUV4MPEGheader(void);
void writeoutframeinYUV4MPEG(unsigned char *frame[]);
void streamout(void);


EditList el;

#define MAX_EDIT_LIST_FILES 256
#define MAX_JPEG_LEN (512*1024)

int out_fd;
static unsigned char jpeg_data[MAX_JPEG_LEN];

int active_x = 0;
int active_y = 0;
int active_width = 0;
int active_height = 0;

static char roundadj[4] = { 0, 0, 1, 2 };

static int param_offset = 0;
static int param_frames = 0;


#define BUFFER_ALIGN 16

unsigned char *filter_buf;
int drop_lsb;
int chroma_format = CHROMA420;

static int param_drop_lsb = 0;
static int param_noise_filt = 0;
static int param_special = 0;
static int param_mono = 0;
static char *param_scenefile = NULL;

static unsigned int delta_lum_threshold = 4;
static unsigned int scene_detection_decimation = 2;

int output_width, output_height;

int luma_size;
int luma_offset;
int luma_top_size;
int luma_bottom_size;

int luma_left_size;
int luma_right_size;
int luma_right_offset;


int chroma_size;
int chroma_offset;
int chroma_top_size;
int chroma_bottom_size;

int chroma_output_width;
int chroma_output_height;
int chroma_height;
int chroma_width;

int chroma_left_size;
int chroma_right_size;
int chroma_right_offset;

unsigned char *frame_buf[3];    /* YUV... */
unsigned char *read_buf[3];
unsigned char *double_buf[3];

unsigned char luma_blank[768 * 480];
unsigned char chroma_blank[768 * 480];

int verbose = 1;

EditList el;



void Usage(char *str)
{
   fprintf(stderr,"Usage: %s [params] inputfiles\n", str);
   fprintf(stderr,"   where possible params are:\n");
   fprintf(stderr,"   -a widthxhight+x+y\n");
   fprintf(stderr,"   -s num     Special output format option:\n");
   fprintf(stderr,"                 0 output like input, nothing special\n");
   fprintf(stderr,
		   "                 1 create half height/width output from interlaced input\n");
   fprintf(stderr,
		   "                 2 create 480 wide output from 720 wide input (for SVCD)\n");
   fprintf(stderr,
		   "                 3 create 352 wide output from 720 wide input (for vcd)\n");
   fprintf(stderr,
		   "                 4 create 480 x 480 output from 720 x 240 input (for svcd)\n");
   fprintf(stderr,"   -d num     Drop lsbs of samples [0..3] (default: 0)\n");
   fprintf(stderr,"   -n num     Noise filter (low-pass) [0..2] (default: 0)\n");
   fprintf(stderr,"   -m         Force mono-chrome\n");
   fprintf(stderr,"   -S list.el Output a scene list with scene detection\n");
   fprintf(stderr,"   -T num     Set scene detection threshold to num (default: 4)\n");
   fprintf(stderr,"   -D num     Width decimation to use for scene detection (default: 2)\n");
   fprintf(stderr,"   -o num     Frame offset - skip num frames in the beginning\n");
   fprintf(stderr,"              if num is negative, all but the last num frames are skipped\n");
   fprintf(stderr,"   -f num     Only num frames are written to stdout (0 means all frames)\n");
   exit(0);
}


int luminance_mean(unsigned char *frame, int w, int h )
{
	unsigned char *p = frame;
	unsigned char *lim = frame + w*h;
	int sum = 0;
	while( p < lim )
	{
		sum += (p[0] + p[1]) + (p[2] + p[3]) + (p[4] + p[5]) + (p[6] + p[7]);
		p += 8;
	}
	return sum / (w * h);
}


/*
	Wrapper for malloc that allocates pbuffers aligned to the 
	specified byte boundary and checks for failure.
	N.b.  don't try to free the resulting pointers, eh...
	BUG: 	Of course this won't work if a char * won't fit in an int....
*/

static unsigned char *bufalloc(size_t size)
{
   char *buf = malloc(size + BUFFER_ALIGN);
   int adjust;
   if (buf == NULL) {
      error("malloc failed\n");
   }
   adjust = BUFFER_ALIGN - ((int) buf) % BUFFER_ALIGN;
   if (adjust == BUFFER_ALIGN)
      adjust = 0;
   return (unsigned char *) (buf + adjust);
}

static void init()
{
   int size, i;

   int chrom_top;
   int chrom_bottom;

   luma_offset = active_y * output_width;
   luma_size = output_width * active_height;
   luma_top_size = active_y * output_width;
   luma_bottom_size =
       (output_height - (active_y + active_height)) * output_width;

   luma_left_size = active_x;
   luma_right_offset = active_x + active_width;
   luma_right_size = output_width - luma_right_offset;

   chroma_width =
       (chroma_format != CHROMA420) ? active_width : active_width / 2;
   chroma_height =
       (chroma_format != CHROMA420) ? active_height : active_height / 2;

   chrom_top = (chroma_format != CHROMA420) ? active_y : active_y / 2;
   chrom_bottom =
       (chroma_format !=
        CHROMA420) ? output_height - (chroma_height +
                                      chrom_top) : output_height / 2 -
       (chroma_height + chrom_top);

   chroma_size = (chroma_format != CHROMA420) ? luma_size : luma_size / 4;
   chroma_offset =
       (chroma_format != CHROMA420) ? luma_offset : luma_offset / 4;
   chroma_top_size =
       (chroma_format != CHROMA420) ? luma_top_size : luma_top_size / 4;
   chroma_bottom_size =
       (chroma_format !=
        CHROMA420) ? luma_bottom_size : luma_bottom_size / 4;

   chroma_output_width =
       (chroma_format != CHROMA420) ? output_width : output_width / 2;
   chroma_output_height =
       (chroma_format != CHROMA420) ? output_height : output_height / 2;
   chroma_left_size =
       (chroma_format != CHROMA420) ? active_x : active_x / 2;
   chroma_right_offset = chroma_left_size + chroma_width;
   chroma_right_size = chroma_output_width - chroma_right_offset;

   memset(luma_blank, 0, sizeof(luma_blank));
   memset(chroma_blank, 0x80, sizeof(chroma_blank));

   for (i = 0; i < 3; i++) {
      if (i == 0)
         size = (output_width * output_height);
      else
         size = chroma_output_width * chroma_output_height;

      read_buf[i] = bufalloc(size);
      double_buf[i] = bufalloc(size);
   }

   filter_buf = bufalloc(output_width * output_height);
}


int readframe(int numframe, unsigned char *frame[])
{
   int len, i, res;

   frame[0] = read_buf[0];
   frame[1] = read_buf[1];
   frame[2] = read_buf[2];

   if (MAX_JPEG_LEN < el.max_frame_size) {
      mjpeg_error_exit1( "Max size of JPEG frame = %ld: too big\n",
						 el.max_frame_size);
   }

   len = el_get_video_frame(jpeg_data, numframe, &el);

   if (param_special == 4) {
      res = decode_jpeg_raw(jpeg_data, len, el.video_inter,
                            chroma_format, output_width, output_height / 2,
                            frame[0], frame[1], frame[2]);
   } else {
      res = decode_jpeg_raw(jpeg_data, len, el.video_inter,
                            chroma_format, output_width, output_height,
                            frame[0], frame[1], frame[2]);
   }

   if (param_special == 4) {
      for (i = 0; i < output_height / 2; i++) {
         memcpy(&double_buf[0][i * output_width * 2],
                &read_buf[0][i * output_width], output_width);
         memcpy(&double_buf[0][i * output_width * 2 + output_width],
                &read_buf[0][i * output_width], output_width);
      }

      for (i = 0; i < chroma_height / 2; i++) {
         memcpy(&double_buf[1][i * chroma_width * 2],
                &read_buf[1][i * chroma_width], chroma_width);
         memcpy(&double_buf[1][i * chroma_width * 2 + chroma_width],
                &read_buf[1][i * chroma_width], chroma_width);

         memcpy(&double_buf[2][i * chroma_width * 2],
                &read_buf[2][i * chroma_width], chroma_width);
         memcpy(&double_buf[2][i * chroma_width * 2 + chroma_width],
                &read_buf[2][i * chroma_width], chroma_width);
      }

      frame[0] = double_buf[0];
      frame[1] = double_buf[1];
      frame[2] = double_buf[2];
   }


   if (res) {
      mjpeg_warn( "Decoding of Frame %d failed\n", numframe);
      /* TODO: Selective exit here... */
      return 1;
   }

   if (drop_lsb) {
      int *p, *end, c;
      int mask;
      int round;
      end = 0;                  /* Silence compiler */
      for (c = 0; c < sizeof(int); ++c) {
         ((char *) &mask)[c] = (0xff << drop_lsb) & 0xff;
         ((char *) &round)[c] = roundadj[drop_lsb];
      }


      /* we know output_width is multiple of 16 so doing lsb dropping
         int-wise will work for sane (32-bit or 64-bit) machines
       */

      for (c = 0; c < 3; ++c) {
         p = (int *) frame[c];
         if (c == 0) {
            end = (int *) (frame[c] + output_width * output_height);
         } else {
            end =
                (int *) (frame[c] +
                         chroma_output_width * chroma_output_height);
         }
         while (p++ < end) {
            *p = (*p & mask) + round;
         }
      }
   }

   if (param_noise_filt) {
      unsigned char *bp;
      unsigned char *p = frame[0] + output_width + 1;
      unsigned char *end = frame[0] + output_width * (output_height - 1);

      bp = filter_buf + output_width + 1;
      if (param_noise_filt == 1) {
         for (p = frame[0] + output_width + 1; p < end; ++p) {
            register int f =
                (p[-output_width - 1] + p[-output_width] +
                 p[-output_width + 1] + p[-1] + p[1] + p[output_width -
                                                         1] +
                 p[output_width] + p[output_width + 1]);
            f = f + ((*p) << 3) + ((*p) << 4);	/* 8 + (8 + 16) = 32 */
            *bp = (f + 8) >> (4 + 1);
            ++bp;
         }
      } else if (param_noise_filt == 2) {
         for (p = frame[0] + output_width + 1; p < end; ++p) {
            register int f =
                (p[-output_width - 1] + p[-output_width] +
                 p[-output_width + 1] + p[-1] + p[1] + p[output_width -
                                                         1] +
                 p[output_width] + p[output_width + 1]);

            f = f + ((*p) << 3);
            *bp = (f + 16) >> (3 + 1);
            ++bp;
         }
      } else {
         for (p = frame[0] + output_width + 1; p < end; ++p) {
            register int f =
                (p[-output_width - 1] + p[-output_width] +
                 p[-output_width + 1] + p[-1] + p[1] + p[output_width -
                                                         1] +
                 p[output_width] + p[output_width + 1]);
            /* f = f + (f<<1) + (*p << 3);
               *bp = (f + (1 << 4)) >> (3+2); */
            f = f + (*p << 3);
            *bp = (f + (1 << 2)) >> (3 + 1);
            ++bp;
         }
      }

      bp = filter_buf + output_width + 1;
      for (p = frame[0] + output_width + 1; p < end; ++p) {
         *p = *bp;
         ++bp;
      }


   }

   if (param_mono) {
      for (i = 0; i < chroma_output_width * chroma_output_height; ++i) {
         frame[1][i] = frame[2][i] = 0x80;
      }
   }
   return 0;

}

void writeoutYUV4MPEGheader(void)
{
   int frame_rate_code = yuv_fps2mpegcode (el.video_fps);

   if (frame_rate_code == 0) {
      mjpeg_warn("unrecognised frame-rate -  no MPEG2 rate code can be specified... encoder is likely to fail!\n");
   }

   yuv_write_header (1, output_width, output_height, frame_rate_code);
}

void writeoutframeinYUV4MPEG(unsigned char *frame[])
{
   int n = 0;
   int i;
   char *ptr;

   pipewrite(out_fd, "FRAME\n", 6);

   for (i = 0; i < active_height; i++) {
      ptr = &frame[0][luma_offset + (i * output_width)];
      if (luma_left_size) {
         memset(ptr, 0x00, luma_left_size);
      }
      if (luma_right_size) {
         memset(&ptr[luma_right_offset], 0x00, luma_right_size);
      }
   }

   for (i = 0; i < chroma_height; i++) {
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
      n += pipewrite(out_fd, luma_blank, luma_top_size);
   }
   n += pipewrite(out_fd, &frame[0][luma_offset], luma_size);
   if (luma_bottom_size) {
      n += pipewrite(out_fd, luma_blank, luma_bottom_size);
   }


   if (chroma_top_size) {
      n += pipewrite(out_fd, chroma_blank, chroma_top_size);
   }
   n += pipewrite(out_fd, &frame[1][chroma_offset], chroma_size);
   if (chroma_bottom_size) {
      n += pipewrite(out_fd, chroma_blank, chroma_bottom_size);
   }

   if (chroma_top_size) {
      n += pipewrite(out_fd, chroma_blank, chroma_top_size);
   }
   n += pipewrite(out_fd, &frame[2][chroma_offset], chroma_size);
   if (chroma_bottom_size) {
      n += pipewrite(out_fd, chroma_blank, chroma_bottom_size);
   }
}

static int lum_mean;
static int last_lum_mean;
static int delta_lum;
static int scene_num;
static int scene_start;

void streamout(void)
{
   int framenum, movie_num=0, index[MAX_EDIT_LIST_FILES];
   long int oldframe=N_EL_FRAME(el.frame_list[0])-1;
   FILE *fd=NULL;
   char temp[32];

   if (!param_scenefile)
      writeoutYUV4MPEGheader();
   scene_num = scene_start = 0;
   if (param_scenefile)
   {
      int num_files, i;

      /* Increase speed */
      output_width = output_width/scene_detection_decimation;

      /* Output file */
      unlink(param_scenefile);
      fd = fopen(param_scenefile,"w");
      if(fd==0)
      {
         mjpeg_error_exit1("Can not open %s - no edit list written!\n",param_scenefile);
      }
      fprintf(fd,"LAV Edit List\n");
      fprintf(fd,"%s\n",el.video_norm=='n'?"NTSC":"PAL");
      for(i=0;i<MAX_EDIT_LIST_FILES;i++) index[i] = -1;
      for(i=0;i<el.video_frames;i++) index[N_EL_FILE(el.frame_list[i])] = 1;
      num_files = 0;
      for(i=0;i<MAX_EDIT_LIST_FILES;i++) if(index[i]==1) index[i] = num_files++;
      fprintf(fd,"%d\n",num_files);
      for(i=0;i<MAX_EDIT_LIST_FILES;i++)
         if(index[i]>=0) fprintf(fd,"%s\n",el.video_file_list[i]);
      sprintf(temp,"%d %ld",index[N_EL_FILE(el.frame_list[0])],N_EL_FRAME(el.frame_list[0]));
   }
   for (framenum = param_offset; framenum < (param_offset + param_frames); ++framenum) {
      readframe(framenum, frame_buf);
      if (param_scenefile) {
         lum_mean =
             luminance_mean(frame_buf[0], output_width, output_height);
         if (framenum == 0)
         {
            delta_lum = 0;
            movie_num = N_EL_FILE(el.frame_list[0]);
         }
         else
            delta_lum = abs(lum_mean - last_lum_mean);
         last_lum_mean = lum_mean;

		 mjpeg_debug( "frame %d/%ld, lum_mean %d, delta_lum %d        \n", framenum,
		    el.video_frames, lum_mean, delta_lum);

         if (delta_lum > delta_lum_threshold || index[N_EL_FILE(el.frame_list[framenum])] != movie_num ||
               oldframe+1 != N_EL_FRAME(el.frame_list[framenum])) {
            if (delta_lum <= delta_lum_threshold)
                     fprintf(fd,"%c",'+'); /* Same scene, new file */
            fprintf(fd,"%s %ld\n", temp, N_EL_FRAME(el.frame_list[framenum-1]));
            sprintf(temp,"%d %ld",index[N_EL_FILE(el.frame_list[framenum])], N_EL_FRAME(el.frame_list[framenum]));
            scene_start = framenum;
            scene_num++;
         }

         oldframe = N_EL_FRAME(el.frame_list[framenum]);
         movie_num = N_EL_FILE(el.frame_list[framenum]);

      } else
         writeoutframeinYUV4MPEG(frame_buf);
   }
   if (param_scenefile)
   {
      fprintf(fd, "%s %ld\n", temp, N_EL_FRAME(el.video_frames-1));
      fclose(fd);
      mjpeg_info( "Editlist written to %s\n", param_scenefile);
   }
}

int main(argc, argv)
int argc;
char *argv[];
{
   int n, nerr = 0;
   char *geom;
   char *end;

   while ((n = getopt(argc, argv, "mv:a:s:d:n:S:T:D:o:f:")) != EOF) {
      switch (n) {

      case 'a':
         geom = optarg;
         active_width = strtol(geom, &end, 10);
         if (*end != 'x' || active_width < 100) {
			 mjpeg_error("Bad width parameter\n");
            nerr++;
            break;
         }

         geom = end + 1;
         active_height = strtol(geom, &end, 10);
         if ((*end != '+' && *end != '\0') || active_height < 100) {
            mjpeg_error( "Bad height parameter\n");
            nerr++;
            break;
         }

         if (*end == '\0')
            break;

         geom = end + 1;
         active_x = strtol(geom, &end, 10);
         if ((*end != '+' && *end != '\0') || active_x > 720) {
            mjpeg_error( "Bad x parameter\n");
            nerr++;
            break;
         }


         geom = end + 1;
         active_y = strtol(geom, &end, 10);
         if (*end != '\0' || active_y > 240) {
            mjpeg_error( "Bad y parameter\n");
            nerr++;
            break;
         }
         break;


      case 's':
         param_special = atoi(optarg);
         if (param_special < 0 || param_special > 4) {
            mjpeg_error( "-s option requires arg 0, 1, 2 or 3\n");
            nerr++;
         }
         break;

      case 'v':
         verbose = atoi(optarg);
         if (verbose < 0 ||verbose > 2) {
            mjpeg_error( "-v option requires arg 0, 1, or 2\n");
            nerr++;
         }
         break;

      case 'd':
         param_drop_lsb = atoi(optarg);
         if (param_drop_lsb < 0 || param_drop_lsb > 3) {
            mjpeg_error( "-d option requires arg 0..3\n");
            nerr++;
         }
         break;

      case 'n':
         param_noise_filt = atoi(optarg);
         if (param_noise_filt < 0 || param_noise_filt > 2) {
            mjpeg_error( "-n option requires arg 0..2\n");
            nerr++;
         }
         break;
      case 'm':
         param_mono = 1;
         break;
      case 'S':
         param_scenefile = optarg;
         break;
      case 'T':
         delta_lum_threshold = atoi(optarg);
         break;
      case 'D':
         scene_detection_decimation = atoi(optarg);
         break;
      case 'o':
         param_offset = atoi(optarg);
         break;
      case 'f':
         param_frames = atoi(optarg);
         break;
      default:
         nerr++;
      }
   }

   if (optind >= argc)
      nerr++;

   if (nerr)
      Usage(argv[0]);

   (void)mjpeg_default_handler_verbosity(verbose);


   /* Open editlist */

   read_video_files(argv + optind, argc - optind, &el);

   output_width = el.video_width;
   output_height = el.video_height;

   if (param_offset < 0) {
      param_offset = el.video_frames + param_offset;
   }
   if (param_offset >= el.video_frames) {
      mjpeg_error_exit1("offset greater than # of frames in input\n");
   }
   if ((param_offset + param_frames) > el.video_frames) {
	   mjpeg_warn("input too short for -f %d\n", param_frames);
	   param_frames = el.video_frames - param_offset;
   }
   if (param_frames == 0) {
      param_frames = el.video_frames - param_offset;
   }

   switch (param_special) {

   case 1:
      if (el.video_inter) {
         output_width = (el.video_width / 2) & 0xfffffff0;
         output_height = el.video_height / 2;
      } else {
         mjpeg_error(
                 "-s 1 may only be set for interlaced video sources\n");
         Usage(argv[0]);
      }
      break;

   case 2:
      if (el.video_width > 700) {
         output_width = 480;
      } else {
         mjpeg_error(
                 "-s 2 may only be set for 720 pixel wide video sources\n");
         Usage(argv[0]);
      }
      break;

   case 3:
      if (el.video_width > 700) {
         output_width = 352;
      } else {
         mjpeg_error(
                 "-s 3 may only be set for 720 pixel wide video sources\n");
         Usage(argv[0]);
      }
      break;

   case 4:
      if (el.video_width > 700) {
         output_width = 480;
         output_height = 480;
      } else {
         mjpeg_error(
                 "-s 4 may only be set for 720 pixel wide video sources\n");
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
      mjpeg_error( "active offset + acitve width > image size\n");
      Usage(argv[0]);
   }

   if (active_height + active_y > output_height) {
      mjpeg_error( "active offset + active height > image size\n");
      Usage(argv[0]);
   }

   out_fd = 1;                  /* stdout */
   init();
   if (delta_lum_threshold != -1) streamout();
   else write_edit_list(param_scenefile, 0, el.video_frames, &el);
   return 0;
}
