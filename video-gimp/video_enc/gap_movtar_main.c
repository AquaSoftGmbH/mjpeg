/* gap_movtar_main.c by Gernot Ziegler (gz@lysator.liu.se)
 * based on gap_qt
 * done 1999.10.01 by hof (Wolfgang Hofer)
 *
 * (GAP ... GIMP Animation Plugins)
 *
 * This module contains an implementation of movtar
 * video & audio encoding for GAP AnimFrames.
 * (for more information about the movtar format see
 *  http://www.lysator.liu.se/~gz/buz)        
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
 * version 1.1.13a; 991113   hof: changed to main program, NLS_makros
 * version 0.0.1; 991113   gz: 1st release
 */

/* SYSTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "libgimp/stdplugins-intl.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lib.h"
#include "gap_arr_dialog.h"
#include "gap_enc_lib.h"
#include "gap_enc_audio.h"
#include "gap_enc_jpeg.h"

/* movtar includes */
#include <tar.h>
#include "movtar.h"

#define MOVTAR_PAL 0
#define MOVTAR_NTSC 1
                      
/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

static int movtar_debug = FALSE;
int gap_debug = FALSE;

static char *gap_movtar_version = "1.1.13a; 1999/11/28";

#define PLUGIN_NAME_MOVTAR_ENCODE "plug_in_gap_movtar_encode"



gint32 gap_movtar_encode(GRunModeType run_mode, gint32 image_id,
			 long range_from, long range_to,
			 char *vidname, gint32 dont_recode_frames, 
			 char *wavname, gint32 auto_videoparam, 
			 char *videonorm, gint32 jpeg_interlaced, 
			 char *text_author, char *text_software,
			 char *text_device, char *text_input, 
			 char *text_contdescription, char *text_contclassification, 
			 gint32 jpeg_quality
			 );

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

static void query ()
{
  static GParamDef args_foreach[] =
  {
    {PARAM_INT32, "run_mode", "Interactive, non-interactive"},
    {PARAM_IMAGE, "image", "Input image"},
    {PARAM_DRAWABLE, "drawable", "Input drawable (unused)"},
    {PARAM_STRING, "vidfile", "filename of the movtar video (to write)"},
    {PARAM_INT32, "dont_recode_frames", "=1: store the frames _directly_ into the movtar. "
                                 "(works only for 4:2:2 JPEG !)"},
    {PARAM_STRING, "wavfile", "optional audiodata file .wav audiodata, pass empty string if no audiodata should be included"},
    {PARAM_INT32, "auto_videoparam", "automatic or manual video parameters ?"},
    {PARAM_STRING, "videonorm", "The used videonorm is one of these 2 strings (ignored if auto_videoparam=1): "
                                "\""  "TVNORM_PAL" "\""
                                "\""  "TVNORM_NTSC"  "\""
                               },
    {PARAM_INT32, "jpeg_interlaced", "=1: store two JPEG frames, for the odd/even lines "
                                 "(ignored if auto_videoparam=1): "},
    {PARAM_STRING, "text_author", "The content of the Author field in the movtar."},
    {PARAM_STRING, "text_software", "The content of the Software field in the movtar."},
    {PARAM_STRING, "text_device", "The content of the Device field in the movtar."},
    {PARAM_STRING, "text_input", "The content of the Input field in the movtar."},
    {PARAM_STRING, "text_contclassification", "The content of the Author field in the movtar."},
    {PARAM_STRING, "text_contdescription", "The content of the Author field in the movtar."},
    {PARAM_INT32, "jpeg_quality", "the quality of the coded jpegs (0 - 100%)"}    
  };

#define NUMARGS 16

  static int nargs_foreach = sizeof(args_foreach) / sizeof(args_foreach[0]);

  static GParamDef *return_vals = NULL;
  static int nreturn_vals = 0;


  INIT_I18N();

  gimp_install_procedure(PLUGIN_NAME_MOVTAR_ENCODE,
			 "movtar encoding of anim frames",
			 _("This plugin encodes the selected range of animframes into a movtar."
			   " The (optional) audiodata must be in .wav-format (RIFF WAVEfmt )"
			   "The text information fields' content can be chosen freely."),
			 "Gernot Ziegler (gz@lysator.liu.se)",
			 "Gernot Ziegler",
			 gap_movtar_version,
			 N_("<Image>/Video/Encode/Movtar"),
			 "RGB*, INDEXED*, GRAY*",
			 PROC_PLUG_IN,
			 nargs_foreach, nreturn_vals,
			 args_foreach, return_vals);			 
}	/* end movtar_query */


void run (char    *name,
		 int      n_params,
		 GParam  *param,
		 int     *nreturn_vals,
		 GParam **return_vals)
{
#define  MAX_FNAME_LEN  256
#define  MAX_TEXTINFO   512

  long l_range_from;
  long l_range_to;
  long l_dont_recode_frames = 0;
  long l_auto_videoparam = 0;
  char l_videonorm[20];
  long l_jpeg_interlaced = 0;      
  long l_jpeg_quality = JPEG_DEFAULT_QUALITY;      
  char l_vidname[MAX_FNAME_LEN];
  char l_wavname[MAX_FNAME_LEN];
  char l_text_author[MAX_TEXTINFO];
  char l_text_software[MAX_TEXTINFO];
  char l_text_device[MAX_TEXTINFO];
  char l_text_input[MAX_TEXTINFO];
  char l_text_contdescription[MAX_TEXTINFO];
  char l_text_contclassification[MAX_TEXTINFO];

  static GParam values[1];
  GRunModeType run_mode;
  GStatusType status = STATUS_SUCCESS;
  gint32     image_id;
  
  gint32     l_rc;
  char       *l_env;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  l_env = g_getenv("MOVTAR_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N'))
    {
      movtar_debug = TRUE; gap_debug = TRUE;
    }
  }

  run_mode = param[0].data.d_int32;
  l_vidname[0] = '\0';
  l_wavname[0] = '\0';
  l_range_from = 0;
  l_range_to   = 0;
  
  /* Check the non-interactive parameters for validity */
  if(movtar_debug) fprintf(stderr, "\n\ngap_movtar_main: debug name = %s\n", name);
   
  if (run_mode == RUN_NONINTERACTIVE) {
    INIT_I18N();
  } else {
    INIT_I18N_UI();
  }
  
  if (strcmp (name, PLUGIN_NAME_MOVTAR_ENCODE) == 0)
  {
      if (run_mode == RUN_NONINTERACTIVE)
      {
        if (n_params != NUMARGS)
          status = STATUS_CALLING_ERROR;
        else
        {
          strncpy(l_vidname,   param[3].data.d_string, MAX_FNAME_LEN - 1);
          l_vidname[MAX_FNAME_LEN - 1] = '\0';
          l_dont_recode_frames = param[4].data.d_int32;

          strncpy(l_wavname,   param[5].data.d_string, MAX_FNAME_LEN - 1);
          l_wavname[MAX_FNAME_LEN - 1] = '\0';

          l_auto_videoparam  =        param[6].data.d_int32;

          strncpy(l_videonorm,   param[7].data.d_string, MAX_FNAME_LEN - 1);
          l_videonorm[MAX_FNAME_LEN - 1] = '\0';

          l_jpeg_interlaced  =        param[8].data.d_int32;

          strncpy(l_text_author, param[9].data.d_string, MAX_TEXTINFO - 1);
          l_text_author[MAX_TEXTINFO - 1] = '\0';
          strncpy(l_text_software, param[10].data.d_string, MAX_TEXTINFO - 1);
          l_text_software[MAX_TEXTINFO - 1] = '\0';
          strncpy(l_text_device, param[11].data.d_string, MAX_TEXTINFO - 1);
          l_text_device[MAX_TEXTINFO - 1] = '\0';
          strncpy(l_text_input, param[12].data.d_string, MAX_TEXTINFO - 1);
          l_text_input[MAX_TEXTINFO - 1] = '\0';
          strncpy(l_text_contdescription, param[13].data.d_string, MAX_TEXTINFO - 1);
          l_text_contdescription[MAX_TEXTINFO - 1] = '\0';
          strncpy(l_text_contclassification, param[14].data.d_string, MAX_TEXTINFO - 1);
          l_text_contclassification[MAX_TEXTINFO - 1] = '\0';

          l_jpeg_quality  =        param[15].data.d_int32;
        }
      }
      else 
	{
	  if(run_mode != RUN_INTERACTIVE)
	    status = STATUS_CALLING_ERROR;
	}

      if (status == STATUS_SUCCESS)
      {
        image_id    = param[1].data.d_image;

	/* all non-interactive parameters ok, go to encoding */
        l_rc = gap_movtar_encode(run_mode, image_id, 
				 l_range_from, l_range_to,
				 l_vidname, l_dont_recode_frames, 
				 l_wavname, l_auto_videoparam, 
				 l_videonorm, l_jpeg_interlaced, 
				 l_text_author, l_text_software,
				 l_text_device, l_text_input, 
				 l_text_contdescription, l_text_contclassification,
				 l_jpeg_quality
				 );
      }
  }
  else
    status = STATUS_CALLING_ERROR;

  if(l_rc < 0)
    status = STATUS_EXECUTION_ERROR;

  values[0].type = PARAM_STATUS;
  values[0].data.d_status = status;
}

/* ============================================================================
 * p_movtar_encode_dialog
 *
 *   return  0 .. OK 
 *          -1 .. in case of Error or cancel
 * ============================================================================
 */
static long
p_movtar_encode_dialog(t_anim_info *ainfo_ptr,
		       long *range_from, long *range_to,
		       char *filename_movtar, long len_movtar, 
		       long *dont_recode_frames,
		       char *filename_wav,  long len_wav,
		       long *auto_videoparams, char *videonorm,
		       long *interlaced_jpeg,
		       char *text_author,long len_text_author,
		       char *text_software,long len_text_software,
		       char *text_device,long len_text_device,
		       char *text_input,long len_text_input,
		       char *text_contclassification,long len_text_contclassification,
		       char *text_contdescription,long len_text_contdescription,
		       long *jpeg_quality)
{
  static t_arr_arg  argv[19];
  static char *radio_vidnormargs[2]  = {"PAL", "NTSC"};
  
  p_init_arr_arg(&argv[0], WGT_INT_PAIR);
  argv[0].constraint = TRUE;
  argv[0].label_txt = _("From Frame:");
  argv[0].help_txt  = _("first handled frame");
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  
  p_init_arr_arg(&argv[1], WGT_INT_PAIR);
  argv[1].constraint = TRUE;
  argv[1].label_txt = _("To Frame:");
  argv[1].help_txt  = _("last handled frame");
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;

  p_init_arr_arg(&argv[2], WGT_LABEL);
  argv[2].label_txt = _("\nMovtar encoding options\n");

  p_init_arr_arg(&argv[3], WGT_FILESEL);
  argv[3].label_txt = _("Output file name:");
  argv[3].help_txt  = _("Name of the movtar-videofile to write\n"
                        "(should end with extension .movtar)");
  argv[3].text_buf_len = len_movtar;
  argv[3].text_buf_ret = filename_movtar;
  argv[3].entry_width = 250;

  p_init_arr_arg(&argv[4], WGT_TOGGLE);
  argv[4].label_txt = _("Don't recode");
  argv[4].help_txt = _("Don't recode the input frames (for 4:2:2 JPEG only !)");
  argv[4].int_ret = 0;

  p_init_arr_arg(&argv[5], WGT_LABEL);
  argv[5].label_txt = _("\nSound parameters\n");

  p_init_arr_arg(&argv[6], WGT_FILESEL);
  argv[6].label_txt = _("Optional audio file:");
  argv[6].help_txt  = _("Name of an optional WAV-Audiofile to include");
  argv[6].text_buf_len = len_wav;
  argv[6].text_buf_ret = filename_wav;
  argv[6].entry_width = 250;
 
  p_init_arr_arg(&argv[7], WGT_LABEL);
  argv[7].label_txt = _("\nVideo parameters\n");

  p_init_arr_arg(&argv[8], WGT_TOGGLE);
  argv[8].label_txt = _("Autoadjust");
  argv[8].help_txt = _("Automatically adjust video parameters");
  argv[8].int_ret = 1;

  p_init_arr_arg(&argv[9], WGT_OPTIONMENU);
  argv[9].label_txt = _("Video norm:");
  argv[9].help_txt  = _("Chooses the videonorm to be used for encoding (PAL/Europe or NTSC/USA)");
  argv[9].radio_argc  = 2;
  argv[9].radio_argv = radio_vidnormargs;
  argv[9].radio_ret  = 1;

  p_init_arr_arg(&argv[10], WGT_TOGGLE);
  argv[10].label_txt = _("interlaced JPEGs");
  argv[10].help_txt = _("Generate interlaced JPEG (two frames for odd/even lines)");
  argv[10].int_ret = 0;

  p_init_arr_arg(&argv[11], WGT_INT_PAIR);
  argv[11].constraint = TRUE;
  argv[11].label_txt = _("JPEG Quality:");
  argv[11].help_txt  = _("The quality of the generated JPEG frames");
  argv[11].int_min   = 0;
  argv[11].int_max   = 100;
  argv[11].int_ret   = JPEG_DEFAULT_QUALITY;

  p_init_arr_arg(&argv[12], WGT_LABEL);
  argv[12].label_txt = _("\nText info\n");

  p_init_arr_arg(&argv[13], WGT_TEXT);
  argv[13].label_txt = _("Author:");
  argv[13].help_txt  = _("Name of the author of the file content.");
  argv[13].text_buf_len = len_text_author;
  argv[13].text_buf_ret = text_author;

  p_init_arr_arg(&argv[14], WGT_TEXT);
  argv[14].label_txt = _("Software:");
  argv[14].help_txt  = _("Name of the software that produced this movtar.");
  argv[14].text_buf_len = len_text_software;
  argv[14].text_buf_ret = text_software;

  p_init_arr_arg(&argv[15], WGT_TEXT);
  argv[15].label_txt = _("Device:");
  argv[15].help_txt  = _("Name of the device this movtar was recorded with."
                         " (e.g. Iomega Buz, Hauppauge WinTV)");
  argv[16].text_buf_len = len_text_device;
  argv[16].text_buf_ret = text_device;

  p_init_arr_arg(&argv[16], WGT_TEXT);
  argv[16].label_txt = _("Input:");
  argv[16].help_txt  = _("Name of the input this movtar was recorded from (e.g. Composite, S-VHS).");
  argv[16].text_buf_len = len_text_input;
  argv[16].text_buf_ret = text_input;

  p_init_arr_arg(&argv[17], WGT_TEXT);
  argv[17].label_txt = _("Content classification:");
  argv[17].help_txt  = _("A short classification of the content "
                         "(e.g. Documentary, Horror, Cartoon)");
  argv[17].text_buf_len = len_text_contclassification;
  argv[17].text_buf_ret = text_contclassification;

  p_init_arr_arg(&argv[18], WGT_TEXT);
  argv[18].label_txt = _("Content description:");
  argv[18].help_txt  = _("A more detailed description of the content (e.g. Me"
                         " and my friends hanging out in the city)");
  argv[18].text_buf_len = len_text_contdescription;
  argv[18].text_buf_ret = text_contdescription;


  if(0 != p_chk_framerange(ainfo_ptr))   return -1;

  if(TRUE == p_array_dialog(_("GAP Movtar Encode"),
                            _("Settings :"), 
                            19, argv))
  {
      *range_from                   = (long)(argv[0].int_ret);
      *range_to                     = (long)(argv[1].int_ret);
      *dont_recode_frames           = (long)(argv[4].int_ret);
      *auto_videoparams             = (long)(argv[8].int_ret);
      *interlaced_jpeg              = (long)(argv[10].int_ret);
      *jpeg_quality                 = (long)(argv[11].int_ret);
      if ((long)(argv[9].int_ret) == 0) 
	strcpy(videonorm, "TVNORM_PAL"); 
      else
	strcpy(videonorm, "TVNORM_NTSC"); 

      return 0;
  }
  else
  {
      return -1;
  }
}

FILE *p_make_movtar_info(FILE *movtar, const char *wavname,
			 gint32 video_width, gint32 video_height, 
			 gint32 auto_videoparam, const char *videonorm,
			 gint32 jpeg_interlaced, 
			 char *text_author, char *text_software,
			 char *text_device, char *text_input, 
			 char *text_contdescription, char *text_contclassification,
			 gint32 *audio_per_frame_x100, gint32 *wavsize)
{ 
  struct movtarinfotype movtarinfo;
  FILE *inwav; 
  gint32 frames_per_second;

  movtarinfo.version = "0.2";
  movtarinfo.gen_author = text_author;
  movtarinfo.gen_date = "Unknown";
  movtarinfo.gen_software = text_software;
  movtarinfo.gen_jpegkompr = 100;
  movtarinfo.gen_device = text_device;
  movtarinfo.gen_input = text_input;  
  movtarinfo.gen_classification = text_contclassification;
  movtarinfo.gen_description = text_contdescription;
  /* The norm is a number, because it is needed for playing back 
     PAL:0, NTSC:1 */
  if (auto_videoparam)
    {
      switch(video_width)
	{ 
	  case 640: case 320: case 160: movtarinfo.mov_norm = 1; break; /* NTSC */ 
	  case 720: case 360: case 180: movtarinfo.mov_norm = 0; break; /* PAL */
	  default: movtarinfo.mov_norm = VIDEONORM_DEFAULT; break; 
	}
      movtarinfo.mov_jpegnumfield = (video_width > 640) ? 2 : 1;	
    }
  else
    {
      movtarinfo.mov_norm = (strcmp(videonorm, "TVNORM_PAL") == 0) ? 0 : 1;
      movtarinfo.mov_jpegnumfield = jpeg_interlaced ? 2 : 1;
    }

  movtarinfo.mov_width = video_width;
  movtarinfo.mov_height = video_height;
  
  inwav = p_get_wavparams(wavname, wavsize,
			  &movtarinfo.sound_size, 
			  &movtarinfo.sound_rate, 
			  &movtarinfo.sound_stereo);

  /* Write the info block into the movtar. */
  if (movtar_debug) printf("Writing movtar start information\n");
  movtar_write_info(movtar, &movtarinfo);

  /* Calculations for encoding the sound into the movtar, 
     see also movtar_unify fom the buz_tools */
  frames_per_second = (movtarinfo.mov_norm == 0) ? 25 : 30;
  *audio_per_frame_x100 = (100 * movtarinfo.sound_rate * movtarinfo.sound_size/8 * 
			   ((movtarinfo.sound_stereo == TRUE) ? 2 : 1)) / frames_per_second;

  return inwav;
}

/* ============================================================================
 * p_movtar_encode
 *    The main "productive" routine 
 *    movtar encoding of anim frames, based on the buz_tools' encoding routines.
 *    (flattens the images if nedded)
 *    Optional you can provide audio, too 
 *    (wav_audiofile must be provided in that case)
 *
 * returns   value >= 0 if all went ok 
 *           (or -1 on error)
 * ============================================================================
 */
static int
p_movtar_encode(t_anim_info *ainfo_ptr,
		long range_from, long range_to,
		char *vidname, gint32 dont_recode_frames, 
		char *wavname, gint32 auto_videoparam, 
		char *videonorm, gint32 jpeg_interlaced, 
		char *text_author, char *text_software,
		char *text_device, char *text_input, 
		char *text_contdescription, char *text_contclassification,
		gint32 jpeg_quality
		)
{
  gint32     l_tmp_image_id = -1;
  gint32     l_layer_id = -1;
  GDrawable *l_drawable;
  long       l_cur_frame_nr;
  long       l_step, l_begin, l_end;
  int  l_width, l_height;
  gint       l_nlayers;
  gint32    *l_layers_list;
  gdouble    l_percentage, l_percentage_step;
  int        l_rc; 
  FILE *movtar, *inwav = NULL;
  gint32 JPEG_size, datasize;
  gint32 audio_x100;

  char *buffer;
  static struct tarinfotype frameinfo;
  //  static struct tarinfotype frcopyinfo;
  static struct tarinfotype audioinfo;

  gint32 wavsize; /* Data size of the wav file */
  long audio_margin = 8192; 
  long audio_filled_100 = 0;
  gchar databuffer[300000];

  gchar *name_examine;
  
  l_rc = 0;
  l_layer_id = -1; l_width = 0; l_height = 0;
  
  movtar_init(FALSE);
  tarblock_makedefault(&frameinfo, "tja.jpeg", 1024);
  tarblock_makedefault(&audioinfo, "tja.raw", 1024);

  /* create file */
  if (movtar_debug) printf("Creating movtar file.\n");
  movtar = movtar_create(vidname);
  if (movtar == NULL) { perror("Creation of movtar file failed."); exit(1); }

  l_percentage = 0.0;
  if(ainfo_ptr->run_mode == RUN_INTERACTIVE)
    gimp_progress_init("Encoding movtar...");

  l_begin = range_from;
  l_end   = range_to;  
 
  /* special setup (makes it possible to code sequences backwards) */
  if(range_from > range_to)
  {
    l_step  = -1;     /* operate in descending (reverse) order */
    l_percentage_step = 1.0 / ((1.0 + range_from) - range_to);

    if(range_to < ainfo_ptr->first_frame_nr)
      l_begin = ainfo_ptr->first_frame_nr;
    
    if(range_from > ainfo_ptr->last_frame_nr)
      l_end = ainfo_ptr->last_frame_nr;    
  }
  else
  {
    l_step  = 1;      /* operate in ascending order */
    l_percentage_step = 1.0 / ((1.0 + range_to) - range_from);

    if(range_from < ainfo_ptr->first_frame_nr)
      l_begin = ainfo_ptr->first_frame_nr;
    
    if(range_to > ainfo_ptr->last_frame_nr)
      l_end = ainfo_ptr->last_frame_nr;    
  }  
  
  /* start with the first frame to encode */
  l_cur_frame_nr = l_begin;
  while(l_rc >= 0)
  {
    /* build the frame name */
    if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    /* Use the gap functions to generate the frame filename */
    ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                        l_cur_frame_nr,
                                        ainfo_ptr->extension);
    /* can't find the frame ? */
    if(ainfo_ptr->new_filename == NULL) return -1;

    if(l_cur_frame_nr == l_begin) /* setup things first if this is the first frame */
      {
	/* load current frame ... */
	l_tmp_image_id = p_load_image(ainfo_ptr->new_filename);
	if(l_tmp_image_id < 0) return -1;
	/* to determine these parameters */
	l_width = (int) gimp_image_width(l_tmp_image_id);
	l_height = (int) gimp_image_height(l_tmp_image_id);
	    
	/* create the MOVTAR Info file, based on the information
	   stored in the first JPEG */
	inwav = p_make_movtar_info(movtar, wavname,
				   l_width, l_height, 
				   auto_videoparam, videonorm,
				   jpeg_interlaced, 
				   text_author, text_software,
				   text_device, text_input, 
				   text_contdescription, text_contclassification, 
				   &audio_x100, &wavsize);

	/* destroy the tmp image, dont_recode doesn't need it */
	if (dont_recode_frames) gimp_image_delete(l_tmp_image_id);    
      }


    if (dont_recode_frames)
      { 
	JPEG_size = p_get_filesize(ainfo_ptr->new_filename);
	buffer = p_load_file(ainfo_ptr->new_filename);
        if (buffer == NULL) { printf("gap_movtar: Failed opening encoded input frame %s.", 
				     ainfo_ptr->new_filename); return -1; }
	/* generate the file name for the frame to be stored */
	/* store the compressed video frame */    
	/* A quick hack to replace the extension with raw */
	strcpy(frameinfo.name, g_basename(ainfo_ptr->new_filename));
	name_examine = frameinfo.name + strlen(frameinfo.name);
	while (frameinfo.name != name_examine && *name_examine != '.')
	  { *name_examine = '\0'; name_examine--; }
	strcpy(name_examine, ".jpeg");
	sprintf(frameinfo.size, "%o", JPEG_size);
	tarblock_create_checksum(&frameinfo);

	if (movtar_debug) printf("gap_movtar: Writing frame nr. %ld, size %d\n", 
			      l_cur_frame_nr, JPEG_size);
	if (movtar_write(movtar, buffer, JPEG_size, &frameinfo) != TRUE) 
  	    { printf("gap_movtar: Failed in writing a frame."); return -1; }
      }
    else
      {
	if(l_cur_frame_nr != l_begin) /* frame has already been loaded if l_begin */
	  {
	    /* load current frame */
	    l_tmp_image_id = p_load_image(ainfo_ptr->new_filename);
	    if (l_tmp_image_id < 0) return -1;
	  }

	/* Examine the layers of the picture */
	l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);
	if(l_layers_list != NULL)
	  {
	    l_layer_id = l_layers_list[0];
	    g_free (l_layers_list);
	  }

	if(l_nlayers > 1 )
	  {
	    if (movtar_debug) fprintf(stderr, "DEBUG: p_movtar_encode flatten tmp image'\n");	    
	    /* flatten current frame image (reduce to single layer) */
	    l_layer_id = gimp_image_flatten (l_tmp_image_id);
	  }
	
	l_drawable = gimp_drawable_get (l_layer_id);
	if (movtar_debug) fprintf(stderr, "DEBUG: JPEG encoding frame %s\n", 
			       ainfo_ptr->new_filename);
	/* Compress the picture into a JPEG */
    	buffer = p_drawable_encode_jpeg(l_drawable, jpeg_interlaced, 
				        &JPEG_size, jpeg_quality, 0, FALSE, NULL, 0);
	if (movtar_debug) fprintf(stderr, "DEBUG: Ready with encoding, now saving %s\n", 
			       ainfo_ptr->new_filename);
	
	/* store the compressed video frame */    
	/* A quick hack to replace the extension with raw */
	strcpy(frameinfo.name, g_basename(ainfo_ptr->new_filename));
	name_examine = frameinfo.name + strlen(frameinfo.name);
	while (frameinfo.name != name_examine && *name_examine != '.')
	  { *name_examine = '\0'; name_examine--; }
	strcpy(name_examine, ".jpeg");
	sprintf(frameinfo.size, "%o", JPEG_size);
	tarblock_create_checksum(&frameinfo);

	if (movtar_debug) printf("gap_movtar: Writing frame nr. %ld, size %d\n", 
			      l_cur_frame_nr, JPEG_size);
	if (movtar_write(movtar, buffer, JPEG_size, &frameinfo) != TRUE) 
  	    { printf("Failed in writing a frame."); return -1; }
	/* free the compressed JPEG data */
	g_free(buffer);

	/* destroy the tmp image */
	gimp_image_delete(l_tmp_image_id);    
      }

    /* As long as there is a video frame, "fill" the fake audio buffer */
    if (inwav) audio_filled_100 += audio_x100;
    
    /* Now "empty" the fake audio buffer, until it goes under the margin again */
    while ((audio_filled_100 >= (audio_margin * 100)) && (wavsize > 0))
      {
	if (wavsize >= audio_margin)
	  { datasize = fread(databuffer, 1, audio_margin, inwav); 
	  if (datasize != audio_margin)
	    { perror("Warning: Read from wav file failed. (non-critical)\n"); }
	  wavsize -= audio_margin;
	  }  
	else
	  { datasize = fread(databuffer, 1, wavsize, inwav);
	  if (datasize != wavsize)
	    { perror("Warning: Read from wav file failed. (non-critical)\n"); }
	  wavsize = 0;
	  }

	/* create a filename, with the sequence number and 
	   the timestamp info in brackets. Checksum the new tarheader. */

	/* A quick hack to replace the extension with raw */
	name_examine = frameinfo.name + strlen(frameinfo.name);
	while (frameinfo.name != name_examine && *name_examine != '.')
	  { *name_examine = '\0'; name_examine--; }
	strcpy(name_examine, ".raw");
	sprintf(frameinfo.size, "%o", datasize);
	tarblock_create_checksum(&frameinfo);
	if (movtar_debug) printf("Now saving audio frame %s\n", frameinfo.name);
	
	movtar_write(movtar, databuffer, datasize, &frameinfo);
	audio_filled_100 -= audio_margin * 100;
      }
    
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

  movtar_finish_close(movtar);

  return l_rc;  
}

/* ============================================================================
 * gap_movtar_encode
 *   Main routine, calling the interactive dialog if needed, and 
 *   then running the neccessary routines to encode the movtar
 * ============================================================================
 */
gint32 gap_movtar_encode(GRunModeType run_mode, gint32 image_id,
			 long range_from, long range_to,
			 char *vidname, gint32 dont_recode_frames, 
			 char *wavname, gint32 auto_videoparam, 
			 char *videonorm, gint32 jpeg_interlaced, 
			 char *text_author, char *text_software,
			 char *text_device, char *text_input, 
			 char *text_contdescription, char *text_contclassification,
			 gint32 jpeg_quality
			 )
{
  gint32  l_rc;
  char   *l_str;
  
  t_anim_info *ainfo_ptr;

  /* l_ means local */
  /* Pfff .... a lot of variable copying is going on here ... */
  long l_range_from;
  long l_range_to;
  long l_dont_recode_frames;
  long l_auto_videoparam;
  char l_videonorm[20];
  long l_jpeg_interlaced;      
  long l_jpeg_quality;
  char l_vidname[MAX_FNAME_LEN+6];
  char l_wavname[MAX_FNAME_LEN+6];
  char l_text_author[MAX_TEXTINFO];
  char l_text_software[MAX_TEXTINFO];
  char l_text_device[MAX_TEXTINFO];
  char l_text_input[MAX_TEXTINFO];
  char l_text_contdescription[MAX_TEXTINFO];
  char l_text_contclassification[MAX_TEXTINFO];

  l_rc = -1;

  /* Retrieve the animframe info struct */
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      /* Try to construct a fitting filename */
      l_str = p_strdup_del_underscore(ainfo_ptr->basename);
      sprintf(l_vidname, "%s.movtar", l_str);
      g_free(l_str);

      /* No audio file by default */
      l_wavname[0] = '\0';
      l_text_author[0] = '\0';
      l_text_software[0] = '\0';
      l_text_device[0] = '\0';
      l_text_input[0] = '\0';
      l_text_contdescription[0] = '\0';
      l_text_contclassification[0] = '\0';
      
      if(run_mode == RUN_INTERACTIVE)
      {
         l_rc = p_movtar_encode_dialog (ainfo_ptr, 
					&l_range_from, &l_range_to,
					l_vidname, sizeof(l_vidname), 
					&l_dont_recode_frames,
					l_wavname,  sizeof(l_wavname),
					&l_auto_videoparam, l_videonorm, &l_jpeg_interlaced,
					l_text_author, sizeof(l_text_author),
					l_text_software, sizeof(l_text_software),
					l_text_device, sizeof(l_text_device),
					l_text_input, sizeof(l_text_input),
					l_text_contdescription, sizeof(l_text_contdescription),
					l_text_contclassification, sizeof(l_text_contclassification),
					&l_jpeg_quality
					);

      }
      else /* NON-INTERACTIVE */
      {
         l_rc = 0;
         l_range_from    = range_from;
         l_range_to      = range_to;

	 l_dont_recode_frames = dont_recode_frames;
	 l_auto_videoparam = auto_videoparam;
	 l_jpeg_interlaced = jpeg_interlaced;
	 l_jpeg_quality = jpeg_quality;

         if(vidname != NULL)
	   if(*vidname != '\0') 
	     strcpy (l_vidname, vidname);

         if(wavname != NULL)
	   if(*wavname != '\0') 
	     strcpy (l_wavname, wavname);

         if(text_author != NULL)
	   if(*text_author != '\0') 
	     strcpy (l_text_author, text_author);
         if(text_software != NULL)
	   if(*text_software != '\0') 
	     strcpy (l_text_software, text_software);
         if(text_device != NULL)
	   if(*text_device != '\0') 
	     strcpy (l_text_device, text_device);
         if(text_input != NULL)
	   if(*text_input != '\0') 
	     strcpy (l_text_input, text_input);
         if(text_contclassification != NULL)
	   if(*text_contclassification != '\0') 
	     strcpy (l_text_contclassification, text_contclassification);
         if(text_contdescription != NULL)
	   if(*text_contdescription != '\0') 
	     strcpy (l_text_contdescription, text_contdescription);
      }

      if(l_rc >= 0)
      {
	/* All parameters retrieved, start the encoding process */
            l_rc = p_movtar_encode(ainfo_ptr, l_range_from, l_range_to,
					l_vidname,  
					l_dont_recode_frames,
					l_wavname, 
					l_auto_videoparam, l_videonorm, l_jpeg_interlaced,
					l_text_author,
					l_text_software,
					l_text_device, 
					l_text_input, 
					l_text_contclassification,
					l_text_contdescription, l_jpeg_quality);
      }
    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(l_rc);    
}	






