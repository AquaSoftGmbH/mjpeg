/*
  *  yuvfps.c
  *  Copyright (C) 2002 Alfonso Garcia-Patiño Barbolani <barbolani@jazzfree.com>
  *
  *  Upsamples or downsamples a yuv stream to a specified frame rate
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
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "yuv4mpeg.h"
#include "mpegconsts.h"

#define YUVFPS_VERSION "0.1"

static void print_usage() 
{
  fprintf (stderr,
	   "usage: yuvfps -r [NewFpsNum:NewFpsDen] [-s [InputFpsNum:InputFpsDen]] [-c] [-n] [-v -h]\n"
	   "yuvfps resamples a yuv video stream read from stdin to a new stream, identical\n"
           "to the source with frames repeated/copied/removed written to stdout.\n"
           "\n"
           "\t -r Frame rate for the resulting stream (in X:Y fractional form)\n"
           "\t -s Assume this source frame rate ignoring source YUV header\n"
           "\t -c Change only the output header frame rate, does not modify stream\n"
           "\t -n don't try to normalize the input framerate\n"
	   "\t -v Verbosity degree : 0=quiet, 1=normal, 2=verbose/debug\n"
	   "\t -h print this help\n"
         );
}
//
// Resamples the video stream coming from fdIn and writes it
// to fdOut.
// There are two variations of the same theme:
//
//   Upsampling: frames are duplicated when needed
//   Downsampling: frames from the original are skipped
//
//   Parameters:
//
//      fdIn,fdOut : File descriptors for reading and writing the stream
//      inStream, outStream: stream handlers for the source and destination streams.
//                           It is assumed that they are correctly initialized from/to
//                           their respective file I/O streams with the desired sample rates.
//
//      src_frame_rate, frame_rate: ratios for source and destination frame rate
//                           (note that this may not match the information contained
//                            in the stream itself due to command line options      
//                           )
// 
//  In both cases a Bresenham - style algorithm is used.
//
static void resample(  int fdIn 
                      , y4m_stream_info_t  *inStrInfo
                      , y4m_ratio_t src_frame_rate
                      , int fdOut
                      , y4m_stream_info_t  *outStrInfo
                      , y4m_ratio_t frame_rate
                      , int not_normalize 
                    )
{
  y4m_frame_info_t   in_frame ;
  uint8_t            *yuv_data[3] ;
  int                read_error_code ;
  int                write_error_code ;
  int                src_frame_counter ;
  int                dest_frame_counter ;
  // To perform bresenham resampling
  long long          srcInc ;
  long long          dstInc ;
  long long          currCount ;
  y4m_ratio_t        normalized_ratio;

  // Allocate memory for the YUV channels
  yuv_data[0] = (uint8_t *)malloc(y4m_si_get_plane_length(inStrInfo, 0));
  yuv_data[1] = (uint8_t *)malloc(y4m_si_get_plane_length(inStrInfo, 1));
  yuv_data[2] = (uint8_t *)malloc(y4m_si_get_plane_length(inStrInfo, 2));

  if( !yuv_data[0] || !yuv_data[1] || !yuv_data[2] )
    mjpeg_error_exit1 ("Could'nt allocate memory for the YUV4MPEG data!");
 
  if (not_normalize == 0)
    { /* Trying to normalize the values */
      normalized_ratio = mpeg_conform_framerate( 
                          (double)src_frame_rate.n/(double)src_frame_rate.d );
      mjpeg_warn( "Original framerate: %d:%d, Normalized framerate: %d:%d", 
 src_frame_rate.n, src_frame_rate.d, normalized_ratio.n, normalized_ratio.d );
      src_frame_rate.n = normalized_ratio.n;
      src_frame_rate.d = normalized_ratio.d;
    }
  
  mjpeg_warn( "Converting from %d:%d to %d:%d", 
               src_frame_rate.n,src_frame_rate.d,frame_rate.n,frame_rate.d  );

  /* Initialize counters */
  srcInc = (long long)src_frame_rate.n * (long long)frame_rate.d ;
  dstInc = (long long)frame_rate.n * (long long)src_frame_rate.d ;

  write_error_code = Y4M_OK ;

  src_frame_counter = 0 ;
  dest_frame_counter = 0 ;
  y4m_init_frame_info( &in_frame );
  read_error_code = y4m_read_frame(fdIn,inStrInfo,&in_frame,yuv_data );
  ++src_frame_counter ;
  currCount = 0 ;

  while( Y4M_ERR_EOF != read_error_code && write_error_code == Y4M_OK ) {
    write_error_code = y4m_write_frame( fdOut, outStrInfo, &in_frame, yuv_data );
    mjpeg_info( "Writing source frame %d at dest frame %d", src_frame_counter,++dest_frame_counter );
    currCount += srcInc ;
    while( currCount >= dstInc && Y4M_ERR_EOF != read_error_code ) {
      currCount -= dstInc ;
      ++src_frame_counter ;
      y4m_fini_frame_info( &in_frame );
      y4m_init_frame_info( &in_frame );
      read_error_code = y4m_read_frame(fdIn, inStrInfo,&in_frame,yuv_data );
    }
  }
  // Clean-up regardless an error happened or not
  y4m_fini_frame_info( &in_frame );
  free( yuv_data[0] );
  free( yuv_data[1] );
  free( yuv_data[2] );

  if( read_error_code != Y4M_ERR_EOF )
    mjpeg_error_exit1 ("Error reading from input stream!");
  if( write_error_code != Y4M_OK )
    mjpeg_error_exit1 ("Error writing output stream!");

}

// *************************************************************************************
// MAIN
// *************************************************************************************
int main (int argc, char *argv[])
{

  int verbose = LOG_ERROR ;
  int change_header_only = 0 ;
  int not_normalize = 0;
  int fdIn = 0 ;
  int fdOut = 1 ;
  y4m_stream_info_t in_streaminfo, out_streaminfo ;
  y4m_ratio_t frame_rate, src_frame_rate ;

  const static char *legal_flags = "r:s:cnv:h";
  int c ;
  
  while ((c = getopt (argc, argv, legal_flags)) != -1) {
        switch (c)
        {
        /* New frame rate */
        case 'r':
          if( Y4M_OK != y4m_parse_ratio(&frame_rate, optarg) )
              mjpeg_error_exit1 ("Syntax for frame rate should be Numerator:Denominator");
          break;
        /* Assumed frame rate for source (useful when the header contains an
           invalid frame rate) */
        case 's':
          if( Y4M_OK != y4m_parse_ratio(&src_frame_rate,optarg) )
              mjpeg_error_exit1 ("Syntax for frame rate should be Numerator:Denominator");
          break ;
        /* Only change header frame-rate, not the stream itself */
        case 'c':
          change_header_only = 1 ;
        case 'n':
          not_normalize = 1;
        break;
        case 'v':
          verbose = atoi (optarg);
          if (verbose < 0 || verbose > 2)
            mjpeg_error_exit1 ("Verbose level must be [0..2]");
          break;
        
        case 'h':
        case '?':
          print_usage (argv);
          return 0 ;
          break;
        }
  }

  y4m_accept_extensions(1);

  /* mjpeg tools global initialisations */
  mjpeg_default_handler_verbosity (verbose);

  /* Initialize input streams */
  y4m_init_stream_info (&in_streaminfo);
  y4m_init_stream_info (&out_streaminfo);

  // ***************************************************************
  // Get video stream informations (size, framerate, interlacing, aspect ratio).
  // The streaminfo structure is filled in
  // ***************************************************************
  // INPUT comes from stdin, we check for a correct file header
  if (y4m_read_stream_header (fdIn, &in_streaminfo) != Y4M_OK)
    mjpeg_error_exit1 ("Could'nt read YUV4MPEG header!");
  if (y4m_si_get_plane_count(&in_streaminfo) != 3)
     mjpeg_error_exit1("Only 3 plane formats supported");

  /* Prepare output stream */
  src_frame_rate = y4m_si_get_framerate( &in_streaminfo );
  y4m_copy_stream_info( &out_streaminfo, &in_streaminfo );
  
  /* Information output */
  mjpeg_info ("yuv2fps (version " YUVFPS_VERSION
              ") is a general frame resampling utility for yuv streams");
  mjpeg_info ("(C) 2002 Alfonso Garcia-Patino Barbolani <barbolani@jazzfree.com>");
  mjpeg_info ("yuvfps -h for help, or man yuvfps");

  y4m_si_set_framerate( &out_streaminfo, frame_rate );                
  y4m_write_stream_header(fdOut,&out_streaminfo);
  if( change_header_only )
    frame_rate = src_frame_rate ;
    
  /* in that function we do all the important work */
  resample( fdIn,&in_streaminfo, src_frame_rate, fdOut,&out_streaminfo,
            frame_rate, not_normalize );

  y4m_fini_stream_info (&in_streaminfo);
  y4m_fini_stream_info (&out_streaminfo);

  return 0;
}
/*
 * Local variables:
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
