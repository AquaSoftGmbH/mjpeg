/* gap_split.c
 * 1997.11.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains 
 * - gap_split_image
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

/* revision history
 * 1.1.9a;  1999/09/21   hof: bugfix GIMP_RUN_NONINTERACTIVE mode did not work
 * 1.1.8a;  1999/08/31   hof: accept anim framenames without underscore '_'
 * 1.1.5a;  1999/05/08   hof: bugix (dont mix GimpImageType with GimpImageBaseType)
 * 0.96.00; 1998/07/01   hof: - added scale, resize and crop 
 *                              (affects full range == all anim frames)
 *                            - now using gap_arr_dialog.h
 * 0.94.01; 1998/04/28   hof: added flatten_mode to plugin: gap_range_to_multilayer
 * 0.92.00  1998.01.10   hof: bugfix in p_frames_to_multilayer
 *                            layers need alpha (to be raise/lower able) 
 * 0.90.00               first development release
 */
#include "config.h"
 
/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* GIMP includes */
#include "gtk/gtk.h"
//#include "config.h"
#include "libgimp/gimpintl.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_arr_dialog.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */

/* ============================================================================
 * p_split_image
 *
 * returns   value >= 0 if all is ok  return the image_id of 
 *                      the new created image (the last handled anim frame)
 *           (or -1 on error)
 * ============================================================================
 */
static int
p_split_image(t_anim_info *ainfo_ptr,
              char *new_extension,
              gint invers, gint no_alpha)
{
  GimpImageBaseType l_type;
  guint   l_width, l_height;
  GimpRunModeType l_run_mode;
  gint32  l_new_image_id;
  gint    l_nlayers;
  gint32 *l_layers_list;
  gint32  l_src_layer_id;
  gint32  l_cp_layer_id;
  gint    l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  gdouble l_percentage, l_percentage_step;
  char   *l_sav_name;
  char   *l_str;
  gint32  l_rc;
  int     l_idx;
  long    l_layer_idx;

  if(gap_debug) printf("DEBUG: p_split_image inv:%d no_alpha:%d ext:%s\n", (int)invers, (int)no_alpha, new_extension);
  l_rc = -1;
  l_percentage = 0.0;
  l_run_mode  = ainfo_ptr->run_mode;
  if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  { 
    gimp_progress_init( _("Splitting into Frames..."));
  }
 
  l_new_image_id = -1;
  /* get info about the image  */
  l_width  = gimp_image_width(ainfo_ptr->image_id);
  l_height = gimp_image_height(ainfo_ptr->image_id);
  l_type = gimp_image_base_type(ainfo_ptr->image_id);

  l_layers_list = gimp_image_get_layers(ainfo_ptr->image_id, &l_nlayers);
  if(l_layers_list != NULL)
  {
    l_percentage_step = 1.0 / (l_nlayers);

    for(l_idx = 0; l_idx < l_nlayers; l_idx++)
    {
       if(l_new_image_id >= 0)
       {
          /* destroy the tmp image (it was saved to disk before) */
          gimp_image_delete(l_new_image_id);
      }
       
       if(invers == TRUE) l_layer_idx = l_idx;
       else               l_layer_idx = (l_nlayers - 1 ) - l_idx;

       l_src_layer_id = l_layers_list[l_layer_idx];

       /* create new image */
       l_new_image_id =  gimp_image_new(l_width, l_height,l_type);
       if(l_new_image_id < 0)
       {
         l_rc = -1;
         break;
       }

       /* copy the layer */
       l_cp_layer_id = p_my_layer_copy(l_new_image_id,
                                     l_src_layer_id,
                                     100.0,   /* Opacity */
                                     0,       /* NORMAL */
                                     &l_src_offset_x,
                                     &l_src_offset_y);
       /* add the copied layer to current destination image */
        gimp_image_add_layer(l_new_image_id, l_cp_layer_id, 0);
        gimp_layer_set_offsets(l_cp_layer_id, l_src_offset_x, l_src_offset_y);
     
       /* delete alpha channel ? */
       if (no_alpha == TRUE)
       {
           /* add a dummy layer (flatten needs at least 2 layers) */
           l_cp_layer_id = gimp_layer_new(l_new_image_id, "dummy",
                                          4, 4,         /* width, height */
                                          ((l_type * 2 ) + 1),  /* convert from GimpImageBaseType to GimpImageType, and add alpha */
                                          0.0,          /* Opacity full transparent */     
                                          0);           /* NORMAL */
           gimp_image_add_layer(l_new_image_id, l_cp_layer_id, 0);
           gimp_image_flatten (l_new_image_id);
         
       }
       
       
       /* build the name for output image */
       l_str = p_strdup_add_underscore(ainfo_ptr->basename);
       l_sav_name = p_alloc_fname(l_str,
                                  (l_idx +1),       /* start at 1 (not at 0) */
                                  new_extension);
       g_free(l_str);
       if(l_sav_name != NULL)
       {
         /* save with selected save procedure
          * (regardless if image was flattened or not)
          */
          l_rc = p_save_named_image(l_new_image_id, l_sav_name, l_run_mode);
          if(l_rc < 0)
          {
            p_msg_win(ainfo_ptr->run_mode, _("Split Frames: SAVE operation FAILED.\n"
					     "desired save plugin can't handle type\n"
					     "or desired save plugin not available."));
            break;
          }

          l_run_mode  = GIMP_RUN_NONINTERACTIVE;  /* for all further calls */

          /* set image name */
          gimp_image_set_filename (l_new_image_id, l_sav_name);
          
          /* prepare return value */
          l_rc = l_new_image_id;

          g_free(l_sav_name);
       }
       
       /* save as frame */
       
 
       /* show progress bar */
       if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
       { 
         l_percentage += l_percentage_step;
         gimp_progress_update (l_percentage);
       }

      
    }
    g_free (l_layers_list);
  }
  

  return l_rc;  
}	/* end p_split_image */


/* ============================================================================
 * p_split_dialog
 *
 *   return  0 (OK) 
 *          or  -1 in case of Error or cancel
 * ============================================================================
 */
static long
p_split_dialog(t_anim_info *ainfo_ptr, gint *inverse_order, gint *no_alpha, char *extension, gint len_ext)
{
  static t_arr_arg  argv[4];
  gchar   *buf;
  
  buf = g_strdup_printf (_("%s\n%s\n(%s_0001.%s)\n"),
			 _("Make a frame (diskfile) from each Layer"),
			 _("frames are named: base_nr.extension"),
			 ainfo_ptr->basename, extension);
  
  p_init_arr_arg(&argv[0], WGT_LABEL);
  argv[0].label_txt = &buf[0];

  p_init_arr_arg(&argv[1], WGT_TEXT);
  argv[1].label_txt = _("Extension:");
  argv[1].help_txt  = _("extension of resulting frames (is also used to define Fileformat)");
  argv[1].text_buf_len = len_ext;
  argv[1].text_buf_ret = extension;

  p_init_arr_arg(&argv[2], WGT_TOGGLE);
  argv[2].label_txt = _("Inverse Order:");
  argv[2].help_txt  = _("Start frame 0001 at Top Layer");
  argv[2].int_ret   = 0;

  p_init_arr_arg(&argv[3], WGT_TOGGLE);
  argv[3].label_txt = _("Flatten:");
  argv[3].help_txt  = _("Remove Alpha Channel in resulting Frames. Transparent parts are filled with BG color.");
  argv[3].int_ret   = 0;

  if(TRUE == p_array_dialog( _("Split Image into Frames"),
			     _("Split Settings"), 
			     4, argv))
  {
    g_free (buf);
    *inverse_order = argv[2].int_ret;
    *no_alpha      = argv[3].int_ret;
    return 0;
  }
  else
  {
    g_free (buf);
    return -1;
  }
}		/* end p_split_dialog */

/* ============================================================================
 * gap_split_image
 *    Split one (multilayer) image into anim-frames
 *    one frame per layer.
 * ============================================================================
 */
int gap_split_image(GimpRunModeType run_mode,
                      gint32     image_id,
                      gint32     inverse_order,
                      gint32     no_alpha,
                      char      *extension)

{
  gint32  l_new_image_id;
  gint32  l_rc;
  gint32  l_inverse_order;
  gint32  l_no_alpha;
  
  t_anim_info *ainfo_ptr;
  char l_extension[32];

  strcpy(l_extension, ".xcf");

  l_rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      if(ainfo_ptr->frame_cnt != 0)
      {
         p_msg_win(run_mode,
           _("OPERATION CANCELLED.\n"
	     "This image is already an AnimFrame.\n"
	     "Try again on a Duplicate (Image/Duplicate)."));
         return -1;
      }
      else
      {
        if(run_mode == GIMP_RUN_INTERACTIVE)
        {
           l_rc = p_split_dialog (ainfo_ptr, &l_inverse_order, &l_no_alpha, &l_extension[0], sizeof(l_extension));
        }
        else
        {
           l_rc = 0;
           l_inverse_order  =  inverse_order;
           l_no_alpha       =  no_alpha;
           strncpy(l_extension, extension, sizeof(l_extension) -1);
           l_extension[sizeof(l_extension) -1] = '\0';

        }

        if(l_rc >= 0)
        {
           l_new_image_id = p_split_image(ainfo_ptr,
                               l_extension,
                               l_inverse_order,
                               l_no_alpha);
           
           /* create a display for the new created image
            * (it is the first or the last frame of the 
            *  new created animation sequence)
            */                    
           gimp_display_new(l_new_image_id);
           l_rc = l_new_image_id;
        }
      }
    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(l_rc);    
}	/* end   gap_split_image */
