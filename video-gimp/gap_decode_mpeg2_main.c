/* gap_decode_mpeg2.c */

/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Plugin to load MPEG2 movies (splitted to as GAP_anim_frames)
 *           (C) 2000          Wolfgang Hofer
 *
 * Requires libmpeg2
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

/*******************************************************************
 * USING libmpeg2 WITH THIS PLUGIN:  libmpeg2-1.1.5 can be found   *
 *******************************************************************
 *     mpeg_lib 1.3.1  is available at: 	                   *
 *     http://heroine.linuxbox.com                         	   *
 *******************************************************************/


/*
 * Changelog:
 *
 * 2000/05/27 v1.1.22a: Initial release. [hof] 
 *                       (needs gimp/gap 1.1.22 and libmpeg2 1.1.5
 */




/* UNIX system includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "libgimp/stdplugins-intl.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

/* GAP includes */
#include "gap_arr_dialog.h"
#include "gap_pdb_calls.h"

/* Includes for extra LIBS */
#include "libmpeg2.h"

int gap_debug; /* ==0  ... dont print debug infos */

/*  for i18n  */
static gchar G_GNUC_UNUSED *dummy_entries[] =
{
  N_("<Image>/Video/Split Video to Frames"),
  N_("<Toolbox>/Xtns/Split Video to Frames")
};

static void   query      (void);
static void   run        (char    *name,
                          int      nparams,
                          GParam  *param,
                          int     *nreturn_vals,
                          GParam **return_vals);
static gint32 load_image (char   *filename,
                          gint32  first_frame,
                          gint32  last_frame,
			  char    *basename,
			  gint32  autoload);
static gint32 load_range_dialog(gint32 *first_frame,
                                gint32 *last_frame,
		                char *filename,
		                gint32 len_filename,
				char *basename,
				gint32 len_basename,
				gint32 *autoload);


GPlugInInfo PLUG_IN_INFO =
{
  NULL,    /* init_proc */
  NULL,    /* quit_proc */
  query,   /* query_proc */
  run,     /* run_proc */
};


MAIN ()

static void
query ()
{
  static GParamDef load_args[] =
  {
    { PARAM_INT32, "run_mode", "Interactive, non-interactive" },
    { PARAM_IMAGE, "image", "(unused)"},
    { PARAM_DRAWABLE, "drawable", "(unused)"},
    { PARAM_STRING, "filename", "The name of the file to load" },
    { PARAM_STRING, "raw_filename", "The name entered" },
    { PARAM_INT32, "first_frame", "1st frame to extract (starting at number 1)" },
    { PARAM_INT32, "last_frame", "last frame to extract (use 0 to load all remaining frames)" },
    { PARAM_STRING, "animframe_basename", "The name for the single frames _0001.xcf is added" },
    { PARAM_INT32, "autoload", "TRUE: load 1.st extracted frame on success" },
  };
  static GParamDef load_return_vals[] =
  {
    { PARAM_IMAGE, "image", "Output image" },
  };
  static int nload_args = sizeof (load_args) / sizeof (load_args[0]);
  static int nload_return_vals = sizeof (load_return_vals) / sizeof (load_return_vals[0]);

  static GParamDef ext_args[] =
  {
    { PARAM_INT32, "run_mode", "Interactive, non-interactive" },
    { PARAM_STRING, "filename", "The name of the file to load" },
    { PARAM_STRING, "raw_filename", "The name entered" },
    { PARAM_INT32, "first_frame", "1st frame to extract (starting at number 1)" },
    { PARAM_INT32, "last_frame", "last frame to extract (use 0 to load all remaining frames)" },
    { PARAM_STRING, "animframe_basename", "The name for the single frames _0001.xcf is added" },
    { PARAM_INT32, "autoload", "TRUE: load 1.st extracted frame on success" },
  };
  static int next_args = sizeof (ext_args) / sizeof (ext_args[0]);

  INIT_I18N();

  gimp_install_procedure ("plug_in_gap_decode_mpeg2",
                          "Split MPEG2 movies into animframes and load 1st frame",
                          "Split MPEG2 movies into single frames (image files on disk) and load 1st frame. audio tracks are ignored",
                          "Wolfgang Hofer (hof@hotbot.com)",
                          "Wolfgang Hofer",
                          "2000/05/27",
                          N_("<Image>/Video/Split Video to Frames/MPEG2"),
			  NULL,
                          PROC_PLUG_IN,
                          nload_args, nload_return_vals,
                          load_args, load_return_vals);

  gimp_install_procedure ("extension_gap_decode_mpeg2",
                          "Split MPEG2 movies into animframes and load 1st frame",
                          "Split MPEG2 movies into single frames (image files on disk) and load 1st frame. audio tracks are ignored",
                          "Wolfgang Hofer (hof@hotbot.com)",
                          "Wolfgang Hofer",
                          "2000/01/01",
                          N_("<Toolbox>/Xtns/Split Video to Frames/MPEG2"),
			  NULL,
                          PROC_EXTENSION,
                          next_args, nload_return_vals,
                          ext_args, load_return_vals);
}

static void
run (char    *name,
     int      nparams,
     GParam  *param,
     int     *nreturn_vals,
     GParam **return_vals)
{
  static GParam values[2];
  GRunModeType run_mode;
  gint32 image_ID;
  gint32 first_frame, last_frame;
  gint32 autoload;
  gint32 l_rc;
  char   l_frames_basename[500];
  char   l_filename[500];
  int    l_par;

  image_ID = -1;
  *nreturn_vals = 1;
  *return_vals = values;
  autoload = FALSE;

  values[0].type = PARAM_STATUS;
  values[0].data.d_status = STATUS_CALLING_ERROR;

  run_mode = param[0].data.d_int32;

  if ((strcmp (name, "plug_in_gap_decode_mpeg2") == 0) 
  ||  (strcmp (name, "extension_gap_decode_mpeg2") == 0))
  {
    l_filename[0] = '\0';
    strcpy(&l_frames_basename[0], "frame_");
    
    l_par = 1;
    if(strcmp(name, "plug_in_gap_decode_mpeg2") == 0)
    {
      l_par = 3;
    }

    if( nparams > l_par)
    {
      if(param[l_par + 0].data.d_string != NULL)
      {
	strncpy(l_filename, param[l_par + 0].data.d_string, sizeof(l_filename) -1);
	l_filename[sizeof(l_filename) -1] = '\0';
      }
    }

    l_rc = 0;
    if (run_mode == RUN_NONINTERACTIVE)
    {
       l_filename[0] = '\0';
       
       if(nparams != (l_par + 6))
       {
             l_rc = 1;            /* calling error */
       }
       else
       {
         first_frame = param[l_par + 2].data.d_int32;
         last_frame  = param[l_par + 3].data.d_int32;
	 if(param[l_par + 4].data.d_string != NULL)
	 {
           strcpy(l_frames_basename, param[l_par + 4].data.d_string);
	 }
         autoload = param[l_par + 5].data.d_int32;
       }
    }
    else
    {
       l_rc = load_range_dialog(&first_frame, &last_frame,
                                &l_filename[0], sizeof(l_filename),
                                &l_frames_basename[0], sizeof(l_frames_basename),
                                &autoload);
    }


    if(l_rc == 0)
    {
       if( (last_frame > 0) && (last_frame < first_frame))
       {
         /* swap, because load_image works only from lower to higher frame number */
         image_ID = load_image (&l_filename[0],
                                last_frame, first_frame,
                                &l_frames_basename[0],
                                autoload);
       }
       else
       {
         image_ID = load_image (&l_filename[0],
                               first_frame, last_frame,
                               &l_frames_basename[0],
                               autoload);
       }
    }
    
    if (image_ID != -1)
    {
      *nreturn_vals = 2;
      values[0].data.d_status = STATUS_SUCCESS;
      values[1].type = PARAM_IMAGE;
      values[1].data.d_image = image_ID;
    }
    else
    {
      values[0].data.d_status = STATUS_EXECUTION_ERROR;
    }
  }

}	/* end run */


int p_does_exist(char *fname)
{
  struct stat  l_stat_buf;

  if (0 != stat(fname, &l_stat_buf))
  {
    /* stat error (file does not exist) */
    return(0);
  }
      
  return(1);
}	/* end p_does_exist */

static gint
p_overwrite_dialog(char *filename, gint overwrite_mode)
{
  static  t_but_arg  l_argv[3];
  static  t_arr_arg  argv[1];

  if(p_does_exist(filename))
  {
    if (overwrite_mode < 1)
    {
       l_argv[0].but_txt  = _("Overwrite Frame");
       l_argv[0].but_val  = 0;
       l_argv[1].but_txt  = _("Overwrite All");
       l_argv[1].but_val  = 1;
       l_argv[2].but_txt  = _("Cancel");
       l_argv[2].but_val  = -1;

       p_init_arr_arg(&argv[0], WGT_LABEL);
       argv[0].label_txt = filename;
    
       return(p_array_std_dialog ( _("GAP Question"),
                                   _("File already exists"),
				   1, argv,
				   3, l_argv, -1));
    }
  }
  return (overwrite_mode);
}


static char *
p_build_gap_framename(gint32 frame_nr, char *basename, char *ext)
{
   char *framename;
   framename = g_strdup_printf("%s%04d.%s", basename, (int)frame_nr, ext);
   return(framename);
}

static gint32 
load_image (char   *filename,
            gint32  first_frame,
            gint32  last_frame,
	    char    *basename,
	    gint32   autoload)
{
  GPixelRgn pixel_rgn;
  GDrawable *drawable;
  gint32 first_image_ID;
  gint32 image_ID;
  gint32 layer_ID;
  gchar *temp;
  gint   framenumber, delay;

  mpeg2_t* mp2_file;
  int l_total_astreams;
  int l_audio_channels;
  int l_sample_rate;
  long l_audio_samples;
  long l_num_frames;
      int in_x;		   /* Location in input frame to take picture */
      int in_y;
      int in_w;
      int in_h;
      int out_w;		   /* Dimensions of output_rows */
      int out_h; 

  int   l_stream;
  int   l_total_vstreams;
  gint  l_row;
  gdouble  l_framerate;
  t_video_info *vin_ptr;
 
  guchar **output_rows;
  
  gint wwidth, wheight;
  gint       l_visible;
  gint       l_overwrite_mode;


  gchar *layername = NULL;
  gchar *framename = NULL;
  first_image_ID = -1;
  l_overwrite_mode = 0;


  if(!p_does_exist(filename))
  {
     printf("Error: file %s not found\n", filename);
     return (-1);
  }
  
  /* check  for file compatibility.  Return 1 if compatible. */
  if(1 != mpeg2_check_sig(filename))
  {
     printf("Error: file %s is not compatible to libmpeg2\n", filename);
     return (-1);
  }
  
  framename = p_build_gap_framename(first_frame, basename, "xcf");

  temp = g_malloc (strlen (filename) + 16);
  if (!temp) gimp_quit ();
  g_free (temp);


  
  gimp_progress_init (_("Decoding MPEG Movie..."));

  mp2_file = mpeg2_open(filename);
  if (mp2_file == NULL)
  {
    fprintf (stderr, "error: mpeg2_open failed\n");
    exit (4);      
  }
 
  mpeg2_set_cpus(mp2_file, 1);  /* use one cpu */
  printf("DEBUG: after mpeg2_set_cpus \n");
 

  /* get Audio information about the opened mpeg file */
 if( mpeg2_has_audio(mp2_file) )
 {
   l_total_astreams = mpeg2_total_astreams(mp2_file);  /* Number of multiplexed audio streams */
   for(l_stream = 0; l_stream < l_total_astreams; l_stream++)
   {
     l_audio_channels = mpeg2_audio_channels(mp2_file, l_stream);
     l_sample_rate  =   mpeg2_sample_rate(mp2_file, l_stream);
     l_audio_samples =  mpeg2_audio_samples(mp2_file, l_stream); /* Total length for stream #1 */

     printf("%s Audio stream %d  channels:%d samplerate:%d samples:%d\n",
             filename, (int)l_stream, (int)l_audio_channels, (int)l_sample_rate, (int)l_audio_samples);

#ifdef COMMENT_CODE_UNDER_CONSTRUCTION
      mpeg2_set_sample(mp2_file, long sample, int stream);    /* Seek */
      /* long mpeg2_get_sample(mp2_file, int stream); */   /* Tell current position */

      /* read the audio data */
	int mpeg2_read_audio(mp2_file, 
                  float *output_f,      // Pointer to pre-allocated buffer of floats
                  short *output_i,      // Pointer to pre-allocated buffer if int16's
                  int channel,          // Channel to decode
                  long samples,         // Number of samples to decode
                  l_stream);          // Stream containing the channel
      /* TODO: optional write audio data to wav file(s) */
#endif
    }
  }



  /* get Video information about the opened mpeg file */
  if(! mpeg2_has_video(mp2_file) )
  {
    printf("%s has NO video streams\n", filename);
    exit (2);
  }

  l_total_vstreams = mpeg2_total_vstreams(mp2_file);            /* Number of multiplexed video streams */
  printf("%s has %d video streams\n", filename, (int)l_total_vstreams);
  
  l_stream = 0;
  
  wwidth = mpeg2_video_width(mp2_file, l_stream);
  wheight = mpeg2_video_height(mp2_file, l_stream);
  l_num_frames = mpeg2_video_frames(mp2_file, l_stream);    /* Total length */

  l_framerate = (gdouble)mpeg2_frame_rate(mp2_file, l_stream);       /* Frames/sec */
  delay = 1000 / l_framerate;

  printf("DEBUG: Video infos for %s  vid_streams:%d, w:%d h:%d frames:%d rate:%f\n",
                filename, (int)l_total_vstreams, (int)wwidth, (int)wheight, (int)l_num_frames ,(float)l_framerate);
  
  /* set framerate in GAP videoinfo file */
  vin_ptr = p_get_video_info(basename);
  if(vin_ptr)
  {
    vin_ptr->framerate = l_framerate;
    p_set_video_info(vin_ptr ,basename);
    g_free(vin_ptr);
  }

  /* operate on the full image size for all frames */
  in_x = 0;
  in_y = 0;
  in_w = wwidth;
  in_h = wheight;
  out_w = wwidth;
  out_h = wheight; 
  
  /* allocate row pointers and databuffer for each row */
  output_rows = g_malloc(wheight * sizeof(guchar *));
  for(l_row = 0; l_row < wheight; l_row++)
  {
    output_rows[l_row] = g_malloc((4 * sizeof(guchar))* wwidth);
  }

  
  l_visible = TRUE;   /* loaded layer should be visible */
  framenumber = 1;
  while (framenumber <= l_num_frames)
  {
    g_free(framename);
    framename = p_build_gap_framename(framenumber, basename, "xcf");


    if (last_frame > 0)
    {
       gimp_progress_update ((gdouble)framenumber / (gdouble)last_frame );
    }
    else
    {
       gimp_progress_update ((gdouble)framenumber / (gdouble)l_num_frames );
    }


    /* stop if desired range is loaded 
     * (negative last_frame ==> load until last frame)
     */
    if ((framenumber > last_frame ) && (last_frame > 0)) break;

    /* ignore frames before first_frame */    
    if (framenumber >= first_frame )
    {
      /* seeking for current video frame */
      printf("DEBUG: before mpeg2_set_frame\n");
      mpeg2_set_frame(mp2_file, (long)framenumber, l_stream); /* seek */
      printf("DEBUG: after mpeg2_set_frame\n");
      /* l_vid_pos = mpeg2_get_frame(mp2_file, l_stream); */  /* Tell current position */


      /* read video data for the current frame
         conveniently for us, libmpeg2
	 is able to provide the imagedata in RGBA 8bit buffer
	 ready for the use in gimp procedures without further
	 transitions.
       */

output_rows = g_malloc(wheight * ((4 * sizeof(guchar))* wwidth));

      printf("DEBUG: before mpeg2_read_frame\n");
       mpeg2_read_frame(mp2_file, 
                output_rows,    /* Array of pointers to the start of each output row */
                in_x,
                in_y, 
                in_w, 
                in_h, 
                out_w,
                out_h, 
                MPEG2_RGBA8888,  /* use colormodel of RGBA 8bit each */
                l_stream);
      printf("DEBUG: after mpeg2_read_frame\n");
/*
      The video decoding works like a camcorder taking an illegal copy of a movie screen.
      The decoder "sees" a region of the movie screen defined 
	by in_x, in_y, in_w, in_h 
      and transfers it to the frame buffer defined
	by **output_rows.
      The input values must be within the boundaries given by mpeg2_video_width and
      mpeg2_video_height. The size of the frame buffer is defined 
	by out_w, out_h.
      Although the input dimensions are constrained, the frame buffer can be any size.

      color_model defines which color model the picture should be decoded to
      and the possible values are given in libmpeg2.h. 
      The frame buffer pointed to by output_rows must have enough memory allocated to store
      the color model you select.
*/



       image_ID = gimp_image_new (wwidth, wheight, RGB);
       gimp_image_set_filename (image_ID, framename);

       if(framenumber == first_frame)
       {
         first_image_ID = image_ID;
       }
    
       if (delay > 0)
         layername = g_strdup_printf("Frame %d (%dms)",
                 framenumber, delay);
       else
         layername = g_strdup_printf("Frame %d",
                 framenumber);

       layer_ID = gimp_layer_new (image_ID, layername,
                                  wwidth,
                                  wheight,
                                  RGBA_IMAGE, 100, NORMAL_MODE);
       g_free(layername);
       gimp_image_add_layer (image_ID, layer_ID, 0);
       gimp_layer_set_visible(layer_ID, l_visible);
       drawable = gimp_drawable_get (layer_ID);


       /* copy data rows */
       for(l_row = 0; l_row < wheight; l_row++)
       {
         gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, l_row,
                            drawable->width, l_row+1, TRUE, FALSE);
         gimp_pixel_rgn_set_rect (&pixel_rgn, output_rows[l_row], 0, l_row,
                                wwidth, l_row+1);
       }
       gimp_drawable_flush (drawable);
       gimp_drawable_detach (drawable);

       /* save each image as frame to disk */       
       {
         GParam* l_params;
	 gint    l_retvals;

         l_overwrite_mode = p_overwrite_dialog(framename, l_overwrite_mode);
	 
         if (l_overwrite_mode < 0)
	 {
	    /* fake high franenumber (break at next loop) if CANCEL was pressed in the Overwrite dialog */
            framenumber = l_num_frames + 2;
	    autoload = FALSE;    /* drop image and do not open on CANCEL */
	 }
	 else
	 {
           l_params = gimp_run_procedure ("gimp_xcf_save",
			           &l_retvals,
			           PARAM_INT32,    RUN_NONINTERACTIVE,
			           PARAM_IMAGE,    image_ID,
			           PARAM_DRAWABLE, 0,
			           PARAM_STRING, framename,
			           PARAM_STRING, framename, /* raw name ? */
			           PARAM_END);
           p_gimp_file_save_thumbnail(image_ID, framename);
	 }

       }

       if((first_image_ID != image_ID) || ( !autoload))
       {
           gimp_image_delete(image_ID);
       }

    }

    framenumber++;  
  }

  mpeg2_close(mp2_file);
     
  gimp_progress_update (1.0);

  if(output_rows)
  {
    for(l_row = 0; l_row < wheight; l_row++)
    {
      g_free(output_rows[l_row]);
    }
    g_free(output_rows);
  }

  if(autoload)
  {
    gimp_display_new(first_image_ID);
    return first_image_ID;
  }
  return -1;

}

static gint32
load_range_dialog(gint32 *first_frame,
                  gint32 *last_frame,
		  char *filename,
		  gint32 len_filename,
		  char *basename,
		  gint32 len_basename,
		  gint32 *autoload)
{
#define ARGC_DIALOG 6
  static t_arr_arg  argv[ARGC_DIALOG];

  p_init_arr_arg(&argv[0], WGT_FILESEL);
  argv[0].label_txt = _("Video");
  argv[0].help_txt  = _("Name of the MPEG2 videofile to READ.\n"
                        "Frames are extracted from the videofile\n"
			"and written to seperate diskfiles.\n"
			"Audiotracks in the videofile are ignored.");
  argv[0].text_buf_len = len_filename;
  argv[0].text_buf_ret = filename;
  argv[0].entry_width = 250;

  p_init_arr_arg(&argv[1], WGT_INT_PAIR);
  argv[1].label_txt = _("From:");
  argv[1].help_txt  = _("Framenumber of 1st frame to extract");
  argv[1].constraint = FALSE;
  argv[1].int_min    = 1;
  argv[1].int_max    = 9999;
  argv[1].int_ret    = 1;
  argv[1].umin       = 0;
  argv[1].entry_width = 80;
  
  p_init_arr_arg(&argv[2], WGT_INT_PAIR);
  argv[2].label_txt = _("To:");
  argv[2].help_txt  = _("Framenumber of last frame to extract");
  argv[2].constraint = FALSE;
  argv[2].int_min    = 1;
  argv[2].int_max    = 9999;
  argv[2].int_ret    = 9999;
  argv[2].umin       = 0;
  argv[2].entry_width = 80;
  
  p_init_arr_arg(&argv[3], WGT_FILESEL);
  argv[3].label_txt = _("Framenames:");
  argv[3].help_txt  = _("Basename for the AnimFrames to write on disk\n"
                        "(framenumber and .xcf is added)");
  argv[3].text_buf_len = len_basename;
  argv[3].text_buf_ret = basename;
  argv[3].entry_width = 250;
  
  p_init_arr_arg(&argv[4], WGT_TOGGLE);
  argv[4].label_txt = _("Open");
  argv[4].help_txt  = _("Open the 1st one of the extracted frames");
  argv[4].int_ret    = 1;
  
  p_init_arr_arg(&argv[5], WGT_LABEL_LEFT);
  argv[5].label_txt = _("\nWARNING: Do not attempt to split other files than MPEG2 videos.\n"
                         "Before you proceed, you should save all open images.");
  
  if(TRUE == p_array_dialog (_("Split MPEG2 Video to Frames"), 
			     _("Select Frame Range"), 
			     ARGC_DIALOG, argv))
  {
     *first_frame = (long)(argv[1].int_ret);
     *last_frame  = (long)(argv[2].int_ret);
     *autoload    = (long)(argv[4].int_ret);
     return (0);    /* OK */
  }
  else
  {
     return -1;     /* Cancel */
  }
   

}
