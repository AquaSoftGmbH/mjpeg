/*
 * liblavrec - a librarified Linux Audio Video Record-application
 *
 * Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>
 * Extended by:     Gernot Ziegler  <gz@lysator.liu.se>
 *               &  Wolfgang Scherr <scherr@net4you.net>
 *               &  Ronald Bultje   <rbultje@ronald.bitfreak.net>
 *               &  many others
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <config.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#ifdef IRIX
#include <sys/statfs.h>
#endif

#include <sys/vfs.h>
#include <stdlib.h>
#include <getopt.h>

#ifndef IRIX
#include <linux/videodev.h>
#include <linux/soundcard.h>
#endif 

#include <videodev_mjpeg.h>
#include <pthread.h>

#include "mjpeg_types.h"
#include "liblavrec.h"
#include "lav_io.h"
#include "audiolib.h"
#include "jpegutils.h"

/* On some systems MAP_FAILED seems to be missing */
#ifndef MAP_FAILED
#define MAP_FAILED ( (caddr_t) -1 )
#endif

#define MJPEG_MAX_BUF 64

#define NUM_AUDIO_TRIES 500 /* makes 10 seconds with 20 ms pause beetween tries */

#define MAX_MBYTES_PER_FILE 1600  /* Maximum number of Mbytes per file.
                                     We make a conservative guess since we
                                     only count the number of bytes output
                                     and don't know how big the control
                                     information will be */
#define MIN_MBYTES_FREE 10        /* Minimum number of Mbytes that should
                                     stay free on the file system, this is also
                                     only a guess */
#define MIN_MBYTES_FREE_OPEN 20   /* Minimum number of Mbytes that have to be
                                     free in the filesystem when opening a new file */
#define CHECK_INTERVAL 50         /* Interval for checking free space on file system */

#define VALUE_NOT_FILLED -10000


typedef struct {
   int    interlaced;                         /* is the video interlaced (even/odd-first)? */
   int    width;                              /* width of the captured frames */
   int    height;                             /* height of the captured frames */
   double spvf;                               /* seconds per video frame */
   int    video_fd;                           /* file descriptor of open("/dev/video") */
   struct mjpeg_requestbuffers breq;          /* buffer requests */
   struct video_mbuf softreq;                 /* Software capture (YUV) buffer requests */
   char   *MJPG_buff;                         /* the MJPEG buffer */
   struct video_mmap mm;                      /* software (YUV) capture info */
   unsigned char *YUV_buff;                   /* in case of software encoding: the YUV buffer */
   lav_file_t *video_file;                    /* current lav_io.c file we're recording to */
   lav_file_t *video_file_old;                /* previous lav_io.c file we're recording to (finish audio/close) */
   int    num_frames_old;

   char   AUDIO_buff[AUDIO_BUFFER_SIZE];      /* the audio buffer */
   struct timeval audio_t0;
   int    astat;
   int    mixer_set;                          /* whether the mixer settings were changed */
   int    audio_bps;                          /* bytes per second for audio stream */
   long   audio_buffer_size;                  /* audio stream buffer size */
   double spas;                               /* seconds per audio sample */
   double sync_lim;                           /* upper limit of 'out-of-sync' - if higher, quit */
   video_capture_stats* stats;                /* the stats */

   long   MBytes_fs_free;                     /* Free disk space when that was last checked */
   long   bytes_output_cur;                   /* Bytes output to the current output file */
   long   bytes_last_checked;                 /* Number of bytes that were output when the
                                                   free space was last checked */
   int    mixer_volume_saved;                 /* saved recording volume before setting mixer */
   int    mixer_recsrc_saved;                 /* saved recording source before setting mixer */
   int    mixer_inplev_saved;                 /* saved output volume before setting mixer */

   pthread_t encoding_thread;                 /* for software encoding recording */
   pthread_mutex_t valid_mutex;               /* for software encoding recording */
   int buffer_valid[MJPEG_MAX_BUF];           /* Non-zero if buffer has been filled */
   pthread_cond_t buffer_filled[MJPEG_MAX_BUF];
   int currently_encoded_frame;

   int    output_status;
   int    state;                              /* recording, paused or stoppped */
   pthread_t capture_thread;
} video_capture_setup;


/******************************************************
 * lavrec_msg()
 *   simplicity function which will give messages
 ******************************************************/

static void lavrec_msg(int type, lavrec_t *info, const char format[], ...) GNUC_PRINTF(3,4);
static void lavrec_msg(int type, lavrec_t *info, const char format[], ...)
{
   if (!info)
   {
      /* we can't let errors pass without giving notice */
      char buf[1024];
      va_list args;

      va_start(args, format);
      vsnprintf(buf, sizeof(buf)-1, format, args);

      printf("**ERROR: %s\n", buf);

      va_end(args);
   }
   else if (info->msg_callback)
   {
      char buf[1024];
      va_list args;

      va_start(args, format);
      vsnprintf(buf, sizeof(buf)-1, format, args);

      info->msg_callback(type, buf);

      va_end(args);
   }
   else if (type == LAVREC_MSG_ERROR)
   {
      /* we can't let errors pass without giving notice */
      char buf[1024];
      va_list args;

      va_start(args, format);
      vsnprintf(buf, sizeof(buf)-1, format, args);

      printf("**ERROR: %s\n", buf);

      va_end(args);
   }
}


/******************************************************
 * lavrec_change_state()
 *   changes the recording state
 ******************************************************/

static void lavrec_change_state(lavrec_t *info, int new_state)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   settings->state = new_state;
   if (info->state_changed)
      info->state_changed(settings->state);
}


/******************************************************
 * set_mixer()
 *   set the sound mixer:
 *    flag = 1 : set for recording from the line input
 *    flag = 0 : restore previously saved values
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_set_mixer(lavrec_t *info, int flag)
{
   int fd, var;
   int sound_mixer_read_input;
   int sound_mixer_write_input;
   int sound_mask_input;
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   /* Avoid restoring anything when nothing was set */
   if (flag==0 && settings->mixer_set==0) return 1;

#ifndef IRIX
   /* Open the audio device */
   fd = open(info->mixer_dev, O_RDONLY);
   if (fd == -1)
   {
      lavrec_msg(LAVREC_MSG_WARNING, info,
         "Unable to open sound mixer \'%s\', try setting the sound mixer with another tool!!!",
         info->mixer_dev);
      return 1; /* 0 means error, so although we failed return 1 */
   }

   switch(info->audio_src)
   {
      case 'm':
         sound_mixer_read_input  = SOUND_MIXER_READ_MIC;
         sound_mixer_write_input = SOUND_MIXER_WRITE_MIC;
         sound_mask_input        = SOUND_MASK_MIC;
         break;
      case 'c':
         sound_mixer_read_input  = SOUND_MIXER_READ_CD;
         sound_mixer_write_input = SOUND_MIXER_WRITE_CD;
         sound_mask_input        = SOUND_MASK_CD;
         break;
      case 'l':
         sound_mixer_read_input  = SOUND_MIXER_READ_LINE;
         sound_mixer_write_input = SOUND_MIXER_WRITE_LINE;
         sound_mask_input        = SOUND_MASK_LINE;
         break;
      default:
         lavrec_msg(LAVREC_MSG_WARNING, info,
            "Unknown sound source: \'%c\'", info->audio_src);
         close(fd);
         return 1; /* 0 means error, so although we failed return 1 */
   }

   if(flag==1)
   {
      int nerr = 0;

      /* Save the values we are going to change */
      if (settings->mixer_set == 0) {
         if (ioctl(fd, SOUND_MIXER_READ_VOLUME, &(settings->mixer_volume_saved)) == -1) nerr++;
         if (ioctl(fd, SOUND_MIXER_READ_RECSRC, &(settings->mixer_recsrc_saved)) == -1) nerr++;
         if (ioctl(fd, sound_mixer_read_input , &(settings->mixer_inplev_saved)) == -1) nerr++;
         settings->mixer_set = 1;

         if (nerr)
         {
            lavrec_msg (LAVREC_MSG_WARNING, info,
               "Unable to save sound mixer settings");
            lavrec_msg (LAVREC_MSG_WARNING, info,
               "Restore your favorite setting with another tool after capture");
            settings->mixer_set = 0; /* prevent us from resetting nonsense settings */
         }
      }

      /* Set the recording source, audio-level and (if wanted) mute */
      nerr = 0;

      var = sound_mask_input;
      if (ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &var) == -1) nerr++;
      var = 256*info->audio_level + info->audio_level; /* left and right channel */
      if (ioctl(fd, sound_mixer_write_input, &var) == -1) nerr++;
      if(info->mute) {
         var = 0;
         if (ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &var) == -1) nerr++;
      }

      if (nerr)
      {
         lavrec_msg (LAVREC_MSG_WARNING, info,
            "Unable to set the sound mixer correctly");
         lavrec_msg (LAVREC_MSG_WARNING, info,
            "Audio capture might not be successfull (try another mixer tool!)");
      }

   }
   else
   {
      int nerr = 0;

      /* Restore previously saved settings */
      if (ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &(settings->mixer_recsrc_saved)) == -1) nerr++;
      if (ioctl(fd, sound_mixer_write_input, &(settings->mixer_inplev_saved)) == -1) nerr++;
      if(info->mute)
         if (ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &(settings->mixer_volume_saved)) == -1) nerr++;

      if (nerr)
      {
         lavrec_msg (LAVREC_MSG_WARNING, info,
            "Unable to restore sound mixer settings");
         lavrec_msg (LAVREC_MSG_WARNING, info,
            "Restore your favorite setting with another tool");
      }
      settings->mixer_set = 0;
   }

   close(fd);
#else
   fprintf(stderr, "Audio mixer setting not supported in IRIX !\n");

#endif
   return 1;
}


/******************************************************
 * lavrec_autodetect_signal()
 *   (try to) autodetect signal/norm
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_autodetect_signal(lavrec_t *info)
{
#ifndef IRIX 
   struct mjpeg_status bstat;
   int i;

   video_capture_setup *settings = (video_capture_setup *)info->settings;

   lavrec_msg(LAVREC_MSG_INFO, info, "Auto detecting input and norm ...");

   if (info->software_encoding && (info->video_norm==3 || info->video_src==3))
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Autodetection of input or norm not supported for non-MJPEG-cards");
      return 0;
   }

   if (info->video_src == 3) /* detect video_src && norm */
   {
      int n = 0;

      for(i=0;i<2;i++)
      {
         lavrec_msg (LAVREC_MSG_INFO, info,
               "Trying %s ...", (i==2)?"TV tuner":(i==0?"Composite":"S-Video"));

         bstat.input = i;
         if (ioctl(settings->video_fd,MJPIOC_G_STATUS,&bstat) < 0)
         {
            lavrec_msg (LAVREC_MSG_ERROR, info,
               "Error getting video input status: %s",
               (char*)sys_errlist[errno]);
            return 0;
         }

         if (bstat.signal)
         {
            lavrec_msg (LAVREC_MSG_INFO, info,
               "Input present: %s %s",
               bstat.norm==0? "PAL":(info->video_norm==1?"NTSC":"SECAM"),
               bstat.color?"color":"no color");
            info->video_src = i;
            info->video_norm = bstat.norm;
            n++;
         }
         else
         {
            lavrec_msg (LAVREC_MSG_INFO, info,
               "No signal ion specified input");
         }
      }

      switch(n)
      {
         case 0:
            lavrec_msg (LAVREC_MSG_ERROR, info,
               "No input signal ... exiting");
            return 0;
         case 1:
            lavrec_msg (LAVREC_MSG_INFO, info,
               "Detected %s %s",
               info->video_norm==0? "PAL":(info->video_norm==1?"NTSC":"SECAM"),
               info->video_src==0?"Composite":(info->video_src==1?"S-Video":"TV tuner"));
            break;
         default:
            lavrec_msg (LAVREC_MSG_ERROR, info,
               "Input signal on more thn one input source... exiting");
            return 0;
      }

   }
   else if (info->video_norm == 3) /* detect norm only */
   {

      lavrec_msg (LAVREC_MSG_INFO, info,
         "Trying to detect norm for %s ...",
         (info->video_src==2) ? "TV tuner" : (info->video_src==0?"Composite":"S-Video"));

      bstat.input = info->video_src;
      if (ioctl(settings->video_fd,MJPIOC_G_STATUS,&bstat) < 0)
      {
         lavrec_msg (LAVREC_MSG_ERROR, info,
            "Error getting video input status: %s",sys_errlist[errno]);
         return 0;
      }

      info->video_norm = bstat.norm;

      lavrec_msg (LAVREC_MSG_INFO, info,
         "Detected %s",
         info->video_norm==0? "PAL":(info->video_norm==1?"NTSC":"SECAM"));
   }

   return 1;
#else
   fprintf(stderr, "Auto detection of video signal not supported in IRIX !\n");

   return 1;
#endif
}


/******************************************************
 * lavrec_get_free_space()
 *   get the amount of free disk space
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static long lavrec_get_free_space(video_capture_setup *settings)
{
   long blocks_per_MB;
   struct statfs statfs_buf;
   long MBytes_fs_free;

#ifndef IRIX
   /* check the disk space again */
   if (statfs(settings->stats->output_filename, &statfs_buf))
   {
      /* some error happened */
      MBytes_fs_free = MAX_MBYTES_PER_FILE; /* some fake value */
   }
   else
   {
      blocks_per_MB = (1024*1024) / statfs_buf.f_bsize;
      MBytes_fs_free = statfs_buf.f_bavail/blocks_per_MB;
   }
   settings->bytes_last_checked = settings->bytes_output_cur;
#else
   /* check the disk space again */
   if (statfs(settings->stats->output_filename, &statfs_buf, sizeof(struct statfs), 0))
   {
      /* some error happened */
      MBytes_fs_free = MAX_MBYTES_PER_FILE; /* some fake value */
   }
   else
   {
      blocks_per_MB = (1024*1024) / statfs_buf.f_bsize;
      MBytes_fs_free = statfs_buf.f_bfree/blocks_per_MB;
   }
   settings->bytes_last_checked = settings->bytes_output_cur;
#endif

   return MBytes_fs_free;
}


/******************************************************
 * lavrec_close_files_on_error()
 *   Close the output file(s) if an error occured.
 *   We don't care about further errors.
 ******************************************************/
                         
static void lavrec_close_files_on_error(lavrec_t *info)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if(settings->output_status > 0 && settings->video_file)
   {
      lav_close(settings->video_file);
      settings->video_file = NULL;
   }
   if(settings->output_status > 1 && settings->video_file_old)
   {
      lav_close(settings->video_file_old);
      settings->video_file_old = NULL;
   }

   lavrec_msg(LAVREC_MSG_WARNING, info, "Closing file(s) and exiting - "
      "output file(s) my not be readable due to error");
}


/******************************************************
 * lavrec_output_video_frame()
 *   outputs a video frame and does all the file handling
 *   necessary like opening new files and closing old ones.
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

#define OUTPUT_VIDEO_ERROR_RETURN \
if (settings->output_status==2)   \
{                                 \
   settings->output_status = 3;   \
   return 1;                      \
}                                 \
else                              \
   return 0;

static int lavrec_output_video_frame(lavrec_t *info, char *buff, long size, long count)
{
   int n;
   int OpenNewFlag = 0;

   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if(settings->output_status == 3) return 1; /* Only audio is still active */

   /* Check space on filesystem if we have filled it up
    * or if we have written more than CHECK_INTERVAL bytes since last check
    */
   if (settings->output_status > 0)
   {
      n = (settings->bytes_output_cur - settings->bytes_last_checked)>>20; /* in MBytes */
      if( n > CHECK_INTERVAL || n > settings->MBytes_fs_free - MIN_MBYTES_FREE )
         settings->MBytes_fs_free = lavrec_get_free_space(settings);
   }

   /* Check if it is time to exit */
   if (settings->state != LAVREC_STATE_RECORDING) 
      lavrec_msg(LAVREC_MSG_INFO, info, "Signal caught, stopping recording");
   if (settings->stats->num_frames * settings->spvf > info->record_time && info->record_time >= 0)
   {
      lavrec_msg(LAVREC_MSG_INFO, info,
         "Recording time reached, stopping");
      lavrec_change_state(info, LAVREC_STATE_STOP);
   }

   /* Check if we have to open a new output file */
   if (settings->output_status > 0 && (settings->bytes_output_cur>>20) > MAX_MBYTES_PER_FILE)
   {
      lavrec_msg(LAVREC_MSG_INFO, info,
         "Max filesize reached, opening next output file");
      OpenNewFlag = 1;
   }
   if (settings->output_status > 0 && settings->MBytes_fs_free < MIN_MBYTES_FREE)
   {
      lavrec_msg(LAVREC_MSG_INFO, info,
         "File system is nearly full, trying to open next output file");
      OpenNewFlag = 1;
   }

   /* If a file is open and we should open a new one or exit, close current file */
   if (settings->output_status > 0 && (OpenNewFlag || settings->state != LAVREC_STATE_RECORDING))
   {
      if (info->audio_size)
      {
         /* Audio is running - flag that the old file should be closed */
         if(settings->output_status != 1)
         {
            /* There happened something bad - the old output file from the
             * last file change is not closed. We try to close all files and exit
             */
            lavrec_msg(LAVREC_MSG_ERROR, info,
               "Audio too far behind video. Check if audio works correctly!");
            lavrec_close_files_on_error(info);
            return -1;
         }
         lavrec_msg(LAVREC_MSG_DEBUG, info,
            "Closing current output file for video, waiting for audio to be filled");
         settings->video_file_old = settings->video_file;
         settings->video_file = NULL;
         settings->num_frames_old = settings->stats->num_frames;
         if (settings->state != LAVREC_STATE_RECORDING)
         {
            settings->output_status = 3;
            return 1;
         }
         else
            settings->output_status = 2;
      }
      else
      {
         if (lav_close(settings->video_file))
         {
            settings->video_file = NULL;
            lavrec_msg(LAVREC_MSG_ERROR, info,
               "Error closing video output file %s, may be unuseable due to error",
               settings->stats->output_filename);
            return 0;
         }
         settings->video_file = NULL;
         if (settings->state != LAVREC_STATE_RECORDING) return 0;
      }
   }

   /* Open new output file if needed */
   if (settings->output_status==0 || OpenNewFlag )
   {
      /* Get next filename */
      if (info->num_files == 0)
      {
         sprintf(settings->stats->output_filename, info->files[0],
            ++settings->stats->current_output_file);
      }
      else
      {
         if (settings->stats->current_output_file >= info->num_files)
         {
            lavrec_msg(LAVREC_MSG_WARNING, info,
               "Number of given output files reached");
            OUTPUT_VIDEO_ERROR_RETURN;
         }
         strncpy(settings->stats->output_filename,
            info->files[settings->stats->current_output_file++],
            sizeof(settings->stats->output_filename));
      }
      lavrec_msg(LAVREC_MSG_INFO, info,
         "Opening output file %s", settings->stats->output_filename);
         
      /* Open next file */
      settings->video_file = lav_open_output_file(settings->stats->output_filename, info->video_format,
         settings->width, settings->height, settings->interlaced,
         (info->video_norm==1? 30000.0/1001.0 : 25.0),
         info->audio_size, (info->stereo ? 2 : 1), info->audio_rate);
      if (!settings->video_file)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error opening output file %s: %s", settings->stats->output_filename, lav_strerror());
         OUTPUT_VIDEO_ERROR_RETURN;
      }

      if (settings->output_status == 0) settings->output_status = 1;

      /* Check space on filesystem. Exit if not enough space */
      settings->bytes_output_cur = 0;
      settings->MBytes_fs_free = lavrec_get_free_space(settings);
      if(settings->MBytes_fs_free < MIN_MBYTES_FREE_OPEN)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Not enough space for opening new output file");

         /* try to close and remove file, don't care about errors */
         lav_close(settings->video_file);
         settings->video_file = NULL;
         remove(settings->stats->output_filename);
         OUTPUT_VIDEO_ERROR_RETURN;
      }
   }

   /* Output the frame count times */
   if (lav_write_frame(settings->video_file,buff,size,count))
   {
      /* If an error happened, try to close output files and exit */
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error writing to output file %s: %s", settings->stats->output_filename, lav_strerror());
      lavrec_close_files_on_error(info);
      return 0;
   }

   /* Update counters. Maybe frame its written only once,
    * but size*count is the save guess
    */
   settings->bytes_output_cur += size*count; 
   settings->stats->num_frames += count;

   return 1;
}

static int video_captured(lavrec_t *info, char *buff, long size, long count)
{
   if (info->files)
      return lavrec_output_video_frame(info, buff, size, count);
   else
      info->video_captured(buff, size, count);
   return 1;
}


/******************************************************
 * lavrec_output_audio_to_file()
 *   writes audio data to a file
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_output_audio_to_file(lavrec_t *info, char *buff, long samps, int old)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if(samps==0) return 1;

   /* Output data */
   if (lav_write_audio(old?settings->video_file_old:settings->video_file,buff,samps))
   {
      /* If an error happened, try to close output files and exit */
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error writing to output file: %s", lav_strerror());
      lavrec_close_files_on_error(info);
      return 0;
   }

   /* update counters */
   settings->stats->num_asamps += samps;
   if (!old) settings->bytes_output_cur += samps * settings->audio_bps;

   return 1;
}


/******************************************************
 * lavrec_output_audio_samples()
 *   outputs audio samples to files
 *
 * return value: 1 on success, 0 or -1 on error
 ******************************************************/

static int lavrec_output_audio_samples(lavrec_t *info, char *buff, long samps)
{
   long diff = 0;

   video_capture_setup *settings = (video_capture_setup *)info->settings;

   /* Safety first */
   if(!settings->output_status)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "**INTERNAL ERROR: Output audio but no file open");
      return -1;
   }

   if(settings->output_status<2)
   {
      /* Normal mode, just output the sample */
      return lavrec_output_audio_to_file(info, buff, samps, 0);
   }

   /* if we come here, we have to fill up the old file first */
   diff = (settings->num_frames_old * settings->spvf -
      settings->stats->num_asamps * settings->spas) * info->audio_rate;
   
   if(diff<0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "**INTERNAL ERROR: Audio output ahead video output");
      return -1;
   }

   if(diff >= samps)
   {
      /* All goes to old file */
      return lavrec_output_audio_to_file(info, buff, samps, 1);
   }

   /* diff samples go to old file */
   if (!lavrec_output_audio_to_file(info, buff, diff, 1))
      return 0;

   /* close old file */
   lavrec_msg(LAVREC_MSG_DEBUG, info, "Audio is filled - closing old file");
   if (lav_close(settings->video_file_old))
   {
      settings->video_file_old = NULL;
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error closing video output file, may be unuseable due to error: %s",
         lav_strerror());
      return 0;
   }
   settings->video_file_old = NULL;

   /* Check if we are ready */
   if (settings->output_status==3) return 0;

   /* remaining samples go to new file */
   settings->output_status = 1;
   return lavrec_output_audio_to_file(info, buff+diff*settings->audio_bps, samps-diff, 0);
}

static int audio_captured(lavrec_t *info, char *buff, long samps)
{
   if (info->files)
      return lavrec_output_audio_samples(info, buff, samps);
   else
      info->audio_captured(buff, samps);
   return 1;
}


/******************************************************
 * lavrec_encoding_thread()
 *   The software encoding thread
 ******************************************************/
#if 0
static void *lavrec_encoding_thread(void* arg)
{
   lavrec_t *info = (lavrec_t *) info;
   video_capture_setup *settings = (video_capture_setup *)info->settings;
   int jpegsize;
   unsigned char *bwbuff;
   char *y_buff;
   int n;

   lavrec_msg(LAVREC_MSG_DEBUG, info,
      "Starting software encoding thread");

   /* Allow easy shutting down by other processes... */
   pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
   pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

   settings->currently_encoded_frame = 0;

   bwbuff = (unsigned char*)malloc(settings->width*settings->height/4);
   for (n=0;n<settings->width*settings->height/4;n++)
      bwbuff[n] = 128;
   y_buff = (char *) malloc(sizeof(char)*settings->width*settings->height);

   while (settings->state != LAVREC_STATE_STOP)
   {
      pthread_mutex_lock(&(settings->valid_mutex));
      while (settings->buffer_valid[settings->currently_encoded_frame] == 0)
      {
         lavrec_msg(LAVREC_MSG_DEBUG, info,
            "Encoding thread: sleeping for new frames (waiting for frame %d)", 
            settings->currently_encoded_frame);
         pthread_cond_wait(&(settings->buffer_filled[settings->currently_encoded_frame]),
            &(settings->valid_mutex));
         if (settings->state == LAVREC_STATE_STOP)
         {
            /* Ok, we shall exit, that's the reason for the wakeup */
            lavrec_msg(LAVREC_MSG_DEBUG, info,
               "Encoding thread: was told to exit");
            pthread_exit(NULL);
         }
      }
      pthread_mutex_unlock(&(settings->valid_mutex));

      /* TEST: move Y-pixels over */
      for (n=0;n<settings->width*settings->height;n++)
      {
         y_buff[n] = settings->YUV_buff[n*2];
      }

      /* encode frame to JPEG and write it out */
      jpegsize = encode_jpeg_raw((unsigned char*)(settings->MJPG_buff+
         (settings->breq.size*settings->currently_encoded_frame)),
         settings->breq.size, info->quality, settings->interlaced,
         CHROMA420, settings->width, settings->height,
         /*settings->YUV_buff+settings->softreq.offsets[settings->mm.frame]*/ (unsigned char*) y_buff,
         /*settings->YUV_buff+settings->softreq.offsets[settings->mm.frame]+
         settings->width*settings->height*/ bwbuff,
         /*settings->YUV_buff+settings->softreq.offsets[settings->mm.frame]+
         settings->width*settings->height*5/4*/ bwbuff);
      if (jpegsize<0)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error encoding frame to JPEG");
         lavrec_change_state(info, LAVREC_STATE_STOP);
      }

      if (!video_captured(info,
         settings->MJPG_buff+(settings->breq.size*settings->currently_encoded_frame), jpegsize,
         settings->buffer_valid[settings->currently_encoded_frame]))
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error writing the frame");
         lavrec_change_state(info, LAVREC_STATE_STOP);
      }

      pthread_mutex_lock(&(settings->valid_mutex));
      settings->buffer_valid[settings->currently_encoded_frame] = 0;
      pthread_mutex_unlock(&(settings->valid_mutex));

      settings->currently_encoded_frame = (settings->currently_encoded_frame + 1) % settings->breq.count;
   }

   pthread_exit(NULL);
}
#endif

/******************************************************
 * lavrec_software_init()
 *   Some software-MJPEG encoding specific initialization
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_software_init(lavrec_t *info)
{
   struct video_capability vc;
   //int i;

   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (ioctl(settings->video_fd, VIDIOCGCAP, &vc) < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error getting device capabilities: %s", (char *)sys_errlist[errno]);
      return 0;
   }
   /* vc.maxwidth is often reported wrong - let's just keep it broken (sigh) */
   if (vc.maxwidth != 768 && vc.maxwidth != 640) vc.maxwidth = 720;

   /* set some "subcapture" options - cropping is done later on (during capture) */
   if(!info->geometry->w)
      info->geometry->w = ((vc.maxwidth==720&&info->horizontal_decimation!=1)?704:vc.maxwidth);
   if(!info->geometry->h)
      info->geometry->h = info->video_norm==1 ? 480 : 576;

   if (info->geometry->w + info->geometry->x > vc.maxwidth)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Image width+offset (%d) bigger than maximum (%d)!",
         info->geometry->w + info->geometry->x, vc.maxwidth);
      return 0;
   }
   if ((info->geometry->w%(info->horizontal_decimation*16))!=0) 
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Image width (%d) not multiple of %d (required for JPEG encoding)!",
	 info->geometry->w, info->horizontal_decimation*16);
      return 0;
   }
   if (info->geometry->h + info->geometry->y > (info->video_norm==1 ? 480 : 576)) 
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Image height+offset (%d) bigger than maximum (%d)!",
         info->geometry->h + info->geometry->y, (info->video_norm==1 ? 480 : 576));
      return 0;
   }

   /* RJ: Image height must only be a multiple of 8, but geom_height
    * is double the field height
    */
   if ((info->geometry->h%(info->vertical_decimation*16))!=0) 
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Image height (%d) not multiple of %d (required for JPEG encoding)!",
         info->geometry->h, info->vertical_decimation*16);
      return 0;
   }

   settings->width = info->geometry->w / info->horizontal_decimation;
   settings->height = info->geometry->h / info->vertical_decimation;

   if (info->geometry->x == VALUE_NOT_FILLED)
      info->geometry->x = (vc.maxwidth - info->geometry->w)/2;
   if (info->geometry->y == VALUE_NOT_FILLED)
      info->geometry->y = ((info->video_norm==1?480:576)-info->geometry->h)/2;

   /* now, set the h/w/x/y to what they will *really* be */
   info->geometry->w /= info->horizontal_decimation;
   info->geometry->x /= info->horizontal_decimation;
   info->geometry->h /= info->vertical_decimation;
   info->geometry->y /= info->vertical_decimation;

   settings->interlaced = LAV_NOT_INTERLACED; /* VERY doubtable - just a guess for now (yikes!) */

   lavrec_msg(LAVREC_MSG_INFO, info,
      "Image size will be %dx%d, %d field(s) per buffer",
      info->geometry->w, info->geometry->h,
      (settings->interlaced==LAV_NOT_INTERLACED)?1:2);

   /* request buffer info */
   if (ioctl(settings->video_fd, VIDIOCGMBUF, &(settings->softreq)) < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error getting buffer information: %s", (char *)sys_errlist[errno]);
      return 0;
   }
   lavrec_msg(LAVREC_MSG_INFO, info,
      "Got %d YUV-buffers of size %d KB", settings->softreq.frames,
      settings->softreq.size/(1024*settings->softreq.frames));

   /* Map the buffers */
   settings->YUV_buff = mmap(0, settings->softreq.size, 
      PROT_READ, MAP_SHARED, settings->video_fd, 0);
   if (settings->YUV_buff == MAP_FAILED)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error mapping video buffers: %s", (char *)sys_errlist[errno]);
      return 0;
   }

   /* set up buffers for software encoding thread */
   if (info->MJPG_numbufs > MJPEG_MAX_BUF)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Too many buffers (%d) requested, maximum is %d",
         info->MJPG_numbufs, MJPEG_MAX_BUF);
      return 0;
   }
   settings->breq.count = info->MJPG_numbufs;
   settings->breq.size = info->MJPG_bufsize*1024;
   settings->MJPG_buff = (char *) malloc(sizeof(char)*settings->breq.size); // *settings->breq.count);
   if (!settings->MJPG_buff)
   {
      lavrec_msg (LAVREC_MSG_ERROR, info,
         "Malloc error, you\'re probably out of memory");
      return 0;
   }
   lavrec_msg(LAVREC_MSG_INFO, info,
      "Created %ld MJPEG-buffers of size %ld KB",
      settings->breq.count, settings->breq.size/1024);

   /* set up thread */
#if 0
   pthread_mutex_init(&(settings->valid_mutex), NULL);
   for (i=0;i<MJPEG_MAX_BUF;i++)
   {
      settings->buffer_valid[i] = 0;
      pthread_cond_init(&(settings->buffer_filled[i]), NULL);
   }
   if ( pthread_create( &(settings->encoding_thread), NULL,
      lavrec_encoding_thread, (void *) info ) )
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Failed to create software encoding thread");
      return 0;
   }
#endif

   return 1;
}


/******************************************************
 * lavrec_hardware_init()
 *   Some hardware-MJPEG encoding specific initialization
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_hardware_init(lavrec_t *info)
{
   struct video_capability vc;
   struct mjpeg_params bparm;

   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (ioctl(settings->video_fd, VIDIOCGCAP, &vc) < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error getting device capabilities: %s", (char *)sys_errlist[errno]);
      return 0;
   }
   /* vc.maxwidth is often reported wrong - let's just keep it broken (sigh) */
   if (vc.maxwidth != 768 && vc.maxwidth != 640) vc.maxwidth = 720;

   /* Query and set params for capture */
   if (ioctl(settings->video_fd, MJPIOC_G_PARAMS, &bparm) < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error getting video parameters: %s", (char *)sys_errlist[errno]);
      return 0;
   }
   bparm.input = info->video_src;
   bparm.norm = info->video_norm;
   bparm.quality = info->quality;

   /* Set decimation and image geometry params - only if we have weird options */
   if (info->geometry->x != VALUE_NOT_FILLED ||
      info->geometry->y != VALUE_NOT_FILLED ||
      (info->geometry->h != 0 && info->geometry->h != info->video_norm==1 ? 480 : 576) ||
      (info->geometry->w != 0 && info->geometry->w != vc.maxwidth) ||
      info->horizontal_decimation != info->vertical_decimation)
   {
      bparm.decimation = 0;
      if(!info->geometry->w) info->geometry->w = ((vc.maxwidth==720&&info->horizontal_decimation!=1)?704:vc.maxwidth);
      if(!info->geometry->h) info->geometry->h = info->video_norm==1 ? 480 : 576;
      bparm.HorDcm = info->horizontal_decimation;
      bparm.VerDcm = (info->vertical_decimation==4) ? 2 : 1;
      bparm.TmpDcm = (info->vertical_decimation==1) ? 1 : 2;
      bparm.field_per_buff = (info->vertical_decimation==1) ? 2 : 1;

      if (info->geometry->w + info->geometry->x > vc.maxwidth)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Image width+offset (%d) bigger than maximum (%d)!",
            info->geometry->w + info->geometry->x, vc.maxwidth);
         return 0;
      }
      if ((info->geometry->w%(bparm.HorDcm*16))!=0) 
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Image width (%d) not multiple of %d (required for JPEG)!",
            info->geometry->w, bparm.HorDcm*16);
         return 0;
      }
      if (info->geometry->h + info->geometry->y > (info->video_norm==1 ? 480 : 576)) 
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Image height+offset (%d) bigger than maximum (%d)!",
            info->geometry->h + info->geometry->y,
            (info->video_norm==1 ? 480 : 576));
         return 0;
      }

      /* RJ: Image height must only be a multiple of 8, but geom_height
       * is double the field height
       */
      if ((info->geometry->h%(bparm.VerDcm*16))!=0) 
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Image height (%d) not multiple of %d (required for JPEG)!",
            info->geometry->h, bparm.VerDcm*16);
         return 0;
      }

      bparm.img_width  = info->geometry->w;
      bparm.img_height = info->geometry->h/2;

      if (info->geometry->x != VALUE_NOT_FILLED)
         bparm.img_x = info->geometry->x;
      else
         bparm.img_x = (vc.maxwidth - bparm.img_width)/2;

      if (info->geometry->y != VALUE_NOT_FILLED)
         bparm.img_y = info->geometry->y/2;
      else
         bparm.img_y = ( (info->video_norm==1 ? 240 : 288) - bparm.img_height)/2;
   }
   else
   {
      bparm.decimation = info->horizontal_decimation;
   }

   /* Care about field polarity and APP Markers which are needed for AVI
    * and Quicktime and may be for other video formats as well
    */
   if(info->vertical_decimation > 1)
   {
      /* for vertical decimation > 1 no known video format needs app markers,
       * we need also not to care about field polarity
       */
      bparm.APP_len = 0; /* No markers */
   }
   else
   {
      int n;
      bparm.APPn = lav_query_APP_marker(info->video_format);
      bparm.APP_len = lav_query_APP_length(info->video_format);

      /* There seems to be some confusion about what is the even and odd field ... */
      /* madmac: 20010810: According to Ronald, this is wrong - changed now to EVEN */
      bparm.odd_even = lav_query_polarity(info->video_format) == LAV_INTER_EVEN_FIRST;
      for(n=0; n<bparm.APP_len && n<60; n++) bparm.APP_data[n] = 0;
   }

   if (ioctl(settings->video_fd, MJPIOC_S_PARAMS, &bparm) < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error setting video parameters: %s", (char *)sys_errlist[errno]);
      return 0;
   }

   settings->width = bparm.img_width/bparm.HorDcm;
   settings->height = bparm.img_height/bparm.VerDcm*bparm.field_per_buff;
   settings->interlaced = (bparm.field_per_buff>1);

   lavrec_msg(LAVREC_MSG_INFO, info,
      "Image size will be %dx%d, %d field(s) per buffer",
      settings->width, settings->height, bparm.field_per_buff);

   /* Request buffers */
   settings->breq.count = info->MJPG_numbufs;
   settings->breq.size = info->MJPG_bufsize*1024;
   if (ioctl(settings->video_fd, MJPIOC_REQBUFS,&(settings->breq)) < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error requesting video buffers: %s", (char *)sys_errlist[errno]);
      return 0;
   }
   lavrec_msg(LAVREC_MSG_INFO, info,
      "Got %ld buffers of size %ld KB", settings->breq.count, settings->breq.size/1024);

   /* Map the buffers */
   settings->MJPG_buff = mmap(0, settings->breq.count*settings->breq.size, 
      PROT_READ, MAP_SHARED, settings->video_fd, 0);
   if (settings->MJPG_buff == MAP_FAILED)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error mapping video buffers: %s", (char *)sys_errlist[errno]);
      return 0;
   }

   return 1;
}


/******************************************************
 * lavrec_init()
 *   initialize, open devices and start streaming
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_init(lavrec_t *info)
{
#ifndef IRIX
   struct video_channel vch;
#endif

   video_capture_setup *settings = (video_capture_setup *)info->settings;

   /* are there files to capture to? */
   if (info->files) /* yes */
   {
      if (info->video_captured || info->audio_captured)
      {
         lavrec_msg(LAVREC_MSG_DEBUG, info,
            "Custom audio-/video-capture functions are being ignored for file-capture");
      }
   }
   else /* no, so we need the custom actions */
   {
      if (!info->video_captured || (!info->audio_captured && info->audio_size))
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "No video files or custom video-/audio-capture functions given");
         return 0;
      }
   }

   /* Special settings for single frame captures */
   if(info->single_frame)
      info->MJPG_numbufs = 4;

   /* time lapse/single frame captures don't want audio */
   if((info->time_lapse > 1 || info->single_frame) && info->audio_size)
   {
      lavrec_msg(LAVREC_MSG_DEBUG, info,
         "Time lapse or single frame capture mode - audio disabled");
      info->audio_size = 0;
   }

   /* set the sound mixer */
   if (info->audio_size && info->audio_level >= 0)
      lavrec_set_mixer(info, 1);

   /* Initialize the audio system if audio is wanted.
    * This involves a fork of the audio task and is done before
    * the video device and the output file is opened
    */
   settings->audio_bps = 0;
   if (info->audio_size)
   {
      if (audio_init(1,info->use_read, info->stereo,info->audio_size,info->audio_rate))
      {
         lavrec_set_mixer(info, 0);
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error initializing Audio: %s",audio_strerror());
         return 0;
      }
      settings->audio_bps = info->audio_size / 8;
      if (info->stereo) settings->audio_bps *= 2;
      settings->audio_buffer_size = audio_get_buffer_size();
   }

   /* After we have fired up the audio system (which is assisted if we're
    * installed setuid root, we want to set the effective user id to the
    * real user id
    */
   if (seteuid(getuid()) < 0 )
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Can't set effective user-id: %s", (char *)sys_errlist[errno]);
      return 0;
   }

   /* open the video device */
   settings->video_fd = open(info->video_dev, O_RDONLY);
   if (settings->video_fd < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error opening video-device (%s): %s",
         info->video_dev, (char *)sys_errlist[errno]);
      return 0;
   }

   /* we might have to autodetect the video-src/norm */
   if (lavrec_autodetect_signal(info) == 0)
      return 0;

#ifndef IRIX 
   vch.channel = info->video_src;
   vch.norm = info->video_norm;
   if (ioctl(settings->video_fd, VIDIOCSCHAN, &vch) < 0)
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Error setting channel: %s", (char *)sys_errlist[errno]);
      return 0;
   }

   /* set channel if we're tuning */
   if (info->video_src == 2 && info->tuner_frequency)
   {
      unsigned long outfreq;
      outfreq = info->tuner_frequency*16/1000;
      if (ioctl(settings->video_fd, VIDIOCSFREQ, &outfreq) < 0)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error setting tuner frequency: %s", (char *)sys_errlist[errno]);
         return 0;
      }
   }

   /* Set up tuner audio if this is a tuner. I think this should be done
    * AFTER the tuner device is selected
    */
   if (info->video_src == 2) 
   {
      struct video_audio vau;

      /* get current */
      if (ioctl(settings->video_fd,VIDIOCGAUDIO, &vau) < 0)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error getting tuner audio params: %s", (char *)sys_errlist[errno]);
         return 0;
      }
      /* unmute so we get sound to record
       * this is done without checking current state because the
       * current mga driver doesn't report mute state accurately
       */
      lavrec_msg(LAVREC_MSG_INFO, info, "Unmuting tuner audio...");
      vau.flags &= (~VIDEO_AUDIO_MUTE);
      if (ioctl(settings->video_fd,VIDIOCSAUDIO, &vau) < 0)
      {
         lavrec_msg(LAVREC_MSG_INFO, info,
            "Error setting tuner audio params: %s", (char *)sys_errlist[errno]);
         return 0;
      }
   }

   /* set state to paused... ugly, but we need it for the software thread */
   settings->state = LAVREC_STATE_PAUSED;

   /* set up some hardware/software-specific stuff */
   if (info->software_encoding)
   {
      if (!lavrec_software_init(info)) return 0;
   }
   else
   {
      if (!lavrec_hardware_init(info)) return 0;
   }   
#else 
   fprintf(stderr, "FATAL: Can't make necessary videosettings in IRIX !\n");
#endif

   /* Try to get a reliable timestamp for Audio */
   if (info->audio_size && info->sync_correction > 1)
   {
      int n,res;

      lavrec_msg(LAVREC_MSG_INFO, info, "Getting audio ...");

      for(n=0;;n++)
      {
         if(n > NUM_AUDIO_TRIES)
         {
            lavrec_msg(LAVREC_MSG_ERROR, info,
               "Unable to get audio - exiting ....");
            return 0;
         }
         res = audio_read(settings->AUDIO_buff,AUDIO_BUFFER_SIZE,0,
            &(settings->audio_t0),&(settings->astat));
         if (res < 0)
         {
            lavrec_msg(LAVREC_MSG_ERROR, info,
               "Error reading audio: %s",audio_strerror());
            return 0;
         }
         if(res && settings->audio_t0.tv_sec ) break;
         usleep(20000);
      }
   }

   /* If we can increase process priority ... no need for R/T though...
    * This is mainly useful for running using "at" which otherwise drops the
    * priority which causes sporadic audio buffer over-runs
    */
   if( getpriority(PRIO_PROCESS, 0) > -5 )
      setpriority(PRIO_PROCESS, 0, -5 );

#ifndef IRIX
   /* Seconds per video frame: */
   settings->spvf = (info->video_norm==VIDEO_MODE_NTSC) ? 1001./30000. : 0.040;
   settings->sync_lim = settings->spvf*1.5;
#else
   fprintf(stderr, "FATAL: Can't even set video norm dependent frames/s in IRIX !\n");
#endif

   /* Seconds per audio sample: */
   if(info->audio_size)
      settings->spas = 1.0/info->audio_rate;
   else
      settings->spas = 0.;

   return 1;
}


/******************************************************
 * lavrec_wait_for_start()
 *   catch audio until we have to stop or record
 ******************************************************/

static void lavrec_wait_for_start(lavrec_t *info)
{
   int res;

   video_capture_setup *settings = (video_capture_setup *)info->settings;

   while(settings->state == LAVREC_STATE_PAUSED)
   {
      usleep(10000);

      /* Audio (if on) is allready running, empty buffer to avoid overflow */
      if (info->audio_size)
      {
         while( (res=audio_read(settings->AUDIO_buff,AUDIO_BUFFER_SIZE,
            0,&settings->audio_t0,&settings->astat)) >0 ) /*noop*/;
         if(res==0) continue;
         if(res<0)
         {
            lavrec_msg(LAVREC_MSG_ERROR, info,
               "Error reading audio: %s", audio_strerror());
            lavrec_change_state(info, LAVREC_STATE_STOP); /* stop */
            return;
         }
      }
   }
}


/******************************************************
 * lavrec_queue_buffer()
 *   queues a buffer (either MJPEG or YUV)
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_queue_buffer(lavrec_t *info, int *num)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (info->software_encoding)
   {
      settings->mm.frame = *num;
      if (ioctl(settings->video_fd, VIDIOCMCAPTURE, &(settings->mm)) < 0)
         return 0;
   }
   else
   {
      if (ioctl(settings->video_fd, MJPIOC_QBUF_CAPT, num) < 0)
         return 0;
   }

   return 1;
}


/******************************************************
 * lavrec_sync_buffer()
 *   sync on a buffer (either MJPIOC_SYNC or VIDIOCSYNC)
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavrec_sync_buffer(lavrec_t *info, struct mjpeg_sync *bsync)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (info->software_encoding)
   {
      bsync->frame = (settings->mm.frame+1)%settings->softreq.frames;
      bsync->seq++;
      if (ioctl(settings->video_fd, VIDIOCSYNC, &(bsync->frame)) < 0)
      {
         return 0;
      }
      gettimeofday( &(bsync->timestamp), NULL );
   }
   else
   {
      if (ioctl(settings->video_fd, MJPIOC_SYNC, bsync) < 0)
      {
         return 0;
      }
   }

   return 1;
}


/******************************************************
 * lavrec_record()
 *   record and process video and audio
 ******************************************************/

static void lavrec_record(lavrec_t *info)
{
   int x, y, write_frame, nerr, nfout, jpegsize=0;
   video_capture_stats stats;
   unsigned int first_lost;
   long audio_offset = 0;
   double time;
   struct timeval first_time;
   struct mjpeg_sync bsync;
   struct timeval audio_tmstmp;

   unsigned char *y_buff = NULL, *u_buff = NULL, *v_buff = NULL;
   unsigned char *YUV;

   video_capture_setup *settings = (video_capture_setup *)info->settings;
   settings->stats = &stats;

#ifndef IRIX
   /* Queue all buffers, this also starts streaming capture */
   if (info->software_encoding)
   {
      x = 1;
      if (ioctl(settings->video_fd,  VIDIOCCAPTURE, &x) < 0)
      {
         lavrec_msg(LAVREC_MSG_WARNING, info,
            "Error starting streaming capture: %s", (char *)sys_errlist[errno]);
         //lavrec_change_state(info, LAVREC_STATE_STOP);
      }
      settings->mm.width = settings->width;
      settings->mm.height = settings->height;
      settings->mm.format = VIDEO_PALETTE_YUV422; /* UYVY, only format supported by the zoran-driver */

      y_buff = (unsigned char *) malloc(sizeof(unsigned char)*info->geometry->w*info->geometry->h);
      u_buff = (unsigned char *) malloc(sizeof(unsigned char)*info->geometry->w*info->geometry->h/4);
      v_buff = (unsigned char *) malloc(sizeof(unsigned char)*info->geometry->w*info->geometry->h/4);

      if (!y_buff || !u_buff || !v_buff)
      {
         lavrec_msg (LAVREC_MSG_ERROR, info,
            "Malloc error, you\'re probably out of memory");
         lavrec_change_state(info, LAVREC_STATE_STOP);
      }
   }
   for (x=0;x<(info->software_encoding?settings->softreq.frames:settings->breq.count);x++)
   {
      if (!lavrec_queue_buffer(info, &x))
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error queuing buffers: %s", (char *)sys_errlist[errno]);
         lavrec_change_state(info, LAVREC_STATE_STOP);
         break;
      }
   }
#else
   fprintf(stderr, "Recording disabled in IRIX !\n");
   return;
#endif

   /* reset the counter(s) */
   nerr = 0;
   write_frame = 1;
   first_lost = 0;
   stats.stats_changed = 0;
   stats.num_syncs = 0;
   stats.num_lost = 0;
   stats.num_frames = 0;
   stats.num_asamps = 0;
   stats.num_ins = 0;
   stats.num_del = 0;
   stats.num_aerr = 0;
   stats.tdiff1 = 0.;
   stats.tdiff2 = 0.;
   gettimeofday( &(stats.prev_sync), NULL );

   /* The video capture loop */
   while (settings->state == LAVREC_STATE_RECORDING)
   {
#ifndef IRIX
      /* sync on a frame */
      if (!lavrec_sync_buffer(info, &bsync))
      {
         if (info->files)
            lavrec_close_files_on_error(info);
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error syncing on a buffer: %s", (char *)sys_errlist[errno]);
         nerr++;
      }
      stats.num_syncs++;

      gettimeofday( &(stats.cur_sync), NULL );
      if(stats.num_syncs==1)
      {
         first_time = bsync.timestamp;
         first_lost = bsync.seq;
         if(info->audio_size && info->sync_correction > 1)
         {
            /* Get time difference beetween audio and video in bytes */
            audio_offset  = ((first_time.tv_usec-settings->audio_t0.tv_usec)*1.e-6 +
               first_time.tv_sec-settings->audio_t0.tv_sec - settings->spvf)*info->audio_rate;
            audio_offset *= settings->audio_bps;   /* convert to bytes */
         }
         else
            audio_offset = 0;
      }

      time = bsync.timestamp.tv_sec - first_time.tv_sec
         + 1.e-6*(bsync.timestamp.tv_usec - first_time.tv_usec)
         + settings->spvf; /* for first frame */


      /* Should we write a frame? */
      if(info->single_frame)
      {
         if (settings->state == LAVREC_STATE_RECORDING)
            lavrec_change_state(info, LAVREC_STATE_PAUSED);
         write_frame = 1;
         nfout = 1; /* always output frame only once */

      }
      else if(info->time_lapse > 1)
      {

         write_frame = (stats.num_syncs % info->time_lapse) == 0;
         nfout = 1; /* always output frame only once */

      }
      else /* normal capture */
      {

         nfout = 1;
         x = bsync.seq - stats.num_syncs - first_lost + 1; /* total lost frames */
         if (info->sync_correction > 0) 
            nfout += x - stats.num_lost; /* lost since last sync */
         stats.stats_changed = (stats.num_lost != x);
         stats.num_lost = x;

         /* Check if we have to insert/delete frames to stay in sync */
         if (info->sync_correction > 1)
         {
            if( stats.tdiff1 - stats.tdiff2 < -settings->sync_lim)
            {
               nfout++;
               stats.num_ins++;
               stats.stats_changed = 1;
               stats.tdiff1 += settings->spvf;
            }
            if( stats.tdiff1 - stats.tdiff2 > settings->sync_lim)
            {
               nfout--;
               stats.num_del++;
               stats.stats_changed = 1;
               stats.tdiff1 -= settings->spvf;
            }
         }
      }

      /* write it out */
      if(write_frame && nfout > 0)
      {
         if (info->software_encoding)
         {
#if 0
            pthread_mutex_lock(&(settings->valid_mutex));
            settings->buffer_valid[settings->currently_encoded_frame] = nfout;
            pthread_cond_broadcast(&(settings->buffer_filled[settings->currently_encoded_frame]));
            pthread_mutex_unlock(&(settings->valid_mutex));
#endif
            /* move Y-pixels over, not a good method but good enough (seems to be UYVY???) */
            YUV = settings->YUV_buff + (settings->softreq.offsets[bsync.frame]);

            /* sit down for this - it's really easy except if you try to understand it :-) */
            for (x=0;x<settings->width;x++)
               if (x>=info->geometry->x && x<(info->geometry->x+info->geometry->w))
                  for (y=0;y<settings->height;y++)
                     if (y>=info->geometry->y && y<(info->geometry->y+info->geometry->h))
                        y_buff[(y-info->geometry->y)*info->geometry->w + (x-info->geometry->x)] =
                           YUV[(y*settings->width+x)*2 + 1];

            for (x=0;x<settings->width/2;x++)
               if (x>=info->geometry->x/2 && x<(info->geometry->x+info->geometry->w)/2)
                  for (y=0;y<settings->height/2;y++)
                     if (y>=(info->geometry->y/2) && y<((info->geometry->y+info->geometry->h)/2))
                     {
                        u_buff[(y-info->geometry->y/2)*info->geometry->w/2 + (x-info->geometry->x/2)] =
                           YUV[(y*settings->width+x)*4];
                        v_buff[(y-info->geometry->y/2)*info->geometry->w/2 + (x-info->geometry->x/2)] =
                           YUV[(y*settings->width+x)*4 + 2];
                     }

            jpegsize = encode_jpeg_raw(settings->MJPG_buff+bsync.frame*settings->breq.size,
               settings->breq.size, info->quality, settings->interlaced,
               CHROMA420, info->geometry->w, info->geometry->h,
               y_buff,u_buff, v_buff);
            if (jpegsize<0)
            {
               lavrec_msg(LAVREC_MSG_ERROR, info,
                  "Error encoding frame to JPEG");
               lavrec_change_state(info, LAVREC_STATE_STOP);
            }
         }

         if (video_captured(info, settings->MJPG_buff+bsync.frame*settings->breq.size,
            info->software_encoding?jpegsize:bsync.length, nfout) != 1)
            nerr++; /* Done or error occured */
      }

      /* Re-queue the buffer */
      if (!lavrec_queue_buffer(info, &(bsync.frame)))
      {
         if (info->files)
            lavrec_close_files_on_error(info);
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error re-queuing buffer: %s", (char *)sys_errlist[errno]);
         nerr++;
      }

      while (info->audio_size)
      {
         /* Only try to read a audio sample if video is ahead - else we might
          * get into difficulties when writing the last samples
          */
         if (settings->output_status < 3 && 
            stats.num_frames * settings->spvf <
            (stats.num_asamps + settings->audio_buffer_size /
            settings->audio_bps) * settings->spas)
            break;

         x = audio_read(settings->AUDIO_buff, sizeof(settings->AUDIO_buff),
            0, &audio_tmstmp, &(settings->astat));

         if (x == 0) break;
         if (x < 0)
         {
            lavrec_msg(LAVREC_MSG_ERROR, info,
               "Error reading audio: %s", audio_strerror());
            if (info->files)
               lavrec_close_files_on_error(info);
            nerr++;
            break;
         }

         if (!(settings->astat))
         {
            stats.num_aerr++;
            stats.stats_changed = 1;
         }

         /* Adjust for difference at start */
         if (audio_offset >= x)
         {
            audio_offset -= x;
            continue;
         }
         x -= audio_offset;

         /* Got an audio sample, write it out */
         if (audio_captured(info, settings->AUDIO_buff+audio_offset, x/settings->audio_bps) != 1)
            break; /* Done or error occured */
         audio_offset = 0;

         /* calculate time differences beetween audio and video
          * tdiff1 is the difference according to the number of frames/samples written
          * tdiff2 is the difference according to the timestamps
          * (only if audio timestamp is not zero)
          */
         if(audio_tmstmp.tv_sec)
         {
            stats.tdiff1 = stats.num_frames * settings->spvf - stats.num_asamps * settings->spas;
            stats.tdiff2 = (bsync.timestamp.tv_sec - audio_tmstmp.tv_sec)
               + (bsync.timestamp.tv_usec - audio_tmstmp.tv_usec) * 1.e-6;
         }
      }

      /* output_statistics */
      if (info->output_statistics) info->output_statistics(&stats);
      stats.stats_changed = 0;

      stats.prev_sync = stats.cur_sync;

      /* if (nerr++) we need to stop and quit */
      if (nerr) lavrec_change_state(info, LAVREC_STATE_STOP);
#else
      fprintf(stderr, "Nope, no recording in IRIX (yet) !\n");
      return;
#endif
   }
}


/******************************************************
 * lavrec_recording_cycle()
 *   the main cycle for recording video
 ******************************************************/

static void lavrec_recording_cycle(lavrec_t *info)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   while (1)
   {
      if (settings->state == LAVREC_STATE_PAUSED)
         lavrec_wait_for_start(info);
      else if (settings->state == LAVREC_STATE_RECORDING)
         lavrec_record(info);
      else
         break;
   }
}


/******************************************************
 * lavrec_capture_thread()
 *   the video/audio capture thread
 ******************************************************/

static void *lavrec_capture_thread(void *arg)
{
   int n;
   lavrec_t *info = (lavrec_t*)arg;
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   /* now, set state to pause and catch audio until started */
   lavrec_change_state(info, LAVREC_STATE_PAUSED);

   lavrec_recording_cycle(info);

   /* shutdown video/audio and close */
   if (info->audio_size)
      audio_shutdown();

   /* certainty for all :-) */
   if (settings->video_file)
   {
      lav_close(settings->video_file);
      settings->video_file = NULL;
   }
   if (settings->video_file_old)
   {
      lav_close(settings->video_file_old);
      settings->video_file_old = NULL;
   }

   /* reset mixer */
   if (info->audio_size)
      lavrec_set_mixer(info, 0);

#ifndef IRIX
   /* Re-mute tuner audio if this is a tuner */
   if (info->video_src == 2) {
      struct video_audio vau;
         
      lavrec_msg(LAVREC_MSG_INFO, info,
         "Re-muting tuner audio...");
      vau.flags |= VIDEO_AUDIO_MUTE;
      if (ioctl(settings->video_fd,VIDIOCSAUDIO,&vau) < 0)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error resetting tuner audio params: %s", (char *)sys_errlist[errno]);
      }
   }
#else
   fprintf(stderr, "Can't set tuner in IRIX !\n");
#endif

#ifndef IRIX
   /* stop streaming capture */
   if (info->software_encoding)
   {
      n = 0;
      if (ioctl(settings->video_fd,  VIDIOCCAPTURE, &n) < 0)
      {
         lavrec_msg(LAVREC_MSG_WARNING, info,
            "Error stopping streaming capture: %s", (char *)sys_errlist[errno]);
         //lavrec_change_state(info, LAVREC_STATE_STOP);
      }
   }
   else
   {
      n = -1;
      if (ioctl(settings->video_fd, MJPIOC_QBUF_CAPT, &n) < 0)
      {
         lavrec_msg(LAVREC_MSG_ERROR, info,
            "Error resetting buffer-queue: %s", (char *)sys_errlist[errno]);
      }
   }
#else
   fprintf(stderr, "Can't stop capturing in IRIX !\n");
#endif

   /* and at last, we need to get rid of the video device */
   close(settings->video_fd);

   /* just to be sure */
   if (settings->state != LAVREC_STATE_STOP)
      lavrec_change_state(info, LAVREC_STATE_STOP);

   pthread_exit(NULL);
}


/******************************************************
 * lavrec_malloc()
 *   malloc a pointer and set default options
 *
 * return value: a pointer to lavrec_t or NULL
 ******************************************************/

lavrec_t *lavrec_malloc(void)
{
   lavrec_t *info;

   info = (lavrec_t *)malloc(sizeof(lavrec_t));
   if (!info)
   {
      lavrec_msg (LAVREC_MSG_ERROR, NULL,
         "Malloc error, you\'re probably out of memory");
      return NULL;
   }

   /* let's set some default values now */
   info->video_format = 'a';
   info->video_norm = 3;
   info->video_src = 3;
   info->software_encoding = 0;
   info->horizontal_decimation = 4;
   info->vertical_decimation = 4;
   info->geometry = (rect *)malloc(sizeof(rect));
   if (!(info->geometry))
   {
      lavrec_msg (LAVREC_MSG_ERROR, NULL,
         "Malloc error, you\'re probably out of memory");
      return NULL;
   }
   info->geometry->x = VALUE_NOT_FILLED;
   info->geometry->y = VALUE_NOT_FILLED;
   info->geometry->w = 0;
   info->geometry->h = 0;
   info->quality = 50;
   info->record_time = -1;
   info->tuner_frequency = 0;
   info->video_dev = "/dev/video";

   info->audio_size = 16;
   info->audio_rate = 44100;
   info->stereo = 0;
   info->audio_level = -1;
   info->mute = 0;
   info->audio_src = 'l';
   info->use_read = 0;
   info->audio_dev = "/dev/dsp";
   info->mixer_dev = "/dev/mixer";

   info->single_frame = 0;
   info->time_lapse = 1;
   info->sync_correction = 2;
   info->MJPG_numbufs = 64;
   info->MJPG_bufsize = 256;

   info->files = NULL;
   info->num_files = 0;

   info->output_statistics = NULL;
   info->audio_captured = NULL;
   info->video_captured = NULL;
   info->msg_callback = NULL;
   info->state_changed = NULL;

   info->settings = (void *)malloc(sizeof(video_capture_setup));
   if (!(info->settings))
   {
      lavrec_msg (LAVREC_MSG_ERROR, NULL,
         "Malloc error, you\'re probably out of memory");
      return NULL;
   }

   ((video_capture_setup*)(info->settings))->state = LAVREC_STATE_STOP;
   ((video_capture_setup*)(info->settings))->output_status = 0;
   ((video_capture_setup*)(info->settings))->video_file = NULL;
   ((video_capture_setup*)(info->settings))->video_file_old = NULL;
   
   return info;
}


/******************************************************
 * lavrec_main()
 *   the whole video-capture cycle
 *
 * Basic setup:
 *   * this function initializes the devices,
 *       sets up the whole thing and then forks
 *       the main task and returns control to the
 *       main app. It can then start recording by
 *       calling lavrec_start():
 *
 *   1) setup/initialize/open devices (state: STOP)
 *   2) wait for lavrec_start() (state: PAUSE)
 *   3) record (state: RECORD)
 *   4) stop/deinitialize/close (state: STOP)
 *
 *   * it should be possible to switch from RECORD
 *       to PAUSE and the other way around. When
 *       STOP, we stop and close the devices, so
 *       then you need to re-call this function.
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavrec_main(lavrec_t *info)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   /* Flush the Linux File buffers to disk */
   sync();

   /* start with initing */
   if (!lavrec_init(info))
      return 0;

   /* fork ourselves to return control to the main app */
   if( pthread_create( &(settings->capture_thread), NULL,
      lavrec_capture_thread, (void*)info) )
   {
      lavrec_msg(LAVREC_MSG_ERROR, info,
         "Failed to create thread");
      return 0;
   }

   return 1;
}


/******************************************************
 * lavrec_start()
 *   start recording (only call when ready!)
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavrec_start(lavrec_t *info)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (settings->state != LAVREC_STATE_PAUSED)
   {
      lavrec_msg(LAVREC_MSG_WARNING, info,
         "Not ready for capture (state = %d)!", settings->state);
      return 0;
   }

   lavrec_change_state(info, LAVREC_STATE_RECORDING);
   return 1;
}


/******************************************************
 * lavrec_pause()
 *   pause recording (you can call play to continue)
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavrec_pause(lavrec_t *info)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (settings->state != LAVREC_STATE_RECORDING)
   {
      lavrec_msg(LAVREC_MSG_WARNING, info,
         "Not recording!");
      return 0;
   }

   lavrec_change_state(info, LAVREC_STATE_PAUSED);
   return 1;
}


/******************************************************
 * lavrec_stop()
 *   stop recording
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavrec_stop(lavrec_t *info)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (settings->state == LAVREC_STATE_STOP)
   {
      lavrec_msg(LAVREC_MSG_WARNING, info,
         "We weren't even initialized!");
      return 0;
   }

   lavrec_change_state(info, LAVREC_STATE_STOP);

   //pthread_cancel( settings->capture_thread );
   pthread_join( settings->capture_thread, NULL );

   return 1;
}


/******************************************************
 * lavrec_free()
 *   free() the struct
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavrec_free(lavrec_t *info)
{
   video_capture_setup *settings = (video_capture_setup *)info->settings;

   if (settings->state != LAVREC_STATE_STOP)
   {
      lavrec_msg(LAVREC_MSG_WARNING, info,
         "We're not stopped yet, use lavrec_stop() first!");
      return 0;
   }

   free(settings);
   free(info->geometry);
   free(info);
   return 1;
}


/******************************************************
 * lavrec_busy()
 *   Wait until capturing is finished
 ******************************************************/

void lavrec_busy(lavrec_t *info)
{
   pthread_join( ((video_capture_setup*)(info->settings))->capture_thread, NULL );
}
