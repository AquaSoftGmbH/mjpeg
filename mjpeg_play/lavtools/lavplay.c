/*
    lavplay - Linux Audio Video PLAYback
      
    Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>


    Play MJPEG AVI-Files or Quicktime files with the Iomega BUZ.


    Usage: lavplay [options] filename [filename ...]

    where options are as follows:

    -h num     Horizontal offset
    -v num     Vertical offset
               You may use that only for quarter resolution videos
               unless you remove the following 4 lines of code in the BUZ driver:

               if(params->img_x     <0) err0++;
               if(params->img_y     <0) err0++;

               and

               if(params->img_x + params->img_width  > 720) err0++;
               if(params->img_y + params->img_height > tvnorms[params->norm].Ha/2) err0++;

               These parameters are usefull for videos captured from other sources
               which do not appear centered when played with the BUZ

    -a [01]    audio playback (defaults to on)

    -s num     skip num seconds before playing

    -c [01]    Sync correction off/on (default on)

    -n num     # of MJPEG buffers
               normally no need to change the default

    -q         Do NOT quit at end of video

    -x         Exchange fields of an interlaced video

    -z         "Zoom to fit flag"

               If this flag is not set, the video is always displayed in origininal
               resolution for interlaced input and in double height and width
               for not interlaced input. The aspect ratio of the input file is maintained.

               If this flag is set, the program tries to zoom the video in order
               to fill the screen as much as possible. The aspect ratio is NOT
               maintained, ie. the zoom factors for horizontal and vertical directions
               may be different. This might not always yield the results you want!



    Following the options, you may give a optional +p/+n parameter (for forcing
    the use of PAL or NTSC) and the several AVI files, Quicktime files
    or Edit Lists arbitrarily mixed (as long as all files have the same
    paramters like width, height and so on).

    Forcing a norm does not convert the video from NTSC to PAL or vice versa.
    It is only intended to be used for foreign AVI/Quicktime files whose
    norm can not be determined from the framerate encoded in the file.
    So use with care!

    The format of an edit list file is as follows:

    line 1: "LAV Edit List"
    line 2: "PAL" or "NTSC"
    line 3: Number of AVI/Quicktime files comprising the edit sequence
    next lines: Filenames of AVI/Quicktime files
    and then for every sequence
    a line consisting of 3 numbers: file-number start-frame end-frame

    If you are a real command line hardliner, you may try to entering the following
    commands during playing (normally used by xlav, just type the command and press
    enter):

    p<num>    Set playback speed, num may also be 0 (=pause) and negative (=reverse)
    s<num>    Skip to frame <num>
    s+<num>   <num> frames forward
    s-<num>   <num> frames backward
    +         1 frame forward (makes only sense when paused)
    -         1 frame backward (makes only sense when paused)
    q         quit



   *** Environment variables ***

   LAV_VIDEO_DEV     Name of video device (default: "/dev/video")
   LAV_AUDIO_DEV     Name of audio device (default: "/dev/dsp")


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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
//#include <sys/resource.h>
#include <SDL/SDL.h>
#include <mjpeg.h>

#include "lav_io.h"
#include "editlist.h"

char *audio_strerror();

int  verbose = 2;

static EditList el;

/* These are explicit prototypes for the compiler, to prepare separation of audiolib.c */
void audio_shutdown();
int audio_init(int a_read, int a_stereo, int a_size, int a_rate);
long audio_get_buffer_size();
int audio_write(char *buf, int size, int swap);
void audio_get_output_status(struct timeval *tmstmp, long *nb_out, long *nb_err);
int audio_start();

static int  h_offset = 0;
static int  v_offset = 0;
static int  quit_at_end = 1;
static int  gui_mode = 0;
static int  exchange_fields  = 0;
static int  zoom_to_fit = 0;
static int  MJPG_nbufs = 8;          /* Number of MJPEG buffers */
static int  skip_seconds = 0;
static double test_factor = 1.0; /* Internal test of synchronizaion only */
static int  audio_enable = 1;

/* gz: This makes lavplay play back in software */
int soft_play = 0;
int soft_fullscreen = 0;

/* gz: This is the new handle for MJPEG library */
struct mjpeg_handle *mjpeg;

/* gz: This forces lavplay to output on the screen (in hardware !) */
int screen_output = 0;

/* The following variable enables flicker reduction by doubling/reversing
   the fields. If this leads to problems during play (because of
   increased CPU overhead), just set it to 0 */

static int flicker_reduction = 1;

static int nvcorr = 0; /* Number of frames added/skipped for sync */

static int numca = 0; /* Number of corrections because video ahead audio */
static int numcb = 0; /* Number of corrections because video behind audio */

static char infostring[4096];

static long nframe;

static int play_speed=1;
static int audio_mute=0;

static unsigned long *save_list=0;
static long save_list_len = 0;

static char abuff[16384];

static char *tmpbuff[2]; /* buffers for flicker reduction */
static long old_field_len = 0;
static int  old_buff_no = 0;

static double spvf;   /* seconds per video frame */
static double spas;   /* seconds per audio sample */

/* Audio/Video sync correction params */

int sync_corr = 1;           /* Switch for sync correction */

/* The following two params control the way how synchronization is achieved:

   If video is behind audio and "sync_skip_frames", video frames are
   skipped, else audio samples are inserted.

   If video is ahead audio and "sync_ins_frames" is set, video frames
   are inserted (by duplication), else audio samples are skipped.
*/

int sync_skip_frames = 1;
int sync_ins_frames  = 1;

SDL_Surface *screen;
SDL_Rect jpegdims;

void audio_shutdown();

#define LAVPLAY_INTERNAL 0
#define LAVPLAY_DEBUG    1
#define LAVPLAY_INFO     2
#define LAVPLAY_WARNING  3
#define LAVPLAY_ERROR    4


static void lavplay_msg(int type, char *str1, char *str2)
{
   char *ctype;

   switch(type)
   {
       case LAVPLAY_INTERNAL: ctype = "Internal Error"; break;
       case LAVPLAY_DEBUG:    ctype = "Debug";          break;
       case LAVPLAY_INFO:     ctype = "Info";           break;
       case LAVPLAY_WARNING:  ctype = "Warning";        break;
       case LAVPLAY_ERROR:    ctype = "Error";          break;
       default:               ctype = "Unkown";
   }
   printf("%s: %s\n",ctype,str1);
   if(str2[0]) printf("%s: %s\n",ctype,str2);
   fflush(stdout);
}

/* system_error: report an error from a system call */

static void system_error(char *str1,char *str2)
{
   sprintf(infostring,"Error %s (in %s)",str1,str2);
   lavplay_msg(LAVPLAY_ERROR,infostring,strerror(errno));
   exit(1);
}

/* Since we use malloc often, here the error handling */

void malloc_error()
{
   lavplay_msg(LAVPLAY_ERROR,"Out of memory - malloc failed","");
   exit(1);
}

void sig_cont(int sig)
{
   int res;

   res = fcntl(0,F_SETFL,O_NONBLOCK);
   if (res<0) system_error("making stdin nonblocking","fcntl F_SETFL");
}


static int inc_frames(long num)
{
   nframe += num;

   if(nframe<0)
   {
      nframe = 0;
      play_speed = 0;
      return 0;
   }
   if(nframe>=el.video_frames)
   {
      nframe = el.video_frames-1;
      play_speed = 0;
      return 1;
   }
   return 0;
}

int queue_next_frame(char *vbuff, int skip_video, int skip_audio, int skip_incr)
{
   int res, mute, i, jpeg_len1, jpeg_len2, new_buff_no;
   char hlp[16];

   /* Read next frame */

   if(!skip_video)
   {
      if(flicker_reduction && el.video_inter && play_speed <= 0)
      {
         if(play_speed == 0)
         {
            res = el_get_video_frame(vbuff, nframe, &el);
            if(res<0) return -1;
            jpeg_len1 = lav_get_field_size(vbuff, res);
            memcpy(vbuff+jpeg_len1, vbuff, jpeg_len1);
            old_field_len = 0;
         }
         else /* play_speed < 0, play old first field + actual second field */
         {
            new_buff_no = 1 - old_buff_no;
            res = el_get_video_frame(tmpbuff[new_buff_no], nframe, &el);
            if(res<0) return -1;
            jpeg_len1 = lav_get_field_size(tmpbuff[new_buff_no], res);
            jpeg_len2 = res - jpeg_len1;
            if(old_field_len==0)
            {
               /* no old first field, duplicate second field */
               memcpy(vbuff,tmpbuff[new_buff_no]+jpeg_len1,jpeg_len2);
               old_field_len = jpeg_len2;
            }
            else
            {
               /* copy old first field into vbuff */
               memcpy(vbuff,tmpbuff[old_buff_no],old_field_len);
            }
            /* copy second field */
            memcpy(vbuff+old_field_len,tmpbuff[new_buff_no]+jpeg_len1,jpeg_len2);
            /* save first field */
            old_field_len = jpeg_len1;
            old_buff_no   = new_buff_no;
         }
      }
      else
      {
         res = el_get_video_frame(vbuff, nframe, &el);
         if(res<0) return -1;
         old_field_len = 0;
      }

   }

   /* Read audio, if present */

   if(el.has_audio && !skip_audio && audio_enable)
   {
      mute = play_speed==0 ||
             (audio_mute && play_speed!=1 && play_speed!=-1);
      res = el_get_audio_data(abuff, nframe, &el, mute);

      if(play_speed<0)
      {
         /* reverse audio */
         for(i=0;i<res/2;i+=el.audio_bps)
         {
            memcpy(hlp,abuff+i,el.audio_bps);
            memcpy(abuff+i,abuff+res-i-el.audio_bps,el.audio_bps);
            memcpy(abuff+res-i-el.audio_bps,hlp,el.audio_bps);
         }
      }
      res = audio_write(abuff,res,0);
      if(res<0)
      {
         lavplay_msg(LAVPLAY_ERROR,"Error playing audio",audio_strerror());
         return -1;
      }
   }

   /* Increment frames */

   if(!skip_incr)
   {
      res = inc_frames(play_speed);
      if(quit_at_end) return res;
   }

   return 0;
}

void Usage(char *progname)
{
   fprintf(stderr, "Usage: %s [options] <filename> [<filename> ...]\n", progname);
   fprintf(stderr, "where options are:\n");
   fprintf(stderr, "   -o [np]    Use norm NTSC or PAL (default: guess it from framerate\n");
   fprintf(stderr, "   -h num     Horizontal offset\n");
   fprintf(stderr, "   -v num     Vertical offset\n");
   fprintf(stderr, "   -s num     skip num seconds before playing\n");
   fprintf(stderr, "   -c [01]    Sync correction off/on (default on)\n");
   fprintf(stderr, "   -n num     # of MJPEG buffers\n");
   fprintf(stderr, "   -q         Do NOT quit at end of video\n");
   fprintf(stderr, "   -x         Exchange fields of an interlaced video\n");
   fprintf(stderr, "   -z         Zoom video to fill screen as much as possible\n");
   fprintf(stderr, "   -S         Use software MJPEG playback (based on SDL and libmjpeg)\n");
   fprintf(stderr, "   -Z         Switch to fullscreen\n");
   fprintf(stderr, "   -H         Use hardware MJPEG on-screen playback\n");
   fprintf(stderr, "   -a [01]    Eanble audio playback\n");
   exit(1);
}

void lock_screen()
{
       /* lock the screen for current decompression */
       if ( SDL_MUSTLOCK(screen) ) 
	 {
	   if ( SDL_LockSurface(screen) < 0 )
	     lavplay_msg(LAVPLAY_ERROR,"Error locking output screen", SDL_GetError());
	 }
}

void unlock_update_screen()
{
  /* unlock it again */
  if ( SDL_MUSTLOCK(screen) ) 
    {
      SDL_UnlockSurface(screen);
    }

  // printf("Updating rectangle with w=%d, h=%d", jpegdims.w, jpegdims.h);
  SDL_UpdateRect(screen, 0, 0, jpegdims.w, jpegdims.h);
}

void x_shutdown(int a)
{
  printf("Ctrl-C shutdown (mjpeg = %p) !\n", mjpeg);
  if (el.has_audio && audio_enable) audio_shutdown();
  mjpeg_close(mjpeg);
  if (soft_play) SDL_Quit();
}

#define stringify( str ) #str

int main(int argc, char ** argv)
{
   int res, frame, hn;
   long nqueue, nsync, first_free;
   struct timeval audio_tmstmp;
   struct timeval time_now;
   long nb_out, nb_err;
   long audio_buffer_size;
   unsigned char * buff;
   int nerr, n, i, k, skipv, skipa, skipi, nskip;
   double tdiff, tdiff1, tdiff2;
   char input_buffer[256];
   long frame_number[256]; /* Must be at least as big as the number of buffers used */

   char *sbuffer;
   struct mjpeg_params bp;
   struct mjpeg_sync bs;
   struct sigaction action, old_action;
   
   /* Output Version information */

   printf("lavplay" stringify(VERSION) ".0\n");
   fflush(stdout);

   if(argc < 2) Usage(argv[0]);

   nerr = 0;
   while( (n=getopt(argc,argv,"a:h:v:s:c:n:t:qSZHxzg")) != EOF)
   {
      switch(n) {
	 case 'a':
	    audio_enable = atoi(optarg);
	    break;

         case 'h':
            h_offset = atoi(optarg);
            break;

         case 'v':
            v_offset = atoi(optarg);
            break;

         case 's':
            skip_seconds = atoi(optarg);
            if(skip_seconds<0) skip_seconds = 0;
            break;

         case 'c':
            sync_corr = atoi(optarg);
            break;

         case 'n':
            MJPG_nbufs = atoi(optarg);
            if(MJPG_nbufs<4) MJPG_nbufs = 4;
            if(MJPG_nbufs>256) MJPG_nbufs = 256;
            break;

         case 't':
            test_factor = atof(optarg);
            if(test_factor<=0) test_factor = 1.0;
            break;

         case 'q':
            quit_at_end = 0;
            break;

         case 'x':
            exchange_fields = 1;
            break;

         case 'z':
            zoom_to_fit = 1;
            break;

         case 'g':
            gui_mode = 1;
            break;

         case '?':
            nerr++;
            break;

         case 'S':
	    printf("Choosing software MJPEG playback\n");
            soft_play = 1;
            break;

         case 'Z':
            soft_fullscreen = 1;
            break;

         case 'H':
	    printf("Choosing hardware MJPEG playback (on-screen)\n");
            screen_output = 1;
            break;
      }
   }

   if(optind>=argc) nerr++;

   if(nerr) Usage(argv[0]);

   /* Get and open input files */

   read_video_files(argv + optind, argc - optind, &el);

   /* Seconds per video frame: */

   spvf = (el.video_norm == 'n') ? 1001./30000. : 0.040;

   /* Seconds per audio sample: */

   if(el.has_audio && audio_enable)
      spas = 1.0/el.audio_rate;
   else
      spas = 0.;

   /* Seek starting position */

   nframe = 0;
   n = skip_seconds/spvf;
   res = inc_frames(n);
   if(res)
   {
      lavplay_msg(LAVPLAY_ERROR,"Start position is behind all files","");
      exit(1);
   }

   /* allocate auxiliary video buffer for flicker reduction */

   tmpbuff[0] = (char*) malloc(el.max_frame_size);
   tmpbuff[1] = (char*) malloc(el.max_frame_size);
   if(tmpbuff[0]==0 || tmpbuff[1]==0) malloc_error();

   if (soft_play)
     mjpeg = mjpeg_open(MJPEG_OUTPUT_SOFTWARE, MJPG_nbufs, el.max_frame_size);
   else 
     if (screen_output)
       mjpeg = mjpeg_open(MJPEG_OUTPUT_HARDWARE_SCREEN, MJPG_nbufs, el.max_frame_size);
     else
       mjpeg = mjpeg_open(MJPEG_OUTPUT_HARDWARE_VIDEO, MJPG_nbufs, el.max_frame_size);

   buff = mjpeg_get_io_buffer(mjpeg);

   if (soft_play)
     {
       char wintitle[255];
	   printf( "Initialising SDL\n" );
	   if (SDL_Init (SDL_INIT_VIDEO) < 0)
		 {
		   fprintf( stderr, "SDL Failed to initialise...\n" );
		   exit(1);
		 }
#ifdef ODFOFOFFO
	   int video_flags;

	   video_flags = SDL_SWSURFACE;
	   printf( "SETTING SDL VIDEO MODE %d %d %d\n", el.video_width, el.video_height, soft_fullscreen);

		screen = SDL_SetVideoMode( 640, 480, 0, video_flags);
	   printf( "GOT PAST SDL VIDEO MODE SET\n");
	   exit(0);
#endif
       /* Now initialize SDL */
       /* Set the video mode, and rely on SDL to find a nice mode */
       if (soft_fullscreen)
		 screen = SDL_SetVideoMode(el.video_width, 
				   el.video_height, 0, SDL_HWSURFACE | SDL_FULLSCREEN);
       else
		 screen = SDL_SetVideoMode(el.video_width, 
								   el.video_height, 0, SDL_SWSURFACE);

       SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
       SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
       
       if (screen->format->BytesPerPixel == 2)
	 mjpeg_calc_rgb16_params(screen->format->Rloss, screen->format->Gloss, screen->format->Bloss,
			   screen->format->Rshift, screen->format->Gshift, screen->format->Bshift);
       
       if ( screen == NULL )  
	 {
	   lavplay_msg(LAVPLAY_ERROR,"SDL: Output screen error", SDL_GetError());
	   exit(1);
	 }

       jpegdims.x = 0; // This is not going to work with interlaced pics !!
       jpegdims.y = 0;
       jpegdims.w = el.video_width;
       jpegdims.h = el.video_height;
       
       /* Lock the screen to test, and to be able to access screen->pixels */
       lock_screen();
       
       /* Draw bands of color on the raw surface, as run indicator for debugging */
       sbuffer = (char *)screen->pixels;
       for ( i=0; i < screen->h; ++i ) 
	 {
	   memset(sbuffer,(i*255)/screen->h,
		  screen->w * screen->format->BytesPerPixel);
	   sbuffer += screen->pitch;
	 }

       /* Set up the necessary callbacks for the decompression process */
       mjpeg->lock_screen_callback = lock_screen;
       mjpeg->unlock_update_screen_callback = unlock_update_screen;

       /* The output framebuffer parameters (where the JPEG frames are rendered into) */
       mjpeg_set_framebuf(mjpeg, screen->pixels, screen->w, screen->h, screen->format->BytesPerPixel); 

       /* Set the windows title (currently simply the first file name) */
       sprintf(wintitle, "lavplay %s", el.video_file_list[0]);
       SDL_WM_SetCaption(wintitle, "0000000");  

       unlock_update_screen();
     }
   if (el.has_audio && audio_enable)
   {
      res = audio_init(0,(el.audio_chans>1),el.audio_bits,
                       (int)(el.audio_rate*test_factor));
      if(res)
      {
         lavplay_msg(LAVPLAY_ERROR,"Error initializing Audio",audio_strerror());
         exit(1);
      }

      audio_buffer_size = audio_get_buffer_size();

      /* Ensure proper exit processing for audio */
      atexit(audio_shutdown);
   }

  signal(SIGINT, x_shutdown);

   /* Fill all buffers first */

   for(nqueue = 0; nqueue < mjpeg->br.count; nqueue++)
   {
      frame_number[nqueue] = nframe;
      if(queue_next_frame(buff+nqueue* mjpeg->br.size,0,0,0)) break;
   }

   /* Choose the correct parameters for playback */

   mjpeg_get_params(mjpeg, &bp);
   
   bp.input = 0;

   /* Set norm */       
   bp.norm = (el.video_norm == 'n') ? 1 : 0;
   sprintf(infostring,"Output norm: %s",bp.norm?"NTSC":"PAL");
   lavplay_msg(LAVPLAY_INFO,infostring,"");
   
   hn = bp.norm ? 480 : 576;  /* Height of norm */
   
   bp.decimation = 0; /* we will set proper params ourselves */
   
   /* Check dimensions of video, select decimation factors */
   if (!soft_play && !screen_output)
     if( el.video_width > 720 || el.video_height > hn )
       {
	 /* This is definitely too large */
	 
	 sprintf(infostring,"Video dimensions too large: %ld x %ld\n",
		 el.video_width,el.video_height);
	 lavplay_msg(LAVPLAY_ERROR,infostring,"");
	 exit(1);
       };
       
   /* if zoom_to_fit is set, HorDcm is independent of interlacing */
       
   if(zoom_to_fit)
     {
       if ( el.video_width < 180 )
	 bp.HorDcm = 4;
       else if ( el.video_width < 360 )
	 bp.HorDcm = 2;
       else
	 bp.HorDcm = 1;
     }
   
   if(el.video_inter)
     {
       /* Interlaced video, 2 fields per buffer */
       
       bp.field_per_buff = 2;
       bp.TmpDcm = 1;
	   
       if(zoom_to_fit)
	 {
	   if( el.video_height <= hn/2 )
	     bp.VerDcm = 2;
	   else
	     bp.VerDcm = 1;
	 }
       else
	 {
	   /* if zoom_to_fit is not set, we always use decimation 1 */
	   bp.HorDcm = 1;
	   bp.VerDcm = 1;
	 }
     }
   else
     {
       /* Not interlaced, 1 field per buffer */
       
       bp.field_per_buff = 1;
       bp.TmpDcm = 2;
       
       if (!soft_play && !screen_output &&  
	   ( el.video_height > hn/2 || (!zoom_to_fit && el.video_width>360) ))
	 {
	   sprintf(infostring,"Video dimensions (not interlaced) too large: %ld x %ld\n",
		   el.video_width,el.video_height);
	   lavplay_msg(LAVPLAY_ERROR,infostring,"");
	   if(el.video_width>360) lavplay_msg(LAVPLAY_INFO,"Try -z option !!!!","");
	   exit(1);
	 }
       
       if(zoom_to_fit)
	 {
	   if( el.video_height <= hn/4 )
		 bp.VerDcm = 2;
	   else
	     bp.VerDcm = 1;
	 }
       else
	 {
	   /* the following is equivalent to decimation 2 in lavrec: */
	   bp.HorDcm = 2;
	   bp.VerDcm = 1;
	 }
     }
   
   /* calculate height, width and offsets from the above settings */
   
   bp.quality = 100;
   bp.img_width  = bp.HorDcm*el.video_width;
   bp.img_height = bp.VerDcm*el.video_height/bp.field_per_buff;
   bp.img_x = (720  - bp.img_width )/2 + h_offset;
   bp.img_y = (hn/2 - bp.img_height)/2 + v_offset/2;
   
   sprintf(infostring,"Output dimensions: %dx%d+%d+%d",
	   bp.img_width,bp.img_height*2,bp.img_x,bp.img_y*2);
   lavplay_msg(LAVPLAY_INFO,infostring,"");
   sprintf(infostring,"Output zoom factors: %d (hor) %d (ver)\n",
	   bp.HorDcm,bp.VerDcm*bp.TmpDcm);
   lavplay_msg(LAVPLAY_INFO,infostring,"");
   
   /* Set field polarity for interlaced video */
   
   bp.odd_even = (el.video_inter==LAV_INTER_EVEN_FIRST);
   if(exchange_fields) bp.odd_even = !bp.odd_even;
   
   mjpeg_set_params(mjpeg, &bp);

  /* Make stdin nonblocking */
  memset(&action, 0, sizeof(action));

  action.sa_handler = sig_cont;
  action.sa_flags = SA_RESTART;


   sigaction(SIGCONT, &action, &old_action);

   sig_cont(0);

   /* Queue the buffers read, this triggers video playback */

   if (el.has_audio && audio_enable) audio_start();


   for(n = 0; n < nqueue; n++)
   {
     mjpeg_queue_buf(mjpeg, n);
   }

   /* Playback loop */

   nsync  = 0;
   tdiff1 = 0.0;
   tdiff2 = 0.0;
   tdiff  = 0.0;

   while(1)
   {
      /* Sync to get a free buffer. We want to have all free buffers,
         which have been played so far. Normally there should be a function
         in the kernel API to get the number of all free buffers.
         I don't want to change this API at the moment, therefore
         we look on the clock to see if there are more buffers ready */

      first_free = nsync;
      do
      {
	mjpeg_sync_buf(mjpeg, &bs);

	frame = bs.frame;
	/* Since we queue the frames in order, we have to get them back in order */
	if(frame != nsync % mjpeg->br.count)
	  {
	    printf("frame = %d, nsync = %ld, mjpeg->br.count = %ld\n", frame, nsync, mjpeg->br.count);
            lavplay_msg(LAVPLAY_INTERNAL,"Wrong frame order on sync","");
	    //mjpeg_close(mjpeg);
	    x_shutdown(1);
            exit(1);
	  }
	nsync++;
	/* Look on clock */
	gettimeofday(&time_now,0);
	tdiff =  time_now.tv_sec  - bs.timestamp.tv_sec +
	  (time_now.tv_usec - bs.timestamp.tv_usec)*1.e-6;
      }
      while(tdiff>spvf && (nsync-first_free)<mjpeg->br.count-1);

      if(gui_mode) printf("@%lc%ld/%ld/%d\n",el.video_norm,frame_number[frame],
                                          el.video_frames,play_speed);

      if((nsync-first_free)> mjpeg->br.count-3)
         lavplay_msg(LAVPLAY_WARNING,"Disk too slow, can not keep pace!","");

      if(el.has_audio && audio_enable)
      {
         audio_get_output_status(&audio_tmstmp,&nb_out,&nb_err);
         if(audio_tmstmp.tv_sec)
         {
            tdiff1 = spvf*(nsync-nvcorr) - spas*audio_buffer_size/el.audio_bps*nb_out;
            tdiff2 = (bs.timestamp.tv_sec -audio_tmstmp.tv_sec )
                   + (bs.timestamp.tv_usec-audio_tmstmp.tv_usec)*1.e-6;
         }
      }

      tdiff = tdiff1-tdiff2;

      /* Fill and queue free buffers again */

      res = 0;
      for(n=first_free;n<nsync;)
      {
         /* Audio/Video sync correction */

         skipv = 0;
         skipa = 0;
         skipi = 0;

         if(sync_corr)
         {
            if(tdiff>spvf)
            {
               /* Video is ahead audio */
               skipa = 1;
               if(sync_ins_frames) skipi = 1;
               nvcorr++;
               numca++;
               tdiff-=spvf;
            }
            if(tdiff<-spvf)
            {
               /* Video is behind audio */
               skipv = 1;
               if(!sync_skip_frames) skipi = 1;
               nvcorr--;
               numcb++;
               tdiff+=spvf;
            }
         }

         /* Read one frame, break if EOF is reached */

         frame = n%mjpeg->br.count;
         frame_number[frame] = nframe;
         res = queue_next_frame(buff+frame*mjpeg->br.size,skipv,skipa,skipi);
         if(res) break;
         if(skipv) continue; /* no frame has been read */

         /* Queue the frame */

	 mjpeg_queue_buf(mjpeg, frame);

         nqueue++;
         n++;
      }
      if (res) break;

      /* See if we got input */

      res = read(0,input_buffer,256);
      if(res>0)
      {
         input_buffer[res-1] = 0;
         if(input_buffer[0]=='q') break; /* We are done */
         switch(input_buffer[0])
         {
            case 'p': play_speed = atoi(input_buffer+1); break;
            case '+': inc_frames( 1); break;
            case '-': inc_frames(-1); break;
            case 'a': audio_mute = input_buffer[1]=='0'; break;
            case 's':
               if(input_buffer[1]=='+')
               {
                  nskip = atoi(input_buffer+2);
                  inc_frames(nskip);
               }
               else if(input_buffer[1]=='-')
               {
                  nskip = atoi(input_buffer+2);
                  inc_frames(-nskip);
               }
               else
               {
                  nskip = atoi(input_buffer+1)-nframe;
                  inc_frames(nskip);
               }
               break;

            case 'e':

               /* Do some simple editing:
                  next chars are u (for cUt), o (for cOpy) or p (for Paste) */

               if(input_buffer[1]=='u'||input_buffer[1]=='o')
               {
                  int nc1, nc2;
                  sscanf(input_buffer+2,"%d %d",&nc1,&nc2);
                  if(nc1>=0 && nc2>=nc1 && nc2<el.video_frames)
                  {
                     /* Save Selection */

                     if(save_list) free(save_list);
                     save_list = (long*) malloc((nc2-nc1+1)*sizeof(long));
                     if(save_list==0) malloc_error();
                     k = 0;
                     for(i=nc1;i<=nc2;i++) save_list[k++] = el.frame_list[i];
                     save_list_len = k;

                     /* Cut Frames */

                     if(input_buffer[1]=='u')
                     {
                        if(nframe>=nc1 && nframe<=nc2) nframe = nc1-1;
                        if(nframe>nc2) nframe -= k;
                        for(i=nc2+1;i<el.video_frames;i++) el.frame_list[i-k] = el.frame_list[i];
                        el.video_frames -= k;
                     }
                     printf("Cut/Copy done ---- !!!!\n");
                  }
                  else
                  {
                     printf("Cut/Copy %d %d params are wrong!\n",nc1,nc2);
                  }
               }

               if(input_buffer[1]=='p')
               {
                  /* We should output a warning if save_list is empty,
                     we just insert nothing */

                  el.frame_list = realloc(el.frame_list,(el.video_frames+save_list_len)*sizeof(long));
                  if(el.frame_list==0) malloc_error();
                  k = save_list_len;
                  for(i=el.video_frames-1;i>nframe;i--) el.frame_list[i+k] = el.frame_list[i];
                  k = nframe+1;
                  for(i=0;i<save_list_len;i++) el.frame_list[k++] = save_list[i];
                  el.video_frames += save_list_len;
                  printf("Paste done ---- !!!!\n");
               }
               break;

            case 'w':

               /* Write edit list */

               if(input_buffer[1]=='a')
               {
                  char filename[256];
                  sscanf(input_buffer+3,"%s",filename);
                  write_edit_list(filename,0,el.video_frames-1,&el);
               }
               if(input_buffer[1]=='s')
               {
                  char filename[256];
                  int ns1, ns2;
                  sscanf(input_buffer+3,"%d %d %s",&ns1,&ns2,filename);
                  write_edit_list(filename,ns1,ns2,&el);
               }

               break;
         }
         printf("- play speed =%3d, pos =%6ld/%ld >",play_speed,nframe,el.video_frames);
      }
      fflush(stdout);
      fflush(stderr);
   }

   /* All buffers are queued, sync on the outstanding buffers */
   /* Never try to sync on the last buffer, it is a hostage of
      the codec since it is played over and over again */

   while(nqueue > nsync + 1)
   {
     mjpeg_sync_buf(mjpeg, &bs);
     nsync++;
   }

   /* Stop streaming playback */
   mjpeg_close(mjpeg);
   if (soft_play) SDL_Quit();

   if(sync_corr)
   {
      sprintf(infostring,"Corrections because video ahead  audio: %4d",numca);
      lavplay_msg(LAVPLAY_INFO,infostring,"");
      sprintf(infostring,"Corrections because video behind audio: %4d",numcb);
      lavplay_msg(LAVPLAY_INFO,infostring,"");
   }

   exit(0);
}






