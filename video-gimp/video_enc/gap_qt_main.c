/* gap_qt_main.c
 * 1999.10.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains implementation of Quicktime 
 * Video+Audio encoding for GAP Anim Frames.
 * it is based based on the (free) quicktime4linux Library
 *  this lib does 
 *     - not support commercial codecs.
 *     - audio raw encoding is not implemented (in quicktime4linux1.1.3)
 *         
 * - gap_range_qt_encode
 *
 */
/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


/* revision history:
 * version 1.1.22a; 2000.05.27   hof: use quicktime4linux 1.1.4x, addiditonal codec mjpa
 *                                     BUG in quicktime4linux crashes in quicktime_startcompressor_jpeg
 *                                     (not on all machines?)  that's why this gap_qt version
 *                                     was not published to the registry !!!!
 * version 1.1.13a; 1999.11.28   hof: moved common procedures to gap_enc_lib.c and gap_enc_audio.c
 *                                    Internationalization NLS Macros
 * version 1.1.11b; 1999.11.20   hof: longer entry fields for filenames
 * version 1.1.11a; 1999.11.15   hof: menu changed from AnimFrames to Video
 * version 1.1.9b; 1999.10.06    hof: 1.st (pre) release
 */


/* Quicktime4Linux includes */
#include "quicktime.h"
 
/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include <stdplugins-intl.h>
#include <libgimp/gimp.h>

/* GAP includes */
#include "gap_lib.h"
#include "gap_arr_dialog.h"
#include "gap_enc_lib.h"
#include "gap_enc_audio.h"

static char *gap_qt_version = "1.1.22a; 2000/05/25";

#define PLUGIN_NAME_QT_ENCODE "plug_in_gap_qt_encode"

gint32 gap_qt_encode(GRunModeType run_mode, gint32 image_id,
                      long range_from, long range_to,
                      char *filename_mov, char *vid_codec, gdouble framerate,
                      char *filename_aud, char *filename_aud2,
		      char *aud_codec, long samplerate);
                      
/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;

static void query(void);
static void run(char *name, int nparam, GParam *param,
                int *nretvals, GParam **retvals);

GPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};

/* ------------------------
 * MAIN query and run
 * ------------------------
 */

MAIN ()

static void
query ()
{
  static GParamDef args_qt_enc[] =
  {
    {PARAM_INT32, "run_mode", "Interactive, non-interactive"},
    {PARAM_IMAGE, "image", "Input image"},
    {PARAM_DRAWABLE, "drawable", "Input drawable (unused)"},
    {PARAM_STRING, "vidfile", "filename of quicktime video (to write)"},
    {PARAM_STRING, "vid_codec", "codec id is one of these 4 byte strings: "
                                "\""  QUICKTIME_JPEG "\""
                                "\""  QUICKTIME_MJPA  "\""
                                "\""  QUICKTIME_PNG  "\""
                                "\""  QUICKTIME_YUV2 "\""
                                "\""  QUICKTIME_YUV4 "\""
                                "\""  QUICKTIME_RAW  "\""
                               },
    {PARAM_FLOAT, "framerate", "framerate in frames per seconds"},
    {PARAM_STRING, "audfile1", "optional audiodata file .wav or raw audiodata, must contain uncompressed 16 bit samples. pass empty string if no audiodata should be included"},
    {PARAM_STRING, "audfile2", "optional 2.nd audiodata file. pass empty string if no additional audiodata should be included"},
    {PARAM_STRING, "aud_codec", "codec id is one of these 4 byte strings: "
                                "\""  QUICKTIME_IMA4 "\""
                                "\""  QUICKTIME_TWOS "\""
                                "\""  QUICKTIME_ULAW "\""
                                "\""  QUICKTIME_RAW  "\""
                               },
    {PARAM_INT32, "samplerate", "audio samplerate in samples per seconds (is ignored .wav files are used)"},
  };
  static int nargs_qt_enc = sizeof(args_qt_enc) / sizeof(args_qt_enc[0]);

  static GParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  INIT_I18N();

  gimp_install_procedure(PLUGIN_NAME_QT_ENCODE,
                         _("This plugin does quicktime video + audio encoding for anim frames"),
			 _("This plugin does quicktime video + audio encoding for the selected range of animframes."
			 " the (optional) audiodata must be a raw datafile(s) or .wav (RIFF WAVEfmt ) file(s)"
			 " .wav files can be mono (1) or stereo (2channels) audiodata must be 16bit uncompressed"),
			 "Wolfgang Hofer (hof@hotbot.com)",
			 "Wolfgang Hofer",
			 gap_qt_version,
			 N_("<Image>/Video/Encode/Quicktime"),
			 "RGB*, INDEXED*, GRAY*",
			 PROC_PLUG_IN,
			 nargs_qt_enc, nreturn_vals,
			 args_qt_enc, return_vals);

			 
}	/* end query */


static void
run (char    *name,
     int      n_params,
     GParam  *param,
     int     *nreturn_vals,
     GParam **return_vals)
{
#define  MAX_FNAME_LEN  256

  char l_vidname[MAX_FNAME_LEN];
  char l_audname[MAX_FNAME_LEN];
  char l_audname2[MAX_FNAME_LEN];
  char l_vid_codec[MAX_FNAME_LEN];
  char l_aud_codec[MAX_FNAME_LEN];
  static GParam values[1];
  GRunModeType run_mode;
  GStatusType status = STATUS_SUCCESS;
  gint32     image_id;
  long       range_from;
  long       range_to;
  gdouble    framerate;
  long       samplerate;

  
  gint32     l_rc;
  char       *l_env;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  run_mode = param[0].data.d_int32;
  l_vidname[0] = '\0';
  l_audname[0] = '\0';
  strcpy(l_vid_codec, QUICKTIME_JPEG);
  strcpy(l_aud_codec, QUICKTIME_IMA4);
  framerate = 30.0;
  range_from = 0;
  range_to   = 0;
  samplerate = 0;
  
  if(gap_debug) fprintf(stderr, "\n\ngap_qt_main: debug name = %s\n", name);
   
  if (run_mode == RUN_NONINTERACTIVE) {
    INIT_I18N();
  } else {
    INIT_I18N_UI();
  }
  
  if (strcmp (name, PLUGIN_NAME_QT_ENCODE) == 0)
  {
      if (run_mode == RUN_NONINTERACTIVE)
      {
        if (n_params != 10)
        {
          status = STATUS_CALLING_ERROR;
        }
        else
        {
          strncpy(l_vidname,   param[3].data.d_string, MAX_FNAME_LEN -1);
          l_vidname[MAX_FNAME_LEN -1] = '\0';
          strncpy(l_vid_codec, param[4].data.d_string, 5);
          framerate   =        param[5].data.d_float;
          strncpy(l_audname,   param[6].data.d_string, MAX_FNAME_LEN -1);
          l_audname[MAX_FNAME_LEN -1] = '\0';
          strncpy(l_audname2,  param[7].data.d_string, MAX_FNAME_LEN -1);
          l_audname2[MAX_FNAME_LEN -1] = '\0';
          strncpy(l_aud_codec, param[8].data.d_string, 5);
          samplerate  =        param[9].data.d_int32;
        }
      }
      else if(run_mode != RUN_INTERACTIVE)
      {
         status = STATUS_CALLING_ERROR;
      }

      if (status == STATUS_SUCCESS)
      {

        image_id    = param[1].data.d_image;

        l_rc = gap_qt_encode(run_mode, image_id, 
                             range_from, range_to,
                             l_vidname, l_vid_codec, framerate,
                             l_audname, l_audname2, l_aud_codec, samplerate);
      }
  }
  else
  {
      status = STATUS_CALLING_ERROR;
  }

 if(l_rc < 0)
 {
    status = STATUS_EXECUTION_ERROR;
 }
 

  values[0].type = PARAM_STATUS;
  values[0].data.d_status = status;

}



void 
p_set_vid_compressor(char *vid_compressor, long vid_codec)
{
   switch(vid_codec)
   {
     case 0:
       strcpy(vid_compressor, QUICKTIME_JPEG);
       break;
     case 1:
       strcpy(vid_compressor, QUICKTIME_MJPA);
       break;
     case 2:
       strcpy(vid_compressor, QUICKTIME_PNG);
       break;
     case 3:
       strcpy(vid_compressor, QUICKTIME_YUV2);
       break;
     case 4:
       strcpy(vid_compressor, QUICKTIME_YUV4);
       break;
     default:
       strcpy(vid_compressor, QUICKTIME_RAW);
       break;
   }
   
   if(gap_debug) printf("selected video_codec: %s\n", vid_compressor);
}

void 
p_set_aud_compressor(char *aud_compressor, long aud_codec)
{
   switch(aud_codec)
   {
     case 0:
       strcpy(aud_compressor, QUICKTIME_IMA4);
       break;
     case 1:
       strcpy(aud_compressor, QUICKTIME_ULAW);
       break;
     case 2:
       strcpy(aud_compressor, QUICKTIME_TWOS);
       break;
     case 3:
       strcpy(aud_compressor, QUICKTIME_RAW);
       break;
     default:
       strcpy(aud_compressor, QUICKTIME_IMA4);
       break;
   }
   if(gap_debug) printf("selected audio_codec: %s\n", aud_compressor);
}



/* ============================================================================
 * p_qt_encode_dialog
 *
 *   return  0 .. OK 
 *          -1 .. in case of Error or cancel
 * ============================================================================
 */
static long
p_qt_encode_dialog(t_anim_info *ainfo_ptr,
                      long *range_from, long *range_to,
                      char *filename_mov, long len_mov, char *vid_codec, gdouble *framerate,
                      char *filename_aud, long len_aud,
		      char *filename_aud2,long len_aud2,
		      char *aud_codec,    long *samplerate)
{
#define QT_DIALOG_ARGC 11
  static t_arr_arg  argv[QT_DIALOG_ARGC];
  static char *radio_vidargs[6]  = {"jpeg", "mjpa", "png ", "yuv2", "yuv4", "raw "};
  static char *radio_audargs[4]  = {"ima4", "ulaw", "twos", "raw "};
  
  p_init_arr_arg(&argv[0], WGT_INT_PAIR);
  argv[0].constraint = TRUE;
  argv[0].label_txt = _("From Frame:");
  argv[0].help_txt  = _("first handled frame");
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  
  p_init_arr_arg(&argv[1], WGT_INT_PAIR);
  argv[1].constraint = TRUE;
  argv[1].label_txt = _("To   Frame:");
  argv[1].help_txt  = _("last handled frame");
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;

  p_init_arr_arg(&argv[2], WGT_LABEL);
  argv[2].label_txt = _("\nQuicktime Video encode Options\n");

  p_init_arr_arg(&argv[3], WGT_FILESEL);
  argv[3].label_txt = _("Video:");
  argv[3].help_txt  = _("Name of the quicktime video to write\n(should end with extension .mov)");
  argv[3].text_buf_len = len_mov;
  argv[3].text_buf_ret = filename_mov;
  argv[3].entry_width = 250;
  
  p_init_arr_arg(&argv[4], WGT_FLT_PAIR);
  argv[4].constraint = TRUE;
  argv[4].label_txt = _("Framerate :");
  argv[4].help_txt  = _("Enter Framerate in frames per second");
  argv[4].flt_min   = 1.0;
  argv[4].flt_step  = 1.0;
  argv[4].pagestep  = 10.0;
  argv[4].flt_max   = 100.0;
  argv[4].flt_ret   = 10.0;

  p_init_arr_arg(&argv[5], WGT_OPTIONMENU);
  argv[5].label_txt = _("Video Codec :");
  argv[5].help_txt  = _("Select Video encoding\n(YUV4 is not supported by Quicktime for Windows or Mac)");
  argv[5].radio_argc  = 6;
  argv[5].radio_argv = radio_vidargs;
  argv[5].radio_ret  = 0;

  p_init_arr_arg(&argv[6], WGT_LABEL);
  argv[6].label_txt = _("\nQuicktime Audio encode Options (optional)\n");

  p_init_arr_arg(&argv[7], WGT_FILESEL);
  argv[7].label_txt = _("Audiofile1:");
  argv[7].help_txt  = _("Name of an optional WAV or RAW Audiofile to include\n"
                        "must contain uncompressed 16bit audiosampledata\n"
			"for 1 channel \n(stereo .wav files with 2 channels are also accepted)");
  argv[7].text_buf_len = len_aud;
  argv[7].text_buf_ret = filename_aud;
  argv[7].entry_width = 250;

  p_init_arr_arg(&argv[8], WGT_FILESEL);
  argv[8].label_txt = _("Audiofile2:");
  argv[8].help_txt  = _("Name of an 2.nd optional WAV or RAW Audiofile\n"
                        "for 2nd channel (stereo)\nboth audiodatafiles must have the same samplerate");
  argv[8].text_buf_len = len_aud2;
  argv[8].text_buf_ret = filename_aud2;
  argv[8].entry_width = 250;

  p_init_arr_arg(&argv[9], WGT_INT_PAIR);
  argv[9].constraint = FALSE;
  argv[9].label_txt = _("Samplerate  :");
  argv[9].help_txt  = _("Audio Samplerate of the Audiofile\n(in Samples per second)\nis ignored for .wav files");
  argv[9].int_min   = 10000;
  argv[9].int_max   = 100000;
  argv[9].umin      = 1.0;
  argv[9].entry_width = 80;
  argv[9].int_ret   = 44100; /* 11025 */;
  argv[9].int_step  = 100;
  argv[9].pagestep  = 1000;


  p_init_arr_arg(&argv[10], WGT_OPTIONMENU);
  argv[10].label_txt = _("Audio Codec :");
  argv[10].help_txt  = _("Select Audio encoding (IMA4 preferred)\n"
                         "(WARNING: ima4 is the only working choice\nin quicktime4linux 1.1.3)");
  argv[10].radio_argc  = 4;
  argv[10].radio_argv = radio_audargs;
  argv[10].radio_ret  = 0;


  if(0 != p_chk_framerange(ainfo_ptr))   return -1;

  if(TRUE == p_array_dialog(_("Quicktime Encoding"),
                            _("Settings :"), 
                            QT_DIALOG_ARGC, argv))
  {
      *range_from                   = (long)(argv[0].int_ret);
      *range_to                     = (long)(argv[1].int_ret);
      *framerate                 = (gdouble)(argv[4].flt_ret);
      p_set_vid_compressor(vid_codec, (long)(argv[5].int_ret));
      *samplerate                   = (long)(argv[9].int_ret);
      p_set_aud_compressor(aud_codec, (long)(argv[10].int_ret));

      return 0;
  }
  else
  {
      return -1;
  }
}		/* end p_qt_encode_dialog */


int
p_audio_wave_check(GRunModeType run_mode, unsigned char *audata, long audlen, char *audfile,
                   QUICKTIME_INT16 **aud_idata_ptr, long *sample_rate, long *samples, int *channels)
{
  /* note: this code is a quick hack and was inspired by a look at some
   *       .wav files. It will work on many .wav files.
   *       but it also may fail to identify some .wav files.
   */
  unsigned char *l_aud_ptr;
  unsigned char *l_left_ptr;
  unsigned char *l_right_ptr;
  long  l_sample_rate;
  long  l_channels;
  long  l_bytes_per_sample;
  long  l_bits;
  long  l_data_len;
  int   l_idx;
  static char l_msg[1024];
  
  /* check if *audata is a RIFF WAVE format file */
  if(audlen > 48)
  {
    if((audata[0]  == 'R')      /* check for RIFF identifier */
    && (audata[1]  == 'I')
    && (audata[2]  == 'F')
    && (audata[3]  == 'F')
    && (audata[8]  == 'W')      /* check for "WAVEfmt " chunk identifier */
    && (audata[9]  == 'A')
    && (audata[10] == 'V')
    && (audata[11] == 'E')
    && (audata[12] == 'f')
    && (audata[13] == 'm')
    && (audata[14] == 't')
    && (audata[15] == ' ')
    && (audata[36] == 'd')      /* check for data chunk  identifier */
    && (audata[37] == 'a')
    && (audata[38] == 't')
    && (audata[39] == 'a'))
    {
       l_sample_rate = audata[24] + (256 * (long)audata[25])+ (65536 * (long)audata[26]) + (16777216 * (long)audata[27]);
       l_channels = audata[22] + (256 * (long)audata[23]);
       l_bytes_per_sample = audata[32] + (256 * (long)audata[33]);
       l_bits = audata[34] + (256 * (long)audata[35]);
       l_data_len = audata[40] + (256 * (long)audata[41]) + (65536 * (long)audata[42]) + (16777216 * (long)audata[43]);
       l_aud_ptr = &audata[44];

       if((l_bits != 8) && (l_bits != 16))
       {
          sprintf(l_msg, "File %s has unsupported bits per sample value %d\n", audfile, (int)l_bits);
          p_info_win(run_mode, l_msg, "CANCEL");
          return -1;
       }
       
       sprintf(l_msg, "File: %s\nlooks like a Wave File with %d channel(s)\nsamplerate is: %d  %d bits(found in file)\n",
              audfile, (int)l_channels, (int)l_sample_rate, (int)l_bits);
       p_info_win(RUN_NONINTERACTIVE, l_msg, "OK");


       if(l_channels == 2)
       {
          if((l_bytes_per_sample != 4) && (l_bytes_per_sample != 2))
          {
            printf ("unsupported %d stereo bytes_per_sample\n", (int)l_bytes_per_sample);
            return -1;
          }
          l_left_ptr  = g_malloc(2* (l_data_len / l_bytes_per_sample)); 
          if(l_left_ptr == NULL)
          {
            return -1;
          }
          l_right_ptr = g_malloc(2* (l_data_len / l_bytes_per_sample));
          if(l_right_ptr == NULL)
          {
            g_free(l_left_ptr);
            return -1;
          }
          
          if(l_bytes_per_sample == 4)
          {
             p_stereo_split16to16(l_left_ptr, l_right_ptr, l_aud_ptr, l_data_len);
          }
          else
          {
             p_stereo_split8to16(l_left_ptr, l_right_ptr, l_aud_ptr, l_data_len);
          }

          if(aud_idata_ptr[0] != NULL) g_free(aud_idata_ptr[0]);
          aud_idata_ptr[0] = (QUICKTIME_INT16 *)l_left_ptr;

          if(aud_idata_ptr[1] != NULL) g_free(aud_idata_ptr[1]);
          aud_idata_ptr[1] = (QUICKTIME_INT16 *)l_right_ptr;
          
          *samples = l_data_len / l_bytes_per_sample;
       }
       else
       {
          if((l_bytes_per_sample != 2) && (l_bytes_per_sample != 1))
          {
            printf ("unsupported %d mono bytes_per_sample\n", (int)l_bytes_per_sample);
            return -1;
          }
          l_left_ptr  = g_malloc(2* (l_data_len / l_bytes_per_sample));
          if(l_left_ptr == NULL)
          {
            return -1;
          }
          if(l_bytes_per_sample == 2)
          {
            memcpy(l_left_ptr, l_aud_ptr, l_data_len);
          }
          else
          {
            /* expand from 8 bit to 16 bit per sample */
            for(l_idx = 0; l_idx < l_data_len; l_idx++)
            {
	      p_dbl_sample_8_to_16(l_aud_ptr[l_idx],                /* 8bit input */
	                          &l_left_ptr[l_idx +l_idx],        /* lsb output */
				  &l_left_ptr[l_idx +l_idx +1 ]);   /* msb output  */
            }
          }
          if(aud_idata_ptr[0] != NULL) g_free(aud_idata_ptr[0]);
          aud_idata_ptr[0] = (QUICKTIME_INT16 *)l_left_ptr;
          
          *samples = l_data_len / l_bytes_per_sample;
       }
       *sample_rate = l_sample_rate;
       *channels    = l_channels;
    }
  }
  return 0;  /* oK */
}	/* end p_audio_wave_check */

/* write audiodata to the quicktime file.
 *  INPUT:
 *     qt_ptr        .. pointer to an quicktime_t file (file must be already opend for write)
 *     filename_aud  .. audiofile must be one of:
 *                        a) raw audiodata 1 channel with 16bit samples (mono),  or
 *                        b) RIFF WAVE file (.wav) with 1 channel of uncompressed 16 bit data or
 *                        c) RIFF WAVE file (.wav) with 2 channels of uncompressed 16 bit data (stereo)
 *     filename_aud2 .. optional 2nd audiofile.
 *                        NOTE:
 *                           - if 2 stereo .wav files are used, you can encode all 4 audio channels
 *                           - samplerate of all audiofiles must be equal.
 *     aud_codec     .. one of the supported codecs
 *     sample_rate   .. samplerate of the raw audiodata (samples per second)
 *                         NOTE: sample rate parameter is ignored on .wav files.
 *                               (on .wav files we use the sample_rate that is stored in the file)
 */
int
p_audio_write(GRunModeType run_mode, quicktime_t *qt_ptr, char *filename_aud,  char *filename_aud2, char *aud_compressor, long sample_rate)
{
  float            **aud_fdata_ptr;
  QUICKTIME_INT16  *aud_idata_ptr[4];   /* pointers to 4 audio channels */
  unsigned char    *audata_ptr;
  int               l_channels;
  int               l_chan_wav;
  int               l_bits;
  long              l_samples;
  long              l_samples1;
  long              l_samples2;
  long              l_sample_rate_wav;
  int               l_rc;
 
  
  l_channels = 1;      /* one audio channel */
  l_bits = 16;         /* 16 bits per sample */
  l_rc = 0;
  l_sample_rate_wav = sample_rate;

  if(filename_aud == NULL)
  {
    return -1;
  }
  
  aud_fdata_ptr = NULL;
  audata_ptr = p_load_file(filename_aud);
  aud_idata_ptr[0] = (QUICKTIME_INT16 *)audata_ptr; 
  aud_idata_ptr[1] = aud_idata_ptr[2] = aud_idata_ptr[3] = NULL;
  
  l_samples1 = p_get_filesize(filename_aud)  / 2;
  l_samples = l_samples1;
  
  if((aud_idata_ptr[0] == NULL) || (l_samples < 1))
  {
     printf("Could not read audio data from file: %s\n", filename_aud);
     l_rc = -1;
  }
  else
  {
    l_rc = p_audio_wave_check(run_mode, audata_ptr, l_samples1 *2 , filename_aud,
                       &aud_idata_ptr[0], &sample_rate, &l_samples1, &l_channels);

    l_samples  = l_samples1;
    
    /* check and load the optional 2.nd audio channel (stereo) */
    if((filename_aud2 != NULL) && (l_rc == 0))
    {
      if(*filename_aud2 != '\0')
      {
         l_samples2 = p_get_filesize(filename_aud2)  / 2;
	 if(l_samples2 > 1)
	 {
	    audata_ptr = p_load_file(filename_aud2);
            aud_idata_ptr[l_channels] = (QUICKTIME_INT16 *)audata_ptr;

            l_rc = p_audio_wave_check(run_mode, audata_ptr, l_samples2 *2, filename_aud,
                       &aud_idata_ptr[l_channels], &l_sample_rate_wav, &l_samples2, &l_chan_wav);
            
            if(l_sample_rate_wav != sample_rate)
            {
               printf("Samplerates do not match\n  file: %s  samplerate is: %d\n  file: %s  samplerate is: %d\n",
                     filename_aud, (int)sample_rate, filename_aud2, (int)l_sample_rate_wav);
               l_rc = -1;
            }   
	    l_samples = MIN (l_samples1, l_samples2);    /* if length differs, we use the shorter audiodata */
	    l_channels += l_chan_wav;
	 }

      }
    }

    quicktime_set_audio(qt_ptr, l_channels, sample_rate, l_bits, aud_compressor);  
    if (quicktime_encode_audio(qt_ptr, aud_idata_ptr, aud_fdata_ptr, l_samples))
    {
      l_rc = -1;
    }
  }

  if(aud_idata_ptr[0]) g_free(aud_idata_ptr[0]);  
  if(aud_idata_ptr[1]) g_free(aud_idata_ptr[1]);  
  if(aud_idata_ptr[2]) g_free(aud_idata_ptr[2]);  
  if(aud_idata_ptr[3]) g_free(aud_idata_ptr[3]);  

  return l_rc;
}


void
p_drawable_to_qt_row(GDrawable *drawable, int row, unsigned char *data_ptr)
{
  GPixelRgn srcPR;
  gint l_width, l_height, l_bytes;
  guchar *l_buffer;
  guchar *l_bptr;
  int    l_idx;

  /* Get the size of the input drawable. */
  l_width = drawable->width;
  l_height = drawable->height;

  /* Get the bytes per pixel of the input drawable. */
  l_bytes = drawable->bpp;

  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, l_width, l_height,
                         FALSE, FALSE);
  if(l_bytes == 3)
  {
     /* gimp_row data has 3 bytes per pixel as needed
      * so we can retrieve the row data without changes from gimp
      */
     gimp_pixel_rgn_get_row (&srcPR, data_ptr, 0, row, l_width);
     return;
  }

  l_buffer = (guchar *) g_malloc (l_width * l_bytes);
  gimp_pixel_rgn_get_row (&srcPR, l_buffer, 0, row, l_width);

  l_bptr = l_buffer;
  if(l_bytes == 4)         /* RGBA */
  {
     for(l_idx=0; l_idx < l_width; l_idx++)
     {
       *(data_ptr++) = l_bptr[0]; 
       *(data_ptr++) = l_bptr[1]; 
       *(data_ptr++) = l_bptr[2];
       l_bptr += l_bytes;
     }
  }
  else                     /* l_bytes= 1 or 2 (GREY or GREYA) */
  {
     for(l_idx=0; l_idx < l_width; l_idx++)
     {
       *(data_ptr++) = *l_bptr; 
       *(data_ptr++) = *l_bptr; 
       *(data_ptr++) = *l_bptr;
       l_bptr += l_bytes;
     }
  }

  g_free(l_buffer);

}	/* end p_drawable_to_qt_row */

/* ============================================================================
 * p_qt_encode
 *    Quicktime encoding aof anim frames, based on quicktime4linux lib
 *    (flatten the images if desired)
 *    Optional you can add an audiotrack 
 *        (raw_audiodatafile and samplerate must be provided in that case)
 *
 * returns   value >= 0 if all is ok 
 *           (or -1 on error)
 * ============================================================================
 */
static int
p_qt_encode(t_anim_info *ainfo_ptr,
                 long range_from, long range_to,
                 char *vidname, char *vid_codec, 
                 gdouble framerate,
                 char *audname, char *audname2, char *aud_codec, long samplerate
                 )
{
  gint32     l_tmp_image_id;
  gint32     l_layer_id;
  GDrawable *l_drawable;
  long       l_cur_frame_nr;
  long       l_step, l_begin, l_end;
  gint       l_nlayers;
  gint32    *l_layers_list;
  gdouble    l_percentage, l_percentage_step;
  int        l_idx;
  int        l_rc;

  quicktime_t    *qt_ptr;
  int             l_width, l_height;
  unsigned char **row_pointers;
  
  l_rc = 0;
  l_layer_id = -1; l_width = 0; l_height = 0;
  
  /* open quicktime file for write */
  qt_ptr = quicktime_open(vidname, 0, 1);
  if(qt_ptr == NULL)
  {
     printf("quicktime_open write failed on %s\n", vidname);
     return -1;
  }
  
  l_percentage = 0.0;
  if(ainfo_ptr->run_mode == RUN_INTERACTIVE)
  { 
    gimp_progress_init("Quicktime Encoding ..");
  }
 

  l_begin = range_from;
  l_end   = range_to;
  
  if(range_from > range_to)
  {
    l_step  = -1;     /* operate in descending (reverse) order */
    l_percentage_step = 1.0 / ((1.0 + range_from) - range_to);

    if(range_to < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_from > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }
  else
  {
    l_step  = 1;      /* operate in ascending order */
    l_percentage_step = 1.0 / ((1.0 + range_to) - range_from);

    if(range_from < ainfo_ptr->first_frame_nr)
    { l_begin = ainfo_ptr->first_frame_nr;
    }
    if(range_to > ainfo_ptr->last_frame_nr)
    { l_end = ainfo_ptr->last_frame_nr;
    }
  }
  
  
  l_cur_frame_nr = l_begin;
  while(l_rc >= 0)
  {
    /* build the frame name */
    if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                        l_cur_frame_nr,
                                        ainfo_ptr->extension);
    if(ainfo_ptr->new_filename == NULL)
       return -1;

    /* load current frame */
    l_tmp_image_id = p_load_image(ainfo_ptr->new_filename);
    if(l_tmp_image_id < 0)
       return -1;
    
    l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);
    if(l_layers_list != NULL)
    {
      l_layer_id = l_layers_list[0];
      g_free (l_layers_list);
    }

    if(l_nlayers > 1 )
    {
       if(gap_debug) fprintf(stderr, "DEBUG: p_qt_encode flatten tmp image'\n");
       
       /* flatten current frame image (reduce to single layer) */
       l_layer_id = gimp_image_flatten (l_tmp_image_id);

    }

    if(l_cur_frame_nr == l_begin)
    {
       l_width = (int) gimp_image_width(l_tmp_image_id);
       l_height = (int) gimp_image_height(l_tmp_image_id);
       
       /* before encoding 1.st frame, setup width and height */
       quicktime_set_video(qt_ptr, 1, l_width, l_height, (float)framerate, vid_codec);

       /* and add (optional) audio channels */
       if((*audname != '\0') || (*audname2 != '\0'))
       {
         if(*audname != '\0') p_audio_write(ainfo_ptr->run_mode, qt_ptr, audname, audname2, aud_codec, samplerate);
	 else                 p_audio_write(ainfo_ptr->run_mode, qt_ptr, audname2, audname, aud_codec, samplerate);
       }
    }

    l_drawable = gimp_drawable_get (l_layer_id);
    
    /* Allocate row_pointers array forech row of the image
     * as needed by quicktime
     */
    row_pointers = g_malloc(sizeof(unsigned char*) * l_height);
    if(row_pointers == NULL)
    {
       printf("GAP: could not allocate mem for quicktime encode pointers\n");
       l_rc = -1;
    }
    else
    {
      for(l_idx = 0; l_idx < l_height; l_idx++)
      {
         row_pointers[l_idx] = g_malloc(3 * l_width);          /* allocate one row */
         if(row_pointers[l_idx] == NULL)
         {
            printf("GAP: could not allocate mem for quicktime encode data\n");
            l_rc = -1;
            break;
         }
         else
         {
            p_drawable_to_qt_row(l_drawable, l_idx, row_pointers[l_idx]);
         }
      }
    }

    if(gap_debug) printf("before quicktime_encode_video cur_frame:%d\n", (int)l_cur_frame_nr);

    /* quicktime_encode one video frame */
    if(l_rc == 0) quicktime_encode_video(qt_ptr, row_pointers, 0);

    if(gap_debug) printf("after quicktime_encode_video cur_frame:%d\n", (int)l_cur_frame_nr);

    /* free the allocated stuff for the current frame */
    for(l_idx = 0; l_idx < l_height; l_idx++)
    {
       if(row_pointers[l_idx]) g_free(row_pointers[l_idx]);
    }
    if(row_pointers) g_free(row_pointers);

    /* destroy the tmp image */
    gimp_image_delete(l_tmp_image_id);

    if(ainfo_ptr->run_mode == RUN_INTERACTIVE)
    { 
      l_percentage += l_percentage_step;
      gimp_progress_update (l_percentage);
    }

    /* advance to next frame */
    if((l_cur_frame_nr == l_end) || (l_rc < 0))
       break;
    l_cur_frame_nr += l_step;

  }

  quicktime_close(qt_ptr);
  return l_rc;  
}	/* end p_qt_encode */


/* ============================================================================
 * gap_qt_encode
 *   quicktime encoding of the selected frame range
 * ============================================================================
 */
gint32 gap_qt_encode(GRunModeType run_mode, gint32 image_id,
                      long range_from, long range_to,
                      char *filename_mov, char *vid_codec, gdouble framerate,
                      char *filename_aud, char *filename_aud2, char *aud_codec, long samplerate
                     )
{
  gint32  l_rc;
  long    l_from, l_to;
  gdouble l_framerate;
  long    l_samplerate;
  char   *l_str;
  
  t_anim_info *ainfo_ptr;
  char  l_vidname[MAX_FNAME_LEN +6];
  char  l_audname[MAX_FNAME_LEN +6];
  char  l_audname2[MAX_FNAME_LEN +6];

  l_rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
#if (( GIMP_MAJOR_VERSION > 0 ) && ( GIMP_MINOR_VERSION > 0))
      l_str = p_strdup_del_underscore(ainfo_ptr->basename);
      sprintf(l_vidname, "%s.mov", l_str);
      g_free(l_str);
#else
      /* p_strdup_del_underscore is not available in older gap versions
       * (from the registry) that are used with gimp 1.0.x 
       */
      sprintf(l_vidname, "%s.mov", ainfo_ptr->basename);
#endif
      l_audname[0] = '\0';
      l_audname2[0] = '\0';
     
      if(run_mode == RUN_INTERACTIVE)
      {
         /* p_qt_encode_dialog : select destination type
          * to find out extension
          */
         l_rc = p_qt_encode_dialog (ainfo_ptr, &l_from, &l_to,
                                  &l_vidname[0], sizeof(l_vidname), 
                                  &vid_codec[0],
                                  &l_framerate,
                                  &l_audname[0], sizeof(l_audname), 
                                  &l_audname2[0], sizeof(l_audname2), 
                                  &aud_codec[0],
				  &l_samplerate
                                  );

      }
      else
      {
         l_rc = 0;
         l_from    = range_from;
         l_to      = range_to;
         l_framerate = framerate;
         l_samplerate = samplerate;
         if(filename_mov != NULL)
	 {
	   if(*filename_mov != '\0') strcpy (l_vidname, filename_mov);
	 }
         if(filename_aud != NULL)
	 {
	   if(*filename_aud != '\0') strcpy (l_audname, filename_aud);
	 }
         if(filename_aud2 != NULL)
	 {
	   if(*filename_aud2 != '\0') strcpy (l_audname2, filename_aud2);
	 }
         
      }

      if(l_rc >= 0)
      {
            l_rc = p_qt_encode(ainfo_ptr, l_from, l_to,
                               l_vidname, vid_codec, 
                               l_framerate,
                               l_audname, l_audname2, aud_codec,
			       l_samplerate
                               );
      }
    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(l_rc);    
}	/* end gap_range_conv */
