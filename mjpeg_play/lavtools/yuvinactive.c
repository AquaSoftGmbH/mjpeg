/**************************************************************************** 
  * yuvinactive.c
  * Copyright (C) 2003 Bernhard Praschinger 
  * 
  * Sets a area in the yuv frame to black
  * 
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *
  *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with this program; if not, write to the Free Software
  *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  *
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "yuv4mpeg.h"

#define LUMA 16
#define CROMA 128

/* some defintions */
extern char *__progname;
struct area_s {
      int width;    /**< width of the area */
      int height;   /**< height of the area */
      int voffset;  /**< vertical offset from the top left boarder */
      int hoffset;  /**< horizontal offest from the top left boarder */
              };

struct color_yuv {
      int luma;       /**< the luma to use */
      int croma_b;    /**< the croma Cb to use */
      int croma_r;    /**< the croma Cr to use */
         };

int verbose = 1;

/* protoypes */
static void print_usage();
void fillarea(char area[20], struct area_s *inarea);
void set_inactive(struct area_s inarea, int horz, int vert, uint8_t *frame[], struct color_yuv *coloryuv);
void set_yuvcolor(char area[20], struct color_yuv *coloryuv);

/* Here we start the programm */

/** The typical help output */
static void print_usage()
{
  fprintf(stderr, "%s usage: -i XxY+XOFF+YOFF\n\n",__progname);
  fprintf(stderr, " -h -H            - print out this help\n");
  fprintf(stderr, " -i X+Y+XOFF+YOFF - the area which will be set inactive, from top left\n");
  fprintf(stderr, "                  - X=Width, Y=Height, XOFF=VerticalOffset, YOFF=HorizonalOffset\n");
  fprintf(stderr, " -s luma,Cb,Cr    - set the filling color in yuv format");
  fprintf(stderr, "\n");
  exit(1);
}

/** Here we set the color to use for filling the area */
void set_yuvcolor(char area[20], struct color_yuv *coloryuv)
{
int i;
unsigned int u1, u2, u3;

  i = sscanf (area, "%i,%i,%i", &u1, &u2, &u3);

  if ( 3 == i )
    {
      (*coloryuv).luma    = u1;
      (*coloryuv).croma_b = u2;
      (*coloryuv).croma_r = u3;
      if ( ((*coloryuv).luma > 235) || ((*coloryuv).luma < 16) )
        mjpeg_error_exit1("out of range value for luma given: %i, \n"
                          " allowed values 16-235", (*coloryuv).luma);
      if ( ((*coloryuv).croma_b > 240) || ((*coloryuv).croma_b < 16) )
        mjpeg_error_exit1("out of range value for Cb given: %i, \n"
                          " allowed values 16-240", (*coloryuv).croma_b);
      if ( ((*coloryuv).croma_r > 240) || ((*coloryuv).croma_r < 16) )
        mjpeg_error_exit1("out of range value for Cr given: %i, \n"
                          " allowed values 16-240", (*coloryuv).croma_r);

       mjpeg_info("got luma %i, Cb %i, Cr %i ", 
                 (*coloryuv).luma, (*coloryuv).croma_b, (*coloryuv).croma_r );
    }
  else 
    mjpeg_error_exit1("Wrong number of colors given, %s", area);
}

/** Here we cut out the number of the area string */
void fillarea(char area[20], struct area_s *inarea)
{
int i;
unsigned int u1, u2, u3, u4;

  /* Cuting out the numbers of the stream */
  i = sscanf (area, "%ix%i+%i+%i", &u1, &u2, &u3, &u4);

  if ( 4 == i)  /* Checking if we have got 4 numbers */
    {
       (*inarea).width = u1;
       (*inarea).height = u2;
       (*inarea).voffset = u3;
       (*inarea).hoffset = u4;
      if (verbose >= 1)
         mjpeg_info("got the area : W %i, H %i, Xoff %i, Yoff %i",
         (*inarea).width,(*inarea).height,(*inarea).voffset,(*inarea).hoffset); 
    }
  else 
    mjpeg_error_exit1("Wrong inactive sting given: %s", area);

}

/** Here is the first stage of setting the stream to black */
void set_inactive(struct area_s inarea, int horz, int vert, uint8_t *frame[],
                  struct color_yuv *coloryuv)
{
int i, hoffset_pix;
uint8_t *plane_l, *plane_cb, *plane_cr;

plane_l = frame[0];
plane_cb= frame[1];
plane_cr= frame[2];

/* Number of pixles for the luma */
hoffset_pix = (horz * (inarea.hoffset) ) + inarea.voffset;

for (i = 0; i < (inarea.height); i++) /* Setting the Luma */
  {
    memset( (plane_l + hoffset_pix), (*coloryuv).luma , (inarea.width) );
    hoffset_pix = hoffset_pix + horz;
  }

/* Number of pixles croma */
hoffset_pix = ((horz / 2)  * (inarea.hoffset/2) ) + (inarea.voffset / 2 );

for (i = 0; i < ((inarea.height/2)); i++) /*Setting the croma */
  {
    memset( (plane_cb + hoffset_pix), (*coloryuv).croma_b, (inarea.width/2) );
    memset( (plane_cr + hoffset_pix), (*coloryuv).croma_r, (inarea.width/2) );
    hoffset_pix = hoffset_pix + (horz/2);
  }

}

/** MAIN */
int main( int argc, char **argv)
{
int c,i, frame_count;
int horz, vert;      /* width and height of the frame */
uint8_t *frame[3];  /*pointer to the 3 color planes of the input frame */
char area [20];
struct area_s inarea;
struct color_yuv coloryuv;
int input_fd = 0;    /* std in */
int output_fd = 1;   /* std out */
y4m_stream_info_t istream, ostream;
y4m_frame_info_t iframe;

inarea.width=0; inarea.height=0; inarea.voffset=0; inarea.hoffset=0;

coloryuv.luma    = LUMA;  /*Setting the luma to black */
coloryuv.croma_b = CROMA; /*Setting the croma to center, means white */
coloryuv.croma_r = CROMA; /*Setting the croma to center, means white */

(void)mjpeg_default_handler_verbosity(verbose);

  while ((c = getopt(argc, argv, "Hhv:i:s:")) != -1)
    {
    switch (c)
      {
         case 'v':
           verbose = atoi(optarg);
           if ( verbose < 0 || verbose > 2)
             print_usage();
           if (verbose == 2)
             mjpeg_info("Set Verbose = %i", verbose);
           break;
         case 'i':
           strncpy(area, optarg, 20); /* This part shows how to process */
           fillarea(area, &inarea);   /* command line options */
           break;
         case 's':
           strncpy(area, optarg, 20); 
           set_yuvcolor(area, &coloryuv);
           break;
         case 'H':
         case 'h':
           print_usage();
      }
    }

  /* Checking if we have used the -i option */
  if ( (inarea.height == 0) && (inarea.width == 0) )
    mjpeg_error_exit1("You have to use the -i option");

  y4m_init_stream_info(&istream);
  y4m_init_stream_info(&ostream);
  y4m_init_frame_info(&iframe);

  /* First read the header of the y4m stream */
  i = y4m_read_stream_header(input_fd, &istream);
  
  if ( i != Y4M_OK)   /* a basic check if we really have y4m stream */
    mjpeg_error_exit1("Input stream error: %s", y4m_strerr(i));
  else 
    {
      /* Here we copy the input stream info to the output stream info header */
      y4m_copy_stream_info(&ostream, &istream);

      /* Here we write the new output header to the output fd */
      y4m_write_stream_header(output_fd, &ostream);

      horz = istream.width;   /* get the width of the frame */
      vert = istream.height;  /* get the height of the frame */

      mjpeg_info("\nWidth %i \n", inarea.width );
      mjpeg_info("\nheight %i \n", inarea.height );

      if ( (inarea.width + inarea.voffset) > horz)
      mjpeg_error_exit1("Input width and offset larger than framewidth,exit");
       
      if ( (inarea.height + inarea.hoffset) > vert)
      mjpeg_error_exit1("Input height and offset larger than frameheight,exit");

      /* Here we allocate the memory for on frame */
      frame[0] = malloc( horz * vert );
      frame[1] = malloc( (horz/2) * (vert/2) );
      frame[2] = malloc( (horz/2) * (vert/2) );

      /* Here we set the initial number of of frames */
      /* We do not need it. Just for showing that is does something */
      frame_count = 0 ; 

      /* This is the main loop here can filters effects, scaling and so 
      on be done with the video frames. Just up to your mind */
      /* We read now a single frame with the header and check if it does not
      have any problems or we have alreaddy processed the last without data */
      while(y4m_read_frame(input_fd, &istream, &iframe, frame) == Y4M_OK)
        {
           frame_count++; 

           /* You can do something usefull here */
           set_inactive(inarea, horz, vert, frame, &coloryuv);

           /* Now we put out the read frame */
           y4m_write_frame(output_fd, &ostream, &iframe, frame);
        }

      /* Writing the headers that show other programs that the stream ends */
      y4m_fini_stream_info(&istream);
      y4m_fini_stream_info(&ostream);
      y4m_fini_frame_info(&iframe);

    }

    /* giving back the memory to the system */
    free(frame[0]);
    frame[0] = 0;
    free(frame[1]);
    frame[1] = 0;
    free(frame[2]);
    frame[2] = 0;

  exit(0); /* exiting */ 
}
