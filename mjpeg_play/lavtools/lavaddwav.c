/*
    lavaddwav: Add a WAV file as soundtrack to an AVI or QT file

    Usage: lavaddwav AVI_or_QT_file WAV_file Output_file

    Multiple output file version by Nicholas Redgrave 
    <baron@bologrew.demon.co.uk> 8th January 2005
    Use "%d" style output filenames for multiple files.
    
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
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "lav_io.h"
#include "mjpeg_logging.h"

#define FOURCC(a,b,c,d) ( (d<<24) | ((c&0xff)<<16) | ((b&0xff)<<8) | (a&0xff) )

#define FOURCC_RIFF     FOURCC ('R', 'I', 'F', 'F')
#define FOURCC_WAVE     FOURCC ('W', 'A', 'V', 'E')
#define FOURCC_FMT      FOURCC ('f', 'm', 't', ' ')
#define FOURCC_DATA     FOURCC ('d', 'a', 't', 'a')

#define MAX_MBYTES_PER_FILE_32 ((0x7fffffff >> 20) * 85/100)/* Is less than 2^31 and 2*10^9 */

int verbose = 1;

int main(int argc, char **argv)
{
   int i, n, res, fmtlen, max_frame_size;
   long video_frames;
   double fps;
   long audio_samps;
   long audio_rate;
   int  audio_chans;
   int  audio_bits;
   int  audio_bps;
   long absize;
   long na_out;
   uint8_t *vbuff, *abuff;
   lav_file_t *lav_fd, *lav_out;
   int wav_fd;
   long data[64];
   int have_percent_n = 0;

   char chOutFile[128] = "\0";
   int iOutNum = 1;
   unsigned long ulOutputBytes = 0;

   if(argc != 4)
   {
      fprintf(stderr,"Usage:\n\n");
      fprintf(stderr,"   %s AVI_or_QT_file WAV_file Output_file\n\n",argv[0]);
      fprintf(stderr,"   If the output file is to large for one AVI use %%0xd in\n");
      fprintf(stderr,"   the output filename so %s creates several files.\n",argv[0]);
      fprintf(stderr,"   The comannd lookes than like: %s video.avi sound.wav output%%02d.avi\n",argv[0]);
      exit(1);
   }

   /* Open AVI or Quicktime file */

   lav_fd = lav_open_input_file(argv[1]);
   if(!lav_fd)
   {
	   mjpeg_error_exit1("Error opening %s: %s",argv[1],lav_strerror());
   }

   /* Debug Output */

   mjpeg_debug("File: %s",argv[1]);
   mjpeg_debug("   frames:      %8ld",lav_video_frames(lav_fd));
   mjpeg_debug("   width:       %8d",lav_video_width (lav_fd));
   mjpeg_debug("   height:      %8d",lav_video_height(lav_fd));
   mjpeg_debug("   interlacing: %8d",lav_video_interlacing(lav_fd));
   mjpeg_debug("   frames/sec:  %8.3f",lav_frame_rate(lav_fd));
   mjpeg_debug(" ");
   video_frames = lav_video_frames(lav_fd);
   fps = lav_frame_rate(lav_fd);
   if(fps<=0)
   {
	   mjpeg_error_exit1("Framerate illegal");
   }

   /* Open WAV file */

   wav_fd = open(argv[2],O_RDONLY);
   if(wav_fd<0) { mjpeg_error_exit1("Open WAV file: %s", strerror(errno));}

   n = read(wav_fd,(char*)data,20);
   if(n!=20) { mjpeg_error_exit1("Read WAV file: %s", strerror(errno)); }

   if(data[0] != FOURCC_RIFF || data[2] != FOURCC_WAVE ||
      data[3] != FOURCC_FMT  || data[4] > sizeof(data) )
   {
      mjpeg_error_exit1("Error in WAV header");
   }

   fmtlen = data[4];

   n = read(wav_fd,(char*)data,fmtlen);
   if(n!=fmtlen) { perror("read WAV header"); exit(1); }

   if( (data[0]&0xffff) != 1)
   {
      mjpeg_error_exit1("WAV file is not in PCM format");
   }

   audio_chans = (data[0]>>16) & 0xffff;
   audio_rate  = data[1];
   audio_bits  = (data[3]>>16) & 0xffff;
   audio_bps   = (audio_chans*audio_bits+7)/8;

   if(audio_bps==0) audio_bps = 1; /* safety first */

   n = read(wav_fd,(char*)data,8);
   if(n!=8) { mjpeg_error_exit1("Read WAV header: %s", strerror(errno)); }

   if(data[0] != FOURCC_DATA)
   {
      mjpeg_error_exit1("Error in WAV header");
   }
   audio_samps = data[1]/audio_bps;

   /* Debug Output */

   mjpeg_debug("File: %s",argv[2]);
   mjpeg_debug("   audio samps: %8ld",audio_samps);
   mjpeg_debug("   audio chans: %8d",audio_chans);
   mjpeg_debug("   audio bits:  %8d",audio_bits);
   mjpeg_debug("   audio rate:  %8ld",audio_rate);
   mjpeg_debug(" ");
   mjpeg_debug("Length of video:  %15.3f sec",video_frames/fps);
   mjpeg_debug("Length of audio:  %15.3f sec",(double)audio_samps/(double)audio_rate);
   mjpeg_debug(" ");


   max_frame_size = 0;
   for(i=0;i<lav_video_frames(lav_fd);i++)
   {
      if(lav_frame_size(lav_fd,i) > max_frame_size)
         max_frame_size = lav_frame_size(lav_fd,i);
   }
   vbuff = (uint8_t*) malloc(max_frame_size);

   absize = audio_rate/fps+0.5;
   absize *= audio_bps;
   abuff = (uint8_t*) malloc(absize);

   if(vbuff==0 || abuff==0)
   {
      mjpeg_error_exit1("Out of Memory - malloc failed");
   }

   lav_seek_start(lav_fd);

   na_out = 0;
   i = 0;
   
   /* check for enough string space for output filename */
   if( strlen(argv[3]) > (sizeof(chOutFile)-2) )
   {
      mjpeg_error_exit1("Insufficient string space to create output filename");
   }

   
   /* outer loop */
   while(1)
   {
      
      /* rough check for "%d" style filename */
      if( strchr(argv[3], 37) != NULL )
      {
         /* build output filename */
         sprintf(chOutFile, argv[3], iOutNum++);
         have_percent_n = 1;
      }
      else
      {
         strncpy(chOutFile, argv[3], sizeof(chOutFile)-1);
         chOutFile[sizeof(chOutFile)-1] = 0;
      }
      
      lav_out = lav_open_output_file(chOutFile,
                                     lav_fd->format,
                                     lav_video_width (lav_fd),
                                     lav_video_height(lav_fd),
                                     lav_video_interlacing(lav_fd),
                                     fps,
                                     audio_bits,
                                     audio_chans,
                                     audio_rate);
      if(!lav_out)
      {
         mjpeg_error_exit1("Error opening %s: %s", chOutFile, lav_strerror());
      }
   

      while( i < lav_video_frames(lav_fd) )
      {
         res = lav_read_frame(lav_fd,vbuff);
         if(res<0)
         {
            mjpeg_error("Reading video frame: %s",lav_strerror());
            lav_close(lav_out);
            exit(1);
         }
         res = lav_write_frame(lav_out,vbuff,lav_frame_size(lav_fd,i),1);
         if(res<0)
         {
            mjpeg_error("Writing video frame: %s", lav_strerror());
            lav_close(lav_out);
            exit(1);
         }
         ulOutputBytes += (unsigned long)lav_frame_size(lav_fd,i);
         
         n = read(wav_fd,abuff,absize);
         if(n>0)
         {
            na_out += n/audio_bps;
            res = lav_write_audio(lav_out,abuff,n/audio_bps);
            if(res<0)
            {
               mjpeg_error("Error writing audio: %s",lav_strerror());
               lav_close(lav_out);
               exit(1);
            }
            ulOutputBytes += (unsigned long)n;
         }
         
         i++;
         /* check for exceeding maximum output file size */
         if( (ulOutputBytes >> 20) >= (unsigned long)MAX_MBYTES_PER_FILE_32 )
         {
	    if (have_percent_n)
              {
	         mjpeg_debug("  Starting new sequence");
                 ulOutputBytes = 0;
                 break;
              }
            else
              {
                 mjpeg_error_exit1("Max file size reaced use %%0xd in your output filename");
              }
         }
      
      } /* end of while( i < lav_video_frames(lav_fd) ) */

      
      if( ulOutputBytes == 0 )
      {
         lav_close(lav_out);
      }
      else
      {
         break;
      }
      
   } /* end of while(1) */
   

      
   /* copy remaining audio */
   
   do
   {
      n = read(wav_fd,abuff,absize);
      if(n>0)
      {
         na_out += n/audio_bps;
         res = lav_write_audio(lav_out,abuff,n/audio_bps);
         if(res<0)
         {
            mjpeg_error("Writing audio: %s",lav_strerror());
            lav_close(lav_out);
            exit(1);
         }
      }
   }
   while(n>0);
   
   if(na_out != audio_samps)
      mjpeg_warn("audio samples expected: %ld, written: %ld",
                 audio_samps, na_out);
   
   lav_close(lav_out);
   return 0;
}
