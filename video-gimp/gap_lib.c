/* gap_lib.c
 * 1997.11.18 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * basic anim functions:
 *   Delete, Duplicate, Exchange, Shift 
 *   Next, Prev, First, Last, Goto
 * 
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
 * 1.1.20a; 2000/04/25   hof: new: p_get_video_paste_name p_vid_edit_clear
 * 1.1.17b; 2000/02/27   hof: bug/style fixes
 * 1.1.14a; 1999/12/18   hof: handle .xvpics on fileops (copy, rename and delete)
 *                            new: p_get_frame_nr,
 * 1.1.9a;  1999/09/14   hof: handle frame filenames with framenumbers
 *                            that are not the 4digit style. (like frame1.xcf)
 * 1.1.8a;  1999/08/31   hof: for AnimFrame Filtypes != XCF:
 *                            p_decide_save_as does save INTERACTIVE at 1.st time
 *                            and uses GIMP_RUN_WITH_LAST_VALS for subsequent calls
 *                            (this enables to set Fileformat specific save-Parameters
 *                            at least at the 1.st call, using the save dialog
 *                            of the selected (by gimp_file_save) file_save procedure.
 *                            in NONINTERACTIVE mode we have no access to
 *                            the Fileformat specific save-Parameters
 *          1999/07/22   hof: accept anim framenames without underscore '_'
 *                            (suggested by Samuel Meder)
 * 0.99.00; 1999/03/15   hof: prepared for win/dos filename conventions
 * 0.98.00; 1998/11/30   hof: started Port to GIMP 1.1:
 *                               exchange of Images (by next frame) is now handled in the
 *                               new module: gap_exchange_image.c
 * 0.96.02; 1998/07/30   hof: extended gap_dup (duplicate range instead of singele frame)
 *                            added gap_shift
 * 0.96.00               hof: - now using gap_arr_dialog.h
 * 0.95.00               hof:  increased duplicate frames limit from 50 to 99
 * 0.93.01               hof: fixup bug when frames are not in the current directory
 * 0.90.00;              hof: 1.st (pre) release
 */
#include "config.h"
 
/* SYSTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimpintl.h"
#include "libgimp/gimp.h"

#ifdef G_OS_WIN32
#include <io.h>
#  ifndef S_ISDIR
#    define S_ISDIR(m) ((m) & _S_IFDIR)
#  endif
#  ifndef S_ISREG
#    define S_ISREG(m) ((m) & _S_IFREG)
#  endif
#endif

#ifdef G_OS_WIN32
#include <direct.h>		/* For _mkdir() */
#define mkdir(path,mode) _mkdir(path)
#endif

/* GAP includes */
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_pdb_calls.h"
#include "gap_arr_dialog.h"
#include "gap_exchange_image.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */
 
/* ------------------------------------------ */
/* forward  working procedures */
/* ------------------------------------------ */

static int          p_save_old_frame(t_anim_info *ainfo_ptr);

static int   p_rename_frame(t_anim_info *ainfo_ptr, long from_nr, long to_nr);
static int   p_delete_frame(t_anim_info *ainfo_ptr, long nr);
static int   p_del(t_anim_info *ainfo_ptr, long cnt);
static int   p_decide_save_as(gint32 image_id, char *sav_name);

/* ============================================================================
 * p_alloc_fname_thumbnail
 *   return the thumbnail name (in .xvpics subdir)
 *   for the given filename
 * ============================================================================
 */
char *
p_alloc_fname_thumbnail(char *name)
{
  int   l_len;
  int   l_idx;
  char *l_str;
  
  if(name == NULL)
  {
    return(g_strdup("\0"));
  }

  l_len = strlen(name);
  l_str = g_malloc(l_len+10);
  strcpy(l_str, name);
  if(l_len > 0)
  {
     for(l_idx = l_len -1; l_idx > 0; l_idx--)
     {
        if((name[l_idx] == G_DIR_SEPARATOR) || (name[l_idx] == DIR_ROOT))
	{
	   l_idx++;
	   break;
	}
     }
     sprintf(&l_str[l_idx], ".xvpics%s%s", G_DIR_SEPARATOR_S, &name[l_idx]);      
  }
  if(gap_debug) printf("p_alloc_fname_thumbnail: thumbname=%s:\n", l_str );
  return(l_str);
}

/* ============================================================================
 * p_strdup_*_underscore
 *   duplicate string and if last char is no underscore add one at end.
 *   duplicate string and delete last char if it is the underscore
 * ============================================================================
 */
char *
p_strdup_add_underscore(char *name)
{
  int   l_len;
  char *l_str;
  if(name == NULL)
  {
    return(g_strdup("\0"));
  }

  l_len = strlen(name);
  l_str = g_malloc(l_len+1);
  strcpy(l_str, name);
  if(l_len > 0)
  {
    if (name[l_len-1] != '_')
    {
       l_str[l_len    ] = '_';
       l_str[l_len +1 ] = '\0';
    }
      
  }
  return(l_str);
}

char *
p_strdup_del_underscore(char *name)
{
  int   l_len;
  char *l_str;
  if(name == NULL)
  {
    return(g_strdup("\0"));
  }

  l_len = strlen(name);
  l_str = g_strdup(name);
  if(l_len > 0)
  {
    if (l_str[l_len-1] == '_')
    {
       l_str[l_len -1 ] = '\0';
    }
      
  }
  return(l_str);
}

/* ============================================================================
 * p_msg_win
 *   print a message both to stderr
 *   and to a gimp info window with OK button (only when run INTERACTIVE)
 * ============================================================================
 */

void p_msg_win(GimpRunModeType run_mode, char *msg)
{
  static t_but_arg  l_argv[1];
  int               l_argc;  
  
  l_argv[0].but_txt  = _("OK");
  l_argv[0].but_val  = 0;
  l_argc             = 1;

  if(msg)
  {
    if(*msg)
    {
       fwrite(msg, 1, strlen(msg), stderr);
       fputc('\n', stderr);

       if(run_mode == GIMP_RUN_INTERACTIVE) p_buttons_dialog  (_("GAP Message"), msg, l_argc, l_argv, -1);
    }
  }
}    /* end  p_msg_win */

/* ============================================================================
 * p_file_exists
 *
 * return 0  ... file does not exist, or is not accessable by user,
 *               or is empty,
 *               or is not a regular file
 *        1  ... file exists
 * ============================================================================
 */
int p_file_exists(char *fname)
{
  struct stat  l_stat_buf;
  long         l_len;

  /* File Laenge ermitteln */
  if (0 != stat(fname, &l_stat_buf))
  {
    /* stat error (file does not exist) */
    return(0);
  }
  
  if(!S_ISREG(l_stat_buf.st_mode))
  {
    return(0);
  }
  
  l_len = (long)l_stat_buf.st_size;
  if(l_len < 1)
  {
    /* File is empty*/
    return(0);
  }
  
  return(1);
}	/* end p_file_exists */

/* ============================================================================
 * p_image_file_copy
 *    (copy the imagefile and its thumbnail)
 * ============================================================================
 */
int p_image_file_copy(char *fname, char *fname_copy)
{
   char          *l_from_fname_thumbnail;
   char          *l_to_fname_thumbnail;
   int            l_rc;

   l_from_fname_thumbnail = p_alloc_fname_thumbnail(fname);
   l_to_fname_thumbnail = p_alloc_fname_thumbnail(fname_copy);
   
   l_rc = p_file_copy(fname, fname_copy);
   if((l_from_fname_thumbnail) 
   && (l_to_fname_thumbnail))
   {
     p_file_copy(l_from_fname_thumbnail, l_to_fname_thumbnail);
   }
   
   if(l_from_fname_thumbnail) g_free(l_from_fname_thumbnail);
   if(l_to_fname_thumbnail) g_free(l_to_fname_thumbnail);
   return(l_rc);
}

/* ============================================================================
 * p_file_copy
 * ============================================================================
 */
int p_file_copy(char *fname, char *fname_copy)
{
  FILE	      *l_fp;
  char                     *l_buffer;
  struct stat               l_stat_buf;
  long	       l_len;


  if(gap_debug) printf("p_file_copy src:%s dst:%s\n", fname, fname_copy);

  /* File Laenge ermitteln */
  if (0 != stat(fname, &l_stat_buf))
  {
    fprintf (stderr, "stat error on '%s'\n", fname);
    return -1;
  }
  l_len = (long)l_stat_buf.st_size;

  /* Buffer allocate */
  l_buffer=(char *)g_malloc0((size_t)l_len+1);
  if(l_buffer == NULL)
  {
    fprintf(stderr, "file_copy: calloc error (%ld Bytes not available)\n", l_len);
    return -1;
  }

  /* load File into Buffer */
  l_fp = fopen(fname, "r");		    /* open read */
  if(l_fp == NULL)
  {
    fprintf (stderr, "open(read) error on '%s'\n", fname);
    g_free(l_buffer);
    return -1;
  }
  fread(l_buffer, 1, (size_t)l_len, l_fp);
  fclose(l_fp);
  
  l_fp = fopen(fname_copy, "w");		    /* open write */
  if(l_fp == NULL)
  {
    fprintf (stderr, "file_copy: open(write) error on '%s' \n", fname_copy);
    g_free(l_buffer);
    return -1;
  }

  if(l_len > 0)
  {
     fwrite(l_buffer,  l_len, 1, l_fp);
  }

  fclose(l_fp);
  g_free(l_buffer);
  return 0;           /* all done OK */
}	/* end p_file_copy */


/* ============================================================================
 * p_delete_frame
 * ============================================================================
 */
int p_delete_frame(t_anim_info *ainfo_ptr, long nr)
{
   char          *l_fname;
   char          *l_fname_thumbnail;
   int            l_rc;
   
   l_fname = p_alloc_fname(ainfo_ptr->basename, nr, ainfo_ptr->extension);
   if(l_fname == NULL) { return(1); }

   l_fname_thumbnail = p_alloc_fname_thumbnail(l_fname);
   if(l_fname_thumbnail == NULL) { return(1); }
     
   if(gap_debug) fprintf(stderr, "\nDEBUG p_delete_frame: %s\n", l_fname);
   l_rc = remove(l_fname);
     
   if(gap_debug) fprintf(stderr, "\nDEBUG p_delete_frame: %s\n", l_fname_thumbnail);
   remove(l_fname_thumbnail);
   
   g_free(l_fname);
   g_free(l_fname_thumbnail);
   
   return(l_rc);
   
}    /* end p_delete_frame */

/* ============================================================================
 * p_rename_frame
 * ============================================================================
 */
int p_rename_frame(t_anim_info *ainfo_ptr, long from_nr, long to_nr)
{
   char          *l_from_fname;
   char          *l_to_fname;
   char          *l_from_fname_thumbnail;
   char          *l_to_fname_thumbnail;
   int            l_rc;
   
   l_from_fname = p_alloc_fname(ainfo_ptr->basename, from_nr, ainfo_ptr->extension);
   if(l_from_fname == NULL) { return(1); }
   
   l_to_fname = p_alloc_fname(ainfo_ptr->basename, to_nr, ainfo_ptr->extension);
   if(l_to_fname == NULL) { g_free(l_from_fname); return(1); }
   
   l_from_fname_thumbnail = p_alloc_fname_thumbnail(l_from_fname);
   if(l_from_fname_thumbnail == NULL) { return(1); }
   
   l_to_fname_thumbnail = p_alloc_fname_thumbnail(l_to_fname);
   if(l_to_fname_thumbnail == NULL) { g_free(l_from_fname_thumbnail); return(1); }
   
     
   if(gap_debug) fprintf(stderr, "\nDEBUG p_rename_frame: %s ..to.. %s\n", l_from_fname, l_to_fname);
   l_rc = rename(l_from_fname, l_to_fname);
   if(gap_debug) fprintf(stderr, "\nDEBUG p_rename_frame: %s ..to.. %s\n", l_from_fname_thumbnail, l_to_fname_thumbnail);
   rename(l_from_fname_thumbnail, l_to_fname_thumbnail);
   
   g_free(l_from_fname);
   g_free(l_to_fname);
   g_free(l_from_fname_thumbnail);
   g_free(l_to_fname_thumbnail);
   
   return(l_rc);
   
}    /* end p_rename_frame */


/* ============================================================================
 * p_alloc_basename
 *
 * build the basename from an images name
 * Extension and trailing digits ("0000.xcf") are cut off.
 * return name or NULL (if malloc fails)
 * Output: number contains the integer representation of the stripped off
 *         number String. (or 0 if there was none)
 * ============================================================================
 */
char*  p_alloc_basename(char *imagename, long *number)
{
  char *l_fname;
  char *l_ptr;
  long  l_idx;

  *number = 0;
  if(imagename == NULL) return (NULL);
  l_fname = (char *)g_malloc(strlen(imagename) + 1);
  if(l_fname == NULL)   return (NULL);

  /* copy from imagename */
  strcpy(l_fname, imagename);

  if(gap_debug) fprintf(stderr, "DEBUG p_alloc_basename  source: '%s'\n", l_fname);
  /* cut off extension */
  l_ptr = &l_fname[strlen(l_fname)];
  while(l_ptr != l_fname)
  {
    if((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))  { break; }  /* dont run into dir part */
    if(*l_ptr == '.')                                        { *l_ptr = '\0'; break; }
    l_ptr--;
  }
  if(gap_debug) fprintf(stderr, "DEBUG p_alloc_basename (ext_off): '%s'\n", l_fname);

  /* cut off trailing digits (0000) */
  l_ptr = &l_fname[strlen(l_fname)];
  if(l_ptr != l_fname) l_ptr--;
  l_idx = 1;
  while(l_ptr != l_fname)
  {
    if((*l_ptr >= '0') && (*l_ptr <= '9'))
    { 
      *number += ((*l_ptr - '0') * l_idx);
       l_idx = l_idx * 10;
      *l_ptr = '\0'; 
       l_ptr--; 
    }
    else
    {
      /* do not cut off underscore any longer */
      /*
       * if(*l_ptr == '_')
       * { 
       *    *l_ptr = '\0';
       * }
       */
       break;
    }
  }
  
  if(gap_debug) fprintf(stderr, "DEBUG p_alloc_basename  result:'%s'\n", l_fname);

  return(l_fname);
   
}    /* end p_alloc_basename */



/* ============================================================================
 * p_alloc_extension
 *
 * input: a filename
 * returns: a copy of the filename extension (incl "." )
 *          if the filename has no extension, a pointer to
 *          an empty string is returned ("\0")
 *          NULL if allocate mem for extension failed.
 * ============================================================================
 */
char*  p_alloc_extension(char *imagename)
{
  int   l_exlen;
  char *l_ext;
  char *l_ptr;
  
  l_exlen = 0;
  l_ptr = &imagename[strlen(imagename)];
  while(l_ptr != imagename)
  {
    if((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))  { break; }     /* dont run into dir part */
    if(*l_ptr == '.')                                        { l_exlen = strlen(l_ptr); break; }
    l_ptr--;
  }
  
  l_ext = g_malloc0((size_t)(l_exlen + 1));
  if(l_ext == NULL)
      return (NULL);
  
  
  if(l_exlen > 0)
     strcpy(l_ext, l_ptr);
     
  return(l_ext);
}


/* ============================================================================
 * p_alloc_fname
 *
 * build the name of a frame using "basename_0000.ext"
 * 
 * return name or NULL (if malloc fails)
 * ============================================================================
 */
char*  p_alloc_fname(char *basename, long nr, char *extension)
{
  gchar *l_fname;
  gint   l_leading_zeroes;
  gint   l_len;
  long   l_nr_chk;
  
  if(basename == NULL) return (NULL);
  l_len = (strlen(basename)  + strlen(extension) + 10);
  l_fname = (char *)g_malloc(l_len);

    l_leading_zeroes = TRUE;
    if(nr < 1000)
    {
       /* try to figure out if the frame numbers are in
        * 4-digit style, with leading zeroes  "frame_0001.xcf"
        * or not                              "frame_1.xcf"
        */
       l_nr_chk = nr;

       while(l_nr_chk >= 0)
       {
         /* check if frame is on disk with 4-digit style framenumber */
         g_snprintf(l_fname, l_len, "%s%04ld%s", basename, l_nr_chk, extension);
         if (p_file_exists(l_fname))
         {
            break;
         }

         /* now check for filename without leading zeroes in the framenumber */
         g_snprintf(l_fname, l_len, "%s%ld%s", basename, l_nr_chk, extension);
         if (p_file_exists(l_fname))
         {
            l_leading_zeroes = FALSE;
            break;
         }
         l_nr_chk--;
         
         /* if the frames nr and nr-1  were not found
          * try to check frames 1 and 0
          * to limit down the loop to max 4 cycles
          */
         if((l_nr_chk == nr -2) && (l_nr_chk > 1))
         {
           l_nr_chk = 1;
         }
      }
    }
    else
    {
      l_leading_zeroes = FALSE;
    }

  g_free(l_fname);

  if(l_leading_zeroes) l_fname = g_strdup_printf("%s%04ld%s", basename, nr, extension);
  else                 l_fname = g_strdup_printf("%s%ld%s", basename, nr, extension);

  return(l_fname);
}    /* end p_alloc_fname */




/* ============================================================================
 * p_alloc_ainfo
 *
 * allocate and init an ainfo structure from the given image.
 * ============================================================================
 */
t_anim_info *p_alloc_ainfo(gint32 image_id, GimpRunModeType run_mode)
{
   t_anim_info   *l_ainfo_ptr;

   l_ainfo_ptr = (t_anim_info*)g_malloc(sizeof(t_anim_info));
   if(l_ainfo_ptr == NULL) return(NULL);
   
   l_ainfo_ptr->basename = NULL;
   l_ainfo_ptr->new_filename = NULL;
   l_ainfo_ptr->extension = NULL;
   l_ainfo_ptr->image_id = image_id;
 
   /* get current gimp images name  */   
   l_ainfo_ptr->old_filename = gimp_image_get_filename(image_id);
   if(l_ainfo_ptr->old_filename == NULL)
   {
     l_ainfo_ptr->old_filename = g_strdup("frame_0001.xcf");    /* assign a defaultname */
     gimp_image_set_filename (image_id, l_ainfo_ptr->old_filename);
   }

   l_ainfo_ptr->basename = p_alloc_basename(l_ainfo_ptr->old_filename, &l_ainfo_ptr->frame_nr);
   if(NULL == l_ainfo_ptr->basename)
   {
       p_free_ainfo(&l_ainfo_ptr);
       return(NULL);
   }

   l_ainfo_ptr->extension = p_alloc_extension(l_ainfo_ptr->old_filename);

   l_ainfo_ptr->curr_frame_nr = l_ainfo_ptr->frame_nr;
   l_ainfo_ptr->first_frame_nr = -1;
   l_ainfo_ptr->last_frame_nr = -1;
   l_ainfo_ptr->frame_cnt = 0;
   l_ainfo_ptr->run_mode = run_mode;


   return(l_ainfo_ptr);
 

}    /* end p_init_ainfo */

/* ============================================================================
 * p_dir_ainfo
 *
 * fill in more items into the given ainfo structure.
 * - first_frame_nr
 * - last_frame_nr
 * - frame_cnt
 *
 * to get this information, the directory entries have to be checked
 * ============================================================================
 */
int p_dir_ainfo(t_anim_info *ainfo_ptr)
{
   char          *l_dirname;
   char          *l_dirname_ptr;
   char          *l_ptr;
   char          *l_exptr;
   char          *l_dummy;
   /* int            l_cmp_len;   */
   DIR           *l_dirp;
   struct dirent *l_dp;
   long           l_nr;
   long           l_maxnr;
   long           l_minnr;
   short          l_dirflag;
   char           dirname_buff[1024];

   l_dirname = g_malloc(strlen(ainfo_ptr->basename) +1);
   if(l_dirname == NULL)
     return -1;

   ainfo_ptr->frame_cnt = 0;
   l_dirp = NULL;
   l_minnr = 99999999;
   l_maxnr = 0;
   strcpy(l_dirname, ainfo_ptr->basename);

   l_ptr = &l_dirname[strlen(l_dirname)];
   while(l_ptr != l_dirname)
   {
      if ((*l_ptr == G_DIR_SEPARATOR) || (*l_ptr == DIR_ROOT))
      { 
        *l_ptr = '\0';   /* split the string into dirpath 0 basename */
        l_ptr++;
        break;           /* stop at char behind the slash */
      }
      l_ptr--;
   }
   /* l_cmp_len = strlen(l_ptr); */

   if(gap_debug) fprintf(stderr, "DEBUG p_dir_ainfo: BASENAME:%s\n", l_ptr);
   
   if      (l_ptr == l_dirname)   { l_dirname_ptr = ".";  l_dirflag = 0; }
   else if (*l_dirname == '\0')   { l_dirname_ptr = G_DIR_SEPARATOR_S ; l_dirflag = 1; }
   else                           { l_dirname_ptr = l_dirname; l_dirflag = 2; }

   if(gap_debug) fprintf(stderr, "DEBUG p_dir_ainfo: DIRNAME:%s\n", l_dirname_ptr);
   l_dirp = opendir( l_dirname_ptr );  
   
   if(!l_dirp) fprintf(stderr, "ERROR p_dir_ainfo: cant read directory %s\n", l_dirname_ptr);
   else
   {
     while ( (l_dp = readdir( l_dirp )) != NULL )
     {

       /* if(gap_debug) fprintf(stderr, "DEBUG p_dir_ainfo: l_dp->d_name:%s\n", l_dp->d_name); */
       
       
       /* findout extension of the directory entry name */
       l_exptr = &l_dp->d_name[strlen(l_dp->d_name)];
       while(l_exptr != l_dp->d_name)
       {
         if(*l_exptr == G_DIR_SEPARATOR) { break; }                 /* dont run into dir part */
         if(*l_exptr == '.')       { break; }
         l_exptr--;
       }
       /* l_exptr now points to the "." of the direntry (or to its begin if has no ext) */
       /* now check for equal extension */
       if((*l_exptr == '.') && (0 == strcmp(l_exptr, ainfo_ptr->extension)))
       {
         /* build full pathname (to check if file exists) */
         switch(l_dirflag)
         {
           case 0:
            g_snprintf(dirname_buff, sizeof(dirname_buff), "%s", l_dp->d_name);
            break;
           case 1:
            g_snprintf(dirname_buff, sizeof(dirname_buff), "%c%s",  G_DIR_SEPARATOR, l_dp->d_name);
            break;
           default:
            /* UNIX:  "/dir/file"
             * DOS:   "drv:\dir\file"
             */
            g_snprintf(dirname_buff, sizeof(dirname_buff), "%s%c%s", l_dirname_ptr,  G_DIR_SEPARATOR,  l_dp->d_name);
            break;
         }
         
         if(1 == p_file_exists(dirname_buff)) /* check for regular file */
         {
           /* get basename and frame number of the directory entry */
           l_dummy = p_alloc_basename(l_dp->d_name, &l_nr);
           if(l_dummy != NULL)
           { 
               /* check for files, with equal basename (frames)
                * (length must be greater than basepart+extension
                * because of the frame_nr part "0000")
                */
               if((0 == strcmp(l_ptr, l_dummy))
               && ( strlen(l_dp->d_name) > strlen(l_dummy) + strlen(l_exptr)  ))
               {
                 ainfo_ptr->frame_cnt++;


                 if(gap_debug) fprintf(stderr, "DEBUG p_dir_ainfo:  %s NR=%ld\n", l_dp->d_name, l_nr);

                 if (l_nr > l_maxnr)
                    l_maxnr = l_nr;
                 if (l_nr < l_minnr)
                    l_minnr = l_nr;
               }

               g_free(l_dummy);
           }
         }
       }
     }
     closedir( l_dirp );
   }

  g_free(l_dirname);

  /* set first_frame_nr and last_frame_nr (found as "_0099" in diskfile namepart) */
  ainfo_ptr->last_frame_nr = l_maxnr;
  ainfo_ptr->first_frame_nr = l_minnr;

  return 0;           /* OK */
}	/* end p_dir_ainfo */
 

/* ============================================================================
 * p_free_ainfo
 *
 * free ainfo and its allocated stuff
 * ============================================================================
 */

void p_free_ainfo(t_anim_info **ainfo)
{
  t_anim_info *aptr;
  
  if((aptr = *ainfo) == NULL)
     return;
  
  if(aptr->basename)
     g_free(aptr->basename);
  
  if(aptr->extension)
     g_free(aptr->extension);
   
  if(aptr->new_filename)
     g_free(aptr->new_filename);

  if(aptr->old_filename)
     g_free(aptr->old_filename);
     
  g_free(aptr);
}


/* ============================================================================
 * p_get_frame_nr
 * ============================================================================
 */
long
p_get_frame_nr_from_name(char *fname)
{
  long number;
  int  len;
  char *basename;
  if(fname == NULL) return(-1);
  
  basename = p_alloc_basename(fname, &number);
  if(basename == NULL) return(-1);
  
  len = strlen(basename);
  g_free(basename);
  
  if(number > 0)  return(number);

  if(fname[len]  == '0') return(number);
/*
 *   if(fname[len]  == '_')
 *   {
 *     if(fname[len+1]  == '0') return(TRUE);
 *   }
 */
  return(-1);
}

long
p_get_frame_nr(gint32 image_id)
{
  char *fname;
  long  number;
  
  number = -1;
  fname = gimp_image_get_filename(image_id);
  if(fname)
  {
     number = p_get_frame_nr_from_name(fname);
     g_free(fname);
  }
  return (number);
}

/* ============================================================================
 * p_chk_framechange
 *
 * check if the current frame nr has changed. 
 * useful after dialogs, because the user may have renamed the current image
 * (using save as)
 * while the gap-plugin's dialog was open.
 * return: 0 .. OK
 *        -1 .. Changed (or error occured)
 * ============================================================================
 */
int p_chk_framechange(t_anim_info *ainfo_ptr)
{
  int l_rc;
  t_anim_info *l_ainfo_ptr;
 
  l_rc = -1;
  l_ainfo_ptr = p_alloc_ainfo(ainfo_ptr->image_id, ainfo_ptr->run_mode);
  if(l_ainfo_ptr != NULL)
  {
    if(ainfo_ptr->curr_frame_nr == l_ainfo_ptr->curr_frame_nr )
    { 
       l_rc = 0;
    }
    else
    {
       p_msg_win(ainfo_ptr->run_mode,
         _("OPERATION CANCELLED.\n"
	   "Current frame changed while dialog was open."));
    }
    p_free_ainfo(&l_ainfo_ptr);
  }

  return l_rc;   
}	/* end p_chk_framechange */

/* ============================================================================
 * p_chk_framerange
 *
 * check ainfo struct if there is a framerange (of at least one frame)
 * return: 0 .. OK
 *        -1 .. No frames there (print error)
 * ============================================================================
 */
int p_chk_framerange(t_anim_info *ainfo_ptr)
{
  if(ainfo_ptr->frame_cnt == 0)
  {
     p_msg_win(ainfo_ptr->run_mode,
	       _("OPERATION CANCELLED.\n"
		 "GAP-plugins works only with filenames\n"
		 "that end with _0001.xcf.\n"
		 "==> Rename your image, then try again."));
     return -1;
  }
  return 0;
}	/* end p_chk_framerange */

/* ============================================================================
 * p_gzip
 *   gzip or gunzip the file to a temporary file.
 *   zip == "zip"    compress
 *   zip == "unzip"  decompress
 *   return a pointer to the temporary created (by gzip) file.
 *          NULL  in case of errors
 * ============================================================================
 */
char * p_gzip (char *orig_name, char *new_name, char *zip)
{
  gchar*   l_cmd;
  gchar*   l_tmpname;
  gint     l_rc, l_rc2;

  if(zip == NULL) return NULL;
  
  l_cmd = NULL;
  l_tmpname = new_name;
  
  /* used gzip options:
   *     -c --stdout --to-stdout
   *          Write  output  on  standard  output
   *     -d --decompress --uncompress
   *          Decompress.
   *     -f --force
   *           Force compression or decompression even if the file
   */

  if(*zip == 'u')
  {
    l_cmd = g_strdup_printf("gzip -cfd <\"%s\"  >\"%s\"", orig_name, l_tmpname);
  }
  else
  {
    l_cmd = g_strdup_printf("gzip -cf  <\"%s\"  >\"%s\"", orig_name, l_tmpname);
  }

  if(gap_debug) fprintf(stderr, "system: %s\n", l_cmd);
  
  l_rc = system(l_cmd);
  if(l_rc != 0)
  {
     /* Shift 8 Bits gets Retcode of the executed Program */
     l_rc2 = l_rc >> 8;
     fprintf(stderr, "ERROR system: %s\nreturncodes %d %d", l_cmd, l_rc, l_rc2);
     l_tmpname = NULL;
  }
  g_free(l_cmd);
  return l_tmpname;
  
}	/* end p_gzip */

/* ============================================================================
 * p_decide_save_as
 *   decide what to to, when attempt to save a frame in any image format 
 *  (other than xcf)
 *   Let the user decide if the frame is to save "as it is" or "flattened"
 *   ("as it is" will save only the backround layer in most fileformat types)
 *   The SAVE_AS_MODE is stored , and reused
 *   (without displaying the dialog) on subsequent calls.
 *
 *   return -1  ... CANCEL (do not save)
 *           0  ... save the image (may be flattened)
 * ============================================================================
 */
int p_decide_save_as(gint32 image_id, char *sav_name)
{
  static char *l_msg;


  static char l_save_as_name[80];
  
  static t_but_arg  l_argv[3];
  int               l_argc;  
  int               l_save_as_mode;
  GimpRunModeType      l_run_mode;  

  l_msg = _("You are using a file format != xcf\n"
	    "Save Operations may result\n"
	    "in loss of layer information.");
  /* check if there are SAVE_AS_MODE settings (from privious calls within one gimp session) */
  l_save_as_mode = -1;
  /* g_snprintf(l_save_as_name, sizeof(l_save_as_name), "plug_in_gap_plugins_SAVE_AS_MODE_%d", (int)image_id);*/
  g_snprintf(l_save_as_name, sizeof(l_save_as_name), "plug_in_gap_plugins_SAVE_AS_MODE");
  gimp_get_data (l_save_as_name, &l_save_as_mode);

  if(l_save_as_mode == -1)
  {
    /* no defined value found (this is the 1.st call for this image_id)
     * ask what to do with a 3 Button dialog
     */
    l_argv[0].but_txt  = _("Cancel");
    l_argv[0].but_val  = -1;
    l_argv[1].but_txt  = _("Save Flattened");
    l_argv[1].but_val  = 1;
    l_argv[2].but_txt  = _("Save As Is");
    l_argv[2].but_val  = 0;
    l_argc             = 3;

    l_save_as_mode =  p_buttons_dialog  (_("GAP Question"), l_msg, l_argc, l_argv, -1);
    
    if(gap_debug) fprintf(stderr, "DEBUG: decide SAVE_AS_MODE %d\n", (int)l_save_as_mode);
    
    if(l_save_as_mode < 0) return -1;
    l_run_mode = GIMP_RUN_INTERACTIVE;
  }
  else
  {
    l_run_mode = GIMP_RUN_WITH_LAST_VALS;
  }

  gimp_set_data (l_save_as_name, &l_save_as_mode, sizeof(l_save_as_mode));
   
  
  if(l_save_as_mode == 1)
  {
      gimp_image_flatten (image_id);
  }

  return(p_save_named_image(image_id, sav_name, l_run_mode));
}	/* end p_decide_save_as */



/* ============================================================================
 * p_save_named_image
 * ============================================================================
 */
gint32 p_save_named_image(gint32 image_id, char *sav_name, GimpRunModeType run_mode)
{
  GimpDrawable  *l_drawable;
  gint        l_nlayers;
  gint32     *l_layers_list;
  GimpParam     *l_params;
  gint        l_retvals;

  if(gap_debug) fprintf(stderr, "DEBUG: before   p_save_named_image: '%s'\n", sav_name);

  l_layers_list = gimp_image_get_layers(image_id, &l_nlayers);
  if(l_layers_list == NULL)
     return -1;

  l_drawable =  gimp_drawable_get(l_layers_list[l_nlayers -1]);  /* use the background layer */
  if(l_drawable == NULL)
  {
     fprintf(stderr, "ERROR: p_save_named_image gimp_drawable_get failed '%s' nlayers=%d\n",
                     sav_name, (int)l_nlayers);
     g_free (l_layers_list);
     return -1;
  }
  
  l_params = gimp_run_procedure ("gimp_file_save",
			       &l_retvals,
			       GIMP_PDB_INT32,    run_mode,
			       GIMP_PDB_IMAGE,    image_id,
			       GIMP_PDB_DRAWABLE, l_drawable->id,
			       GIMP_PDB_STRING, sav_name,
			       GIMP_PDB_STRING, sav_name, /* raw name ? */
			       GIMP_PDB_END);

  if(gap_debug) fprintf(stderr, "DEBUG: after    p_save_named_image: '%s' nlayers=%d image=%d drw=%d run_mode=%d\n", sav_name, (int)l_nlayers, (int)image_id, (int)l_drawable->id, (int)run_mode);

  p_gimp_file_save_thumbnail(image_id, sav_name);

  g_free (l_layers_list);
  g_free (l_drawable);


  if (l_params[0].data.d_status == FALSE)
  {
    fprintf(stderr, "ERROR: p_save_named_image  gimp_file_save failed '%s'\n", sav_name);
    g_free(l_params);
    return -1;
  }
  else
  {
    g_free(l_params);
    return image_id;
  }
   
}	/* end p_save_named_image */



/* ============================================================================
 * p_save_named_frame
 *  save frame into temporary image,
 *  on success rename it to desired framename.
 *  (this is done, to avoid corrupted frames on disk in case of
 *   crash in one of the save procedures)
 * ============================================================================
 */
int p_save_named_frame(gint32 image_id, char *sav_name)
{
  GimpParam* l_params;
  char  *l_ext;
  char  *l_tmpname;
  gint   l_retvals;
  int    l_gzip;
  int    l_xcf;
  int    l_rc;

  l_tmpname = NULL;
  l_rc   = -1;
  l_gzip = 0;          /* dont zip */
  l_xcf  = 0;          /* assume no xcf format */
  
  /* check extension to decide if savd file will be zipped */      
  l_ext = p_alloc_extension(sav_name);
  if(l_ext == NULL)  return -1;
  
  if(0 == strcmp(l_ext, ".xcf"))
  { 
    l_xcf  = 1;
  }
  else if(0 == strcmp(l_ext, ".xcfgz"))
  { 
    l_gzip = 1;          /* zip it */
    l_xcf  = 1;
  }
  else if(0 == strcmp(l_ext, ".gz"))
  { 
    l_gzip = 1;          /* zip it */
  }

  /* find a temp name 
   * that resides on the same filesystem as sav_name
   * and has the same extension as the original sav_name 
   */
  l_tmpname = g_strdup_printf("%s.gtmp%s", sav_name, l_ext);
  if(1 == p_file_exists(l_tmpname))
  {
      /* FILE exists: let gimp find another temp name */
      l_params = gimp_run_procedure ("gimp_temp_name",
                                &l_retvals,
                                GIMP_PDB_STRING, 
                                &l_ext[1],
                                GIMP_PDB_END);

      if(l_params[1].data.d_string != NULL)
      {
         l_tmpname = l_params[1].data.d_string;
      }
      g_free(l_params);
  }

  g_free(l_ext);


   if(gap_debug)
   {
     l_ext = g_getenv("GAP_NO_SAVE");
     if(l_ext != NULL)
     {
       fprintf(stderr, "DEBUG: GAP_NO_SAVE is set: save is skipped: '%s'\n", l_tmpname);
       g_free(l_tmpname);  /* free if it was a temporary name */
       return 0;
     }
   }

  if(gap_debug) fprintf(stderr, "DEBUG: before   p_save_named_frame: '%s'\n", l_tmpname);

  if(l_xcf != 0)
  {
    /* save current frame as xcf image
     * xcf_save does operate on the complete image,
     * the drawable is ignored. (we can supply a dummy value)
     */
    l_params = gimp_run_procedure ("gimp_xcf_save",
			         &l_retvals,
			         GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
			         GIMP_PDB_IMAGE,    image_id,
			         GIMP_PDB_DRAWABLE, 0,
			         GIMP_PDB_STRING, l_tmpname,
			         GIMP_PDB_STRING, l_tmpname, /* raw name ? */
			         GIMP_PDB_END);
    if(gap_debug) fprintf(stderr, "DEBUG: after   xcf  p_save_named_frame: '%s'\n", l_tmpname);

    if (l_params[0].data.d_status != FALSE)
    {
       l_rc = image_id;
    }
    g_free(l_params);
  }
  else
  {
     /* let gimp try to save (and detect filetype by extension)
      * Note: the most imagefileformats do not support multilayer
      *       images, and extra channels
      *       the result may not contain the whole imagedata
      *
      * To Do: Should warn the user at 1.st attempt to do this.
      */
      
     l_rc = p_decide_save_as(image_id, l_tmpname);
  } 

  if(l_rc < 0)
  {
     remove(l_tmpname);
     g_free(l_tmpname);  /* free temporary name */
     return l_rc;
  }

  if(l_gzip == 0)
  {
     /* remove sav_name, then rename tmpname ==> sav_name */
     remove(sav_name);
     if (0 != rename(l_tmpname, sav_name))
     {
        /* if tempname is located on another filesystem (errno == EXDEV)
         * rename will not work.
         * so lets try a  copy ; remove sequence
         */
         if(gap_debug) fprintf(stderr, "DEBUG: p_save_named_frame: RENAME 2nd try\n");
         if(0 == p_file_copy(l_tmpname, sav_name))
	 {
	    remove(l_tmpname);
	 }
         else
         {
            fprintf(stderr, "ERROR in p_save_named_frame: cant rename %s to %s\n",
                            l_tmpname, sav_name);
            return -1;
         }
     }
  }
  else
  {
    /* ZIP tmpname ==> sav_name */
    if(NULL != p_gzip(l_tmpname, sav_name, "zip"))
    {
       /* OK zip created compressed file named sav_name
        * now delete the uncompressed l_tempname
        */
       remove(l_tmpname);
    }
  }

  p_gimp_file_save_thumbnail(image_id, sav_name);

  g_free(l_tmpname);  /* free temporary name */

  return l_rc;
   
}	/* end p_save_named_frame */


/* ============================================================================
 * p_save_old_frame
 * ============================================================================
 */
int p_save_old_frame(t_anim_info *ainfo_ptr)
{
  /* (here we should check if image has unsaved changes
   * before save)
   */ 
  if(1)
  {
    return (p_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename));
  }
  else
  {
    return -1;
  }
}	/* end p_save_old_frame */



/* ============================================================================
 * p_load_image
 * load image of any type by filename, and return its image id
 * (or -1 in case of errors)
 * ============================================================================
 */
gint32 p_load_image (char *lod_name)
{
  GimpParam* l_params;
  char  *l_ext;
  char  *l_tmpname;
  gint   l_retvals;
  gint32 l_tmp_image_id;
  int    l_rc;
  
  l_rc = 0;
  l_tmpname = NULL;
  l_ext = p_alloc_extension(lod_name);
  if(l_ext != NULL)
  {
    if((0 == strcmp(l_ext, ".xcfgz"))
    || (0 == strcmp(l_ext, ".gz")))
    { 

      /* find a temp name */
      l_params = gimp_run_procedure ("gimp_temp_name",
			           &l_retvals,
			           GIMP_PDB_STRING, 
			           &l_ext[1],           /* extension */
			           GIMP_PDB_END);

      if(l_params[1].data.d_string != NULL)
      {
         /* try to unzip file, before loading it */
         l_tmpname = p_gzip(lod_name, l_params[1].data.d_string, "unzip");
      }
 
      g_free(l_params);
    }
    else l_tmpname = lod_name;
    g_free(l_ext);
  }

  if(l_tmpname == NULL)
  {
    return -1;
  }


  if(gap_debug) fprintf(stderr, "DEBUG: before   p_load_image: '%s'\n", l_tmpname);

  l_params = gimp_run_procedure ("gimp_file_load",  /* "gimp_xcf_load" */
			       &l_retvals,
			       GIMP_PDB_INT32,  GIMP_RUN_NONINTERACTIVE,
			       GIMP_PDB_STRING, l_tmpname,
			       GIMP_PDB_STRING, l_tmpname, /* raw name ? */
			       GIMP_PDB_END);

  l_tmp_image_id = l_params[1].data.d_int32;
  
  if(gap_debug) fprintf(stderr, "DEBUG: after    p_load_image: '%s' id=%d\n\n",
                               l_tmpname, (int)l_tmp_image_id);

  if(l_tmpname != lod_name)
  {
    remove(l_tmpname);
    g_free(l_tmpname);  /* free if it was a temporary name */
  }
  

  g_free(l_params);
  return l_tmp_image_id;

}	/* end p_load_image */



/* ============================================================================
 * p_load_named_frame
 * load new frame, replacing the existing image
 * file must be of same type and size
 * ============================================================================
 */
int p_load_named_frame (gint32 image_id, char *lod_name)
{
  gint32 l_tmp_image_id;
  int    l_rc;
  
  l_rc = 0;

  l_tmp_image_id = p_load_image(lod_name);
  
  if(gap_debug) fprintf(stderr, "DEBUG: after    p_load_named_frame: '%s' id=%d  new_id=%d\n\n",
                               lod_name, (int)image_id, (int)l_tmp_image_id);

  if(l_tmp_image_id < 0)
      return -1;


   /* replace image_id with the content of l_tmp_image_id */
   if(p_exchange_image (image_id, l_tmp_image_id) < 0)
   {
      /* in case of errors the image will be trashed */
      image_id = -1;
   }

   /* delete the temporary image (old content of the original image) */
   if(gap_debug)  printf("p_load_named_frame: BEFORE gimp_image_delete %d\n", (int)l_tmp_image_id);
   gimp_image_delete(l_tmp_image_id);
   if(gap_debug)  printf("p_load_named_frame: AFTER gimp_image_delete %d\n", (int)l_tmp_image_id);

   /* use the original lod_name */
   gimp_image_set_filename (image_id, lod_name);
   
   /* dont consider image dirty after load */
   gimp_image_clean_all(image_id);
   return image_id;

}	/* end p_load_named_frame */



/* ============================================================================
 * p_replace_image
 *
 * make new_filename of next file to load, check if that file does exist on disk
 * then save current image and replace it, by loading the new_filename
 * ============================================================================
 */
int p_replace_image(t_anim_info *ainfo_ptr)
{
  if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
  ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->frame_nr,
                                      ainfo_ptr->extension);
  if(ainfo_ptr->new_filename == NULL)
     return -1;

  if(0 == p_file_exists(ainfo_ptr->new_filename ))
     return -1;

  if(p_save_old_frame(ainfo_ptr) <0)
  {
    return -1;
  }
  else
  {
    return (p_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
  }
}	/* end p_replace_image */



/* ============================================================================
 * p_del
 *
 * delete cnt frames starting at current
 * all following frames are renamed (renumbered down by cnt) 
 * ============================================================================
 */
int p_del(t_anim_info *ainfo_ptr, long cnt)
{
   long  l_lo, l_hi, l_curr, l_idx;

   if(gap_debug) fprintf(stderr, "DEBUG  p_del\n");

   if(cnt < 1) return -1;

   l_curr =  ainfo_ptr->curr_frame_nr;
   if((1 + ainfo_ptr->last_frame_nr - l_curr) < cnt)
   {
      /* limt cnt to last existing frame */
      cnt = 1 + ainfo_ptr->frame_cnt - l_curr;
   }

   if(cnt >= ainfo_ptr->frame_cnt)
   {
      /* dont want to delete all frames
       * so we have to leave a rest of one frame
       */
      cnt--;
   }

   
   l_idx   = l_curr;
   while(l_idx < (l_curr + cnt))
   {
      p_delete_frame(ainfo_ptr, l_idx);
      l_idx++;
   }
   
   /* rename (renumber) all frames with number greater than current
    */
   l_lo   = l_curr;
   l_hi   = l_curr + cnt;
   while(l_hi <= ainfo_ptr->last_frame_nr)
   {
     if(0 != p_rename_frame(ainfo_ptr, l_hi, l_lo))
     {
        gchar *tmp_errtxt;
	
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld") ,l_hi, l_lo);
        p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	g_free(tmp_errtxt);
        return -1;
     }
     l_lo++;
     l_hi++;
   }

   /* calculate how much frames are left */
   ainfo_ptr->frame_cnt -= cnt;
   ainfo_ptr->last_frame_nr = ainfo_ptr->first_frame_nr + ainfo_ptr->frame_cnt -1;
   
   /* set current position to previous frame (if there is one) */
   if(l_curr > ainfo_ptr->first_frame_nr) 
   { 
     ainfo_ptr->frame_nr = l_curr -1;
   }
   else
   { 
      ainfo_ptr->frame_nr = ainfo_ptr->first_frame_nr;
   }

   /* make filename, then load the new current frame */
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->frame_nr,
                                      ainfo_ptr->extension);

   if(ainfo_ptr->new_filename != NULL)
      return (p_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
   else
      return -1;
      
}        /* end p_del */

/* ============================================================================
 * p_dup
 *
 * all following frames are renamed (renumbered up by cnt) 
 * current frame is duplicated (cnt) times
 * ============================================================================
 */
int p_dup(t_anim_info *ainfo_ptr, long cnt, long range_from, long range_to)
{
   long  l_lo, l_hi;
   long  l_cnt2;
   long  l_step;
   long  l_src_nr, l_src_nr_min, l_src_nr_max;
   char  *l_dup_name;
   char  *l_curr_name;
   gdouble    l_percentage, l_percentage_step;  

   if(gap_debug) fprintf(stderr, "DEBUG  p_dup fr:%d to:%d cnt:%d\n",
                         (int)range_from, (int)range_to, (int)cnt);

   if(cnt < 1) return -1;

   l_curr_name = p_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
   /* save current frame  */   
   p_save_named_frame(ainfo_ptr->image_id, l_curr_name);

   /* use a new name (0001.xcf Konvention) */ 
   gimp_image_set_filename (ainfo_ptr->image_id, l_curr_name);
   g_free(l_curr_name);


   if((range_from <0 ) && (range_to < 0 ))
   {
      /* set range to one single current frame
       * (used for the old non_interactive PDB-interface without range params)
       */
      range_from = ainfo_ptr->curr_frame_nr;
      range_to   = ainfo_ptr->curr_frame_nr;
   }

   /* clip range */
   if(range_from > ainfo_ptr->last_frame_nr)  range_from = ainfo_ptr->last_frame_nr;
   if(range_to   > ainfo_ptr->last_frame_nr)  range_to   = ainfo_ptr->last_frame_nr;
   if(range_from < ainfo_ptr->first_frame_nr) range_from = ainfo_ptr->first_frame_nr;
   if(range_to   < ainfo_ptr->first_frame_nr) range_to   = ainfo_ptr->first_frame_nr;

   if(range_to < range_from)
   {
       /* invers range */
      l_cnt2 = ((range_from - range_to ) + 1) * cnt;
      l_step = -1;
      l_src_nr_max = range_from;
      l_src_nr_min = range_to;
   }
   else
   {
      l_cnt2 = ((range_to - range_from ) + 1) * cnt;  
      l_step = 1;
      l_src_nr_max = range_to;
      l_src_nr_min = range_from;
   }    

   if(gap_debug) fprintf(stderr, "DEBUG  p_dup fr:%d to:%d cnt2:%d l_src_nr_max:%d\n",
                         (int)range_from, (int)range_to, (int)l_cnt2, (int)l_src_nr_max);
   

   l_percentage = 0.0;  
   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   { 
     gimp_progress_init( _("Duplicating frames..."));
   }

   /* rename (renumber) all frames with number greater than current
    */
   l_lo   = ainfo_ptr->last_frame_nr;
   l_hi   = l_lo + l_cnt2;
   while(l_lo > l_src_nr_max)
   {     
     if(0 != p_rename_frame(ainfo_ptr, l_lo, l_hi))
     {
        gchar *tmp_errtxt;
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_lo, l_hi);
        p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	g_free(tmp_errtxt);
        return -1;
     }
     l_lo--;
     l_hi--;
   }


   l_percentage_step = 1.0 / ((1.0 + l_hi) - l_src_nr_max);
   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   { 
      l_percentage += l_percentage_step;
      gimp_progress_update (l_percentage);
   }

   /* copy cnt duplicates */   
   l_src_nr = range_to;
   while(l_hi > l_src_nr_max)
   {
      l_curr_name = p_alloc_fname(ainfo_ptr->basename, l_src_nr, ainfo_ptr->extension);  
      l_dup_name = p_alloc_fname(ainfo_ptr->basename, l_hi, ainfo_ptr->extension);
      if((l_dup_name != NULL) && (l_curr_name != NULL))
      {
         p_image_file_copy(l_curr_name, l_dup_name);
         g_free(l_dup_name);
         g_free(l_curr_name);
      }
      if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
      { 
         l_percentage += l_percentage_step;
         gimp_progress_update (l_percentage);
      }
      
      
      l_src_nr -= l_step;
      if(l_src_nr < l_src_nr_min) l_src_nr = l_src_nr_max;
      if(l_src_nr > l_src_nr_max) l_src_nr = l_src_nr_min;
      
      l_hi--;
   }

   /* restore current position */
   ainfo_ptr->frame_cnt += l_cnt2;
   ainfo_ptr->last_frame_nr = ainfo_ptr->first_frame_nr + ainfo_ptr->frame_cnt -1;

   /* load from the "new" current frame */   
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
   return (p_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
}        /* end p_dup */

/* ============================================================================
 * p_exchg
 *
 * save current frame, exchange its name with destination frame on disk 
 * and reload current frame (now has contents of dest. and vice versa)
 * ============================================================================
 */
int p_exchg(t_anim_info *ainfo_ptr, long dest)
{
   long  l_tmp_nr;
   gchar *tmp_errtxt;

   l_tmp_nr = ainfo_ptr->last_frame_nr + 4;  /* use a free frame_nr for temp name */

   if((dest < 1) || (dest == ainfo_ptr->curr_frame_nr)) 
      return -1;

   if(p_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename) < 0)
      return -1;
   

   /* rename (renumber) frames */
   if(0 != p_rename_frame(ainfo_ptr, dest, l_tmp_nr))
   {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), dest, l_tmp_nr);
        p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	g_free(tmp_errtxt);
        return -1;
   }
   if(0 != p_rename_frame(ainfo_ptr, ainfo_ptr->curr_frame_nr, dest))
   {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), ainfo_ptr->curr_frame_nr, dest);
        p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	g_free(tmp_errtxt);
        return -1;
   }
   if(0 != p_rename_frame(ainfo_ptr, l_tmp_nr, ainfo_ptr->curr_frame_nr))
   {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_tmp_nr, ainfo_ptr->curr_frame_nr);
        p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	g_free(tmp_errtxt);
        return -1;
   }

   /* load from the "new" current frame */   
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
   return (p_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));

}        /* end p_exchg */

/* ============================================================================
 * p_shift
 *
 * all frmaes in the given range are renumbered (shifted)
 * according to cnt:
 *  example:  cnt == 1 :  range before 3, 4, 5, 6, 7
 *                        range after  4, 5, 6, 7, 3
 * ============================================================================
 */
static int
p_shift(t_anim_info *ainfo_ptr, long cnt, long range_from, long range_to)
{
   long  l_lo, l_hi, l_curr, l_dst;
   long  l_upper;
   long  l_shift;
   gchar *l_curr_name;
   gchar *tmp_errtxt;
	
   gdouble    l_percentage, l_percentage_step;  

   if(gap_debug) fprintf(stderr, "DEBUG  p_shift fr:%d to:%d cnt:%d\n",
                         (int)range_from, (int)range_to, (int)cnt);

   if(range_from == range_to) return -1;

   /* clip range */
   if(range_from > ainfo_ptr->last_frame_nr)  range_from = ainfo_ptr->last_frame_nr;
   if(range_to   > ainfo_ptr->last_frame_nr)  range_to   = ainfo_ptr->last_frame_nr;
   if(range_from < ainfo_ptr->first_frame_nr) range_from = ainfo_ptr->first_frame_nr;
   if(range_to   < ainfo_ptr->first_frame_nr) range_to   = ainfo_ptr->first_frame_nr;

   if(range_to < range_from)
   {
      l_lo = range_to;
      l_hi = range_from;
   }
   else
   {
      l_lo = range_from;
      l_hi = range_to;
   }
   
   /* limit shift  amount to number of frames in range */
   l_shift = cnt % (l_hi - l_lo);
   if(gap_debug) fprintf(stderr, "DEBUG  p_shift shift:%d\n",
                         (int)l_shift);
   if(l_shift == 0) return -1;

   l_curr_name = p_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
   /* save current frame  */   
   p_save_named_frame(ainfo_ptr->image_id, l_curr_name);
   g_free(l_curr_name);

   l_percentage = 0.0;  
   if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
   { 
     gimp_progress_init( _("Renumber Framesequence..."));
   }

   /* rename (renumber) all frames (using high numbers)
    */

   l_upper = ainfo_ptr->last_frame_nr +100;
   l_percentage_step = 0.5 / ((1.0 + l_lo) - l_hi);
   for(l_curr = l_lo; l_curr <= l_hi; l_curr++)
   {
     if(0 != p_rename_frame(ainfo_ptr, l_curr, l_curr + l_upper))
     {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_lo, l_hi);
        p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	g_free(tmp_errtxt);
        return -1;
     }
     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     { 
       l_percentage += l_percentage_step;
       gimp_progress_update (l_percentage);
     }
   }

   /* rename (renumber) all frames (using desied destination numbers)
    */
   l_dst = l_lo + l_shift;
   if (l_dst > l_hi) { l_dst -= (l_lo -1); }
   if (l_dst < l_lo) { l_dst += ((l_hi - l_lo) +1); }
   for(l_curr = l_upper + l_lo; l_curr <= l_upper + l_hi; l_curr++)
   {
     if (l_dst > l_hi) { l_dst = l_lo; }
     if(0 != p_rename_frame(ainfo_ptr, l_curr, l_dst))
     {
        tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_lo, l_hi);
        p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	g_free(tmp_errtxt);
        return -1;
     }
     if(ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
     { 
       l_percentage += l_percentage_step;
       gimp_progress_update (l_percentage);
     }

     l_dst ++;     
   }


   /* load from the "new" current frame */   
   if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
   ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
   return (p_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename));
}  /* end p_shift */





/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */ 
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */ 
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */ 


/* ============================================================================
 * gap_next gap_prev
 *
 * store the current Gimp Image to the current anim Frame
 * and load it from the next/prev anim Frame on disk.
 * ============================================================================
 */
int gap_next(GimpRunModeType run_mode, gint32 image_id)
{
  int rc;
  t_anim_info *ainfo_ptr;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    ainfo_ptr->frame_nr = ainfo_ptr->curr_frame_nr + 1;
    rc = p_replace_image(ainfo_ptr);
  
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    
}	/* end gap_next */

int gap_prev(GimpRunModeType run_mode, gint32 image_id)
{
  int rc;
  t_anim_info *ainfo_ptr;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    ainfo_ptr->frame_nr = ainfo_ptr->curr_frame_nr - 1;
    rc = p_replace_image(ainfo_ptr);
  
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    
}	/* end gap_prev */

/* ============================================================================
 * gap_first  gap_last
 *
 * store the current Gimp Image to the current anim Frame
 * and load it from the first/last anim Frame on disk.
 * ============================================================================
 */

int gap_first(GimpRunModeType run_mode, gint32 image_id)
{
  int rc;
  t_anim_info *ainfo_ptr;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      ainfo_ptr->frame_nr = ainfo_ptr->first_frame_nr;
      rc = p_replace_image(ainfo_ptr);
    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    
}	/* end gap_first */

int gap_last(GimpRunModeType run_mode, gint32 image_id)
{
  int rc;
  t_anim_info *ainfo_ptr;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      ainfo_ptr->frame_nr = ainfo_ptr->last_frame_nr;
      rc = p_replace_image(ainfo_ptr);
    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    
}	/* end gap_last */

/* ============================================================================
 * gap_goto
 * 
 * store the current Gimp Image to disk
 * and load it from the anim Frame on disk that has the specified frame Nr.
 * GIMP_RUN_INTERACTIVE:
 *    show dialogwindow where user can enter the destination frame Nr.
 * ============================================================================
 */

int gap_goto(GimpRunModeType run_mode, gint32 image_id, int nr)
{
  int rc;
  t_anim_info *ainfo_ptr;

  long            l_dest;
  gchar          *l_hline;
  gchar          *l_title;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      if(0 != p_chk_framerange(ainfo_ptr))   return -1;

      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
        l_title = g_strdup_printf (_("Goto Frame (%ld/%ld)")
				   , ainfo_ptr->curr_frame_nr
				   , ainfo_ptr->frame_cnt);
        l_hline =  g_strdup_printf (_("Destination Frame Number (%ld  - %ld)")
				    , ainfo_ptr->first_frame_nr
				    , ainfo_ptr->last_frame_nr);

        l_dest = p_slider_dialog(l_title, l_hline, _("Number:"), NULL
                , ainfo_ptr->first_frame_nr
                , ainfo_ptr->last_frame_nr
                , ainfo_ptr->curr_frame_nr
                , TRUE);

	g_free (l_title);
	g_free (l_hline);
                
        if(l_dest < 0)
        {
           /* Cancel button: go back to current frame */
           l_dest = ainfo_ptr->curr_frame_nr;
        }  
        if(0 != p_chk_framechange(ainfo_ptr))
        {
           l_dest = -1;
        }
      }
      else
      {
        l_dest = nr;
      }

      if(l_dest >= 0)
      {
        ainfo_ptr->frame_nr = l_dest;
        rc = p_replace_image(ainfo_ptr);
      }

    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    
}	/* end gap_goto */

/* ============================================================================
 * gap_del
 * ============================================================================
 */
int gap_del(GimpRunModeType run_mode, gint32 image_id, int nr)
{
  int rc;
  t_anim_info *ainfo_ptr;

  long           l_cnt;
  long           l_max;
  gchar         *l_hline;
  gchar         *l_title;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      if(0 != p_chk_framerange(ainfo_ptr))   return -1;
      
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
        l_title = g_strdup_printf (_("Delete Frames (%ld/%ld)")
				   , ainfo_ptr->curr_frame_nr
				   , ainfo_ptr->frame_cnt);
        l_hline = g_strdup_printf (_("Delete Frames from %ld to (number)")
				   , ainfo_ptr->curr_frame_nr);

        l_max = ainfo_ptr->last_frame_nr;
        if(l_max == ainfo_ptr->curr_frame_nr)
        { 
          /* bugfix: the slider needs a maximum > minimum
           *         (if not an error is printed, and
           *          a default range 0 to 100 is displayed)
           */
          l_max++;
        }
        
        l_cnt = p_slider_dialog(l_title, l_hline, _("Number:"), NULL
              , ainfo_ptr->curr_frame_nr
              , l_max
              , ainfo_ptr->curr_frame_nr
              , TRUE);
                
	g_free (l_title);
	g_free (l_hline);
	
        if(l_cnt >= 0)
        {
           l_cnt = 1 + l_cnt - ainfo_ptr->curr_frame_nr;
        } 
        if(0 != p_chk_framechange(ainfo_ptr))
        {
           l_cnt = -1;
        }
      }
      else
      {
        l_cnt = nr;
      }
 
      if(l_cnt >= 0)
      {
         /* delete l_cnt number of frames (-1 CANCEL button) */

         rc = p_del(ainfo_ptr, l_cnt);
      }


    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    

}	/* end gap_del */


/* ============================================================================
 * p_dup_dialog
 *
 * ============================================================================
 */
int p_dup_dialog(t_anim_info *ainfo_ptr, long *range_from, long *range_to)
{
  static t_arr_arg  argv[3];
  gchar            *l_title;

  l_title = g_strdup_printf (_("Duplicate Frames (%ld/%ld)")
			     , ainfo_ptr->curr_frame_nr
			     , ainfo_ptr->frame_cnt);

  p_init_arr_arg(&argv[0], WGT_INT_PAIR);
  argv[0].label_txt = _("From:");
  argv[0].constraint = TRUE;
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[0].help_txt  = _("Source Range starts at this framenumber");

  p_init_arr_arg(&argv[1], WGT_INT_PAIR);
  argv[1].label_txt = _("To:");
  argv[1].constraint = TRUE;
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[1].help_txt  = _("Source Range ends at this framenumber");
    
  p_init_arr_arg(&argv[2], WGT_INT_PAIR);
  argv[2].label_txt = _("N times:");
  argv[2].constraint = FALSE;
  argv[2].int_min   = 0;
  argv[2].int_max   = 99;
  argv[2].int_ret   = 1;
  argv[2].umin      = 0;
  argv[2].umax      = 9999;
  argv[2].help_txt  = _("Copy selected Range n-times  \n(you may type in Values > 99)");
  
  if(TRUE == p_array_dialog(l_title, _("Duplicate Frame Range"),  3, argv))
  { 
    g_free (l_title);
    *range_from = (long)(argv[0].int_ret);
    *range_to   = (long)(argv[1].int_ret);
       return (int)(argv[2].int_ret);
  }
  else
  {
    g_free (l_title);
    return -1;
  }
   

}	/* end p_dup_dialog */


/* ============================================================================
 * gap_dup
 * ============================================================================
 */
int gap_dup(GimpRunModeType run_mode, gint32 image_id, int nr,
            long range_from, long range_to)
{
  int rc;
  t_anim_info *ainfo_ptr;

  long           l_cnt, l_from, l_to;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         if(0 != p_chk_framechange(ainfo_ptr)) { l_cnt = -1; }
         else { l_cnt = p_dup_dialog(ainfo_ptr, &l_from, &l_to); }

         if((0 != p_chk_framechange(ainfo_ptr)) || (l_cnt < 1))
         {
            l_cnt = -1;
         }
                
      }
      else
      {
        l_cnt  = nr;
        l_from = range_from;
        l_to   = range_to;
      }
 
      if(l_cnt > 0)
      {
         /* make l_cnt duplicate frames (on disk) */
         rc = p_dup(ainfo_ptr, l_cnt, l_from, l_to);
      }


    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    

 
}	/* end gap_dup */


/* ============================================================================
 * gap_exchg
 * ============================================================================
 */

int gap_exchg(GimpRunModeType run_mode, gint32 image_id, int nr)
{
  int rc;
  t_anim_info *ainfo_ptr;

  long           l_dest;
  long           l_initial;
  gchar         *l_title;

  rc = -1;
  l_initial = 1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      if(0 != p_chk_framerange(ainfo_ptr))   return -1;
      
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         if(ainfo_ptr->curr_frame_nr < ainfo_ptr->last_frame_nr)
         {
           l_initial = ainfo_ptr->curr_frame_nr + 1;
         }
         else
         {
           l_initial = ainfo_ptr->last_frame_nr; 
         }
         l_title = g_strdup_printf (_("Exchange current Frame (%ld)")
				    , ainfo_ptr->curr_frame_nr);

         l_dest = p_slider_dialog(l_title, 
				  _("With Frame (number)"), 
				  _("Number:"), NULL
				  , ainfo_ptr->first_frame_nr 
				  , ainfo_ptr->last_frame_nr
				  , l_initial
				  , TRUE);
	 g_free (l_title);
				  
         if(0 != p_chk_framechange(ainfo_ptr))
         {
            l_dest = -1;
         }
      }
      else
      {
        l_dest = nr;
      }
 
      if((l_dest >= ainfo_ptr->first_frame_nr ) && (l_dest <= ainfo_ptr->last_frame_nr ))
      {
         /* excange current frames with destination frame (on disk) */
         rc = p_exchg(ainfo_ptr, l_dest);
      }


    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    
}	/* end gap_exchg */

/* ============================================================================
 * p_shift_dialog
 *
 * ============================================================================
 */
int p_shift_dialog(t_anim_info *ainfo_ptr, long *range_from, long *range_to)
{
  static t_arr_arg  argv[3];
  gchar            *l_title;

  l_title = g_strdup_printf (_("Framesequence Shift (%ld/%ld)")
			     , ainfo_ptr->curr_frame_nr
			     , ainfo_ptr->frame_cnt);

  p_init_arr_arg(&argv[0], WGT_INT_PAIR);
  argv[0].label_txt = _("From:");
  argv[0].constraint = TRUE;
  argv[0].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[0].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[0].int_ret   = (gint)ainfo_ptr->curr_frame_nr;
  argv[0].help_txt  = _("Affected Range starts at this framenumber");

  p_init_arr_arg(&argv[1], WGT_INT_PAIR);
  argv[1].label_txt = _("To:");
  argv[1].constraint = TRUE;
  argv[1].int_min   = (gint)ainfo_ptr->first_frame_nr;
  argv[1].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].int_ret   = (gint)ainfo_ptr->last_frame_nr;
  argv[1].help_txt  = _("Affected Range ends at this framenumber");
    
  p_init_arr_arg(&argv[2], WGT_INT_PAIR);
  argv[2].label_txt = _("N-Shift:");
  argv[2].constraint = TRUE;
  argv[2].int_min   = -1 * (gint)ainfo_ptr->last_frame_nr;
  argv[2].int_max   = (gint)ainfo_ptr->last_frame_nr;
  argv[2].int_ret   = 1;
  argv[2].help_txt  = _("Renumber the affected framesequence     \n(numbers are shifted in circle by N)");
  
  if(TRUE == p_array_dialog(l_title, _("Framesequence shift"),  3, argv))
  { 
    g_free (l_title);
    *range_from = (long)(argv[0].int_ret);
    *range_to   = (long)(argv[1].int_ret);
    return (int)(argv[2].int_ret);
  }
  else
  {
    g_free (l_title);
    return 0;
  }
   

}	/* end p_shift_dialog */


/* ============================================================================
 * gap_shift
 * ============================================================================
 */
int gap_shift(GimpRunModeType run_mode, gint32 image_id, int nr,
            long range_from, long range_to)
{
  int rc;
  t_anim_info *ainfo_ptr;

  long           l_cnt, l_from, l_to;

  rc = -1;
  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    if (0 == p_dir_ainfo(ainfo_ptr))
    {
      if(run_mode == GIMP_RUN_INTERACTIVE)
      {
         l_cnt = 1;
         if(0 != p_chk_framechange(ainfo_ptr)) { l_cnt = 0; }
         else { l_cnt = p_shift_dialog(ainfo_ptr, &l_from, &l_to); }

         if((0 != p_chk_framechange(ainfo_ptr)) || (l_cnt == 0))
         {
            l_cnt = 0;
         }
                
      }
      else
      {
        l_cnt  = nr;
        l_from = range_from;
        l_to   = range_to;
      }
 
      if(l_cnt != 0)
      {
         /* shift framesquence by l_cnt frames 
          * (rename all frames in the given range on disk)
          */
         rc = p_shift(ainfo_ptr, l_cnt, l_from, l_to);
      }


    }
    p_free_ainfo(&ainfo_ptr);
  }
  
  return(rc);    

 
}	/* end gap_shift */


/* ============================================================================
 * gap_video_paste Buffer procedures
 * ============================================================================
 */

gchar *
p_get_video_paste_basename(void)
{
  gchar *l_basename;
  
  l_basename = p_gimp_gimprc_query("video-paste-basename");
  if(l_basename == NULL)
  {
     l_basename = g_strdup("gap_video_pastebuffer_");
  }
  return(l_basename);
}

gchar *
p_get_video_paste_dir(void)
{
  gchar *l_dir;
  gint   l_len;
  
  l_dir = p_gimp_gimprc_query("video-paste-dir");
  if(l_dir == NULL)
  {
     l_dir = g_strdup("/tmp");
  }
  
  /* if dir is configured with trailing dir seprator slash
   * then cut it off
   */
  l_len = strlen(l_dir);
  if((l_dir[l_len -1] == G_DIR_SEPARATOR) && (l_len > 1))
  {
     l_dir[l_len -1] = '\0';
  }
  return(l_dir);
}

gchar *
p_get_video_paste_name(void)
{
  gchar *l_dir;
  gchar *l_basename;
  gchar *l_video_name;
  gchar *l_dir_thumb;
	 
  l_dir = p_get_video_paste_dir();
  l_basename = p_get_video_paste_basename();
  l_video_name = g_strdup_printf("%s%s%s", l_dir, G_DIR_SEPARATOR_S, l_basename);
  l_dir_thumb = g_strdup_printf("%s%s%s", l_dir, G_DIR_SEPARATOR_S, ".xvpics");

  mkdir (l_dir_thumb, 0755);

  g_free(l_dir);
  g_free(l_basename);
  g_free(l_dir_thumb);

  if(gap_debug) printf("p_get_video_paste_name: %s\n", l_video_name);
  return(l_video_name); 
}

static gint32
p_clear_or_count_video_paste(gint delete_flag)
{
  gchar *l_dir;
  gchar *l_basename;
  gchar *l_filename;
  gchar *l_fname_thumbnail;
  gint   l_len;
  gint32 l_framecount;
  DIR           *l_dirp;
  struct dirent *l_dp;

  l_dir = p_get_video_paste_dir();
  l_dirp = opendir(l_dir);  
  l_framecount = 0;
  
  if(!l_dirp)
  {
    printf("ERROR p_vid_edit_clear: cant read directory %s\n", l_dir);
    l_framecount = -1;
  }
  else
  {
     l_basename = p_get_video_paste_basename();
     
     l_len = strlen(l_basename);
     while ( (l_dp = readdir( l_dirp )) != NULL )
     {
       if(strncmp(l_basename, l_dp->d_name, l_len) == 0)
       {
          l_filename = g_strdup_printf("%s%s%s", l_dir, G_DIR_SEPARATOR_S, l_dp->d_name);
          if(1 == p_file_exists(l_filename)) /* check for regular file */
	  {
             /* delete all files in the video paste directory
	      * with names matching the basename part
	      */
	     l_framecount++;
	     if(delete_flag)
	     {
        	if(gap_debug) printf("p_vid_edit_clear: remove file %s\n", l_filename);
		remove(l_filename);

		/* also delete thumbnail */
        	l_fname_thumbnail = p_alloc_fname_thumbnail(l_filename);
        	remove(l_fname_thumbnail);
                g_free(l_fname_thumbnail);
	     }
	  }
          g_free(l_filename);
       }
     }
     closedir( l_dirp );
     g_free(l_basename);
   }
   g_free(l_dir);
   return(l_framecount);
}

gint32
p_vid_edit_clear(void)
{
  return(p_clear_or_count_video_paste(TRUE)); /* delete frames */
}

gint32
p_vid_edit_framecount()
{
  return (p_clear_or_count_video_paste(FALSE)); /* delete_flag is off, just count frames */
}


/* ============================================================================
 * gap_vid_edit_copy
 * ============================================================================
 */
gint
gap_vid_edit_copy(GimpRunModeType run_mode, gint32 image_id, long range_from, long range_to)
{
  int rc;
  t_anim_info *ainfo_ptr;
  
  gchar *l_curr_name;
  gchar *l_fname ;
  gchar *l_fname_copy;
  gchar *l_basename;
  gint32 l_frame_nr;
  gint32 l_cnt_range;
  gint32 l_cnt2;
  gint32 l_idx;
  gint32 l_tmp_image_id;

  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr == NULL)
  {
     return (-1);
  }
  rc = 0;

  if((ainfo_ptr->curr_frame_nr >= MIN(range_to, range_from))
  && (ainfo_ptr->curr_frame_nr <= MAX(range_to, range_from)))
  {
    /* current frame is in the affected range
     * so we have to save current frame to file
     */   
    l_curr_name = p_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
    p_save_named_frame(ainfo_ptr->image_id, l_curr_name);
    g_free(l_curr_name);
  }
  
  l_basename = p_get_video_paste_name();
  l_cnt2 = p_vid_edit_framecount();  /* count frames in the video paste buffer */
  l_frame_nr = 1 + l_cnt2;           /* start at one, or continue (append) at end +1 */
  
  l_cnt_range = 1 + MAX(range_to, range_from) - MIN(range_to, range_from);
  for(l_idx = 0; l_idx < l_cnt_range;  l_idx++)
  {
     if(rc < 0)
     {
       break;
     }
     l_fname = p_alloc_fname(ainfo_ptr->basename,
                             MIN(range_to, range_from) + l_idx,
                             ainfo_ptr->extension);
     l_fname_copy = g_strdup_printf("%s%04ld.xcf", l_basename, (long)l_frame_nr);
     
     if(strcmp(ainfo_ptr->extension, ".xcf") == 0)
     {
        rc = p_image_file_copy(l_fname, l_fname_copy);
     }
     else
     {
        /* convert other fileformats to xcf before saving to video paste buffer */
	l_tmp_image_id = p_load_image(l_fname);
	rc = p_save_named_frame(l_tmp_image_id, l_fname_copy);
	gimp_image_delete(l_tmp_image_id);
     }
     g_free(l_fname);
     g_free(l_fname_copy);
     l_frame_nr++;
  }
  p_free_ainfo(&ainfo_ptr);
  return(rc);
}	/* end gap_vid_edit_copy */

/* ============================================================================
 * p_custom_palette_file
 *   write a gimp palette file
 * ============================================================================
 */

static gint p_custom_palette_file(char *filename, guchar *rgb, gint count)
{
  FILE *l_fp;

  l_fp= fopen(filename, "w");
  if (l_fp == NULL)
  {
    return -1;
  }
  
  fprintf(l_fp, "GIMP Palette\n");
  fprintf(l_fp, "# this file will be overwritten each time when video frames are converted to INDEXED\n");

  while (count > 0)
  {
    fprintf(l_fp, "%d %d %d\tUnknown\n", rgb[0], rgb[1], rgb[2]);
    rgb+= 3;
    --count;
  }


  fclose (l_fp);
  return 0;
}	/* end p_custom_palette_file */


/* ============================================================================
 * gap_vid_edit_paste
 * ============================================================================
 */
gint
gap_vid_edit_paste(GimpRunModeType run_mode, gint32 image_id, long paste_mode)
{
#define CUSTOM_PALETTE_NAME "gap_cmap"
  int rc;
  t_anim_info *ainfo_ptr;
  
  gchar *l_curr_name;
  gchar *l_fname ;
  gchar *l_fname_copy;
  gchar *l_basename;
  gint32 l_frame_nr;
  gint32 l_dst_frame_nr;
  gint32 l_cnt2;
  gint32 l_lo, l_hi;
  gint32 l_insert_frame_nr;
  gint32 l_tmp_image_id;
  gint       l_rc;
  GimpParam     *l_params;
  gint        l_retvals;
  GimpImageBaseType  l_orig_basetype;

  l_cnt2 = p_vid_edit_framecount();
  if(gap_debug)
  {
    printf("gap_vid_edit_paste: paste_mode %d found %d frames to paste\n"
           , (int)paste_mode, (int)l_cnt2);
  }
  if (l_cnt2 < 1)
  {
    return(0);  /* video paste buffer is empty */
  }


  ainfo_ptr = p_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr == NULL)
  {
     return (-1);
  }
  if (0 != p_dir_ainfo(ainfo_ptr))
  {
     return (-1);
  }

  rc = 0;

  l_insert_frame_nr = ainfo_ptr->curr_frame_nr;

  if(paste_mode != VID_PASTE_INSERT_AFTER)
  {
    /* we have to save current frame to file */   
    l_curr_name = p_alloc_fname(ainfo_ptr->basename, ainfo_ptr->curr_frame_nr, ainfo_ptr->extension);
    p_save_named_frame(ainfo_ptr->image_id, l_curr_name);
    g_free(l_curr_name);
  }
  
  if(paste_mode != VID_PASTE_REPLACE)
  {
     if(paste_mode == VID_PASTE_INSERT_AFTER)
     {
       l_insert_frame_nr = ainfo_ptr->curr_frame_nr +1;
     }
    
     /* rename (renumber) all frames with number greater (or greater equal)  than current
      */
     l_lo   = ainfo_ptr->last_frame_nr;
     l_hi   = l_lo + l_cnt2;

     if(gap_debug)
     {
       printf("gap_vid_edit_paste: l_insert_frame_nr %d l_lo:%d l_hi:%d\n"
           , (int)l_insert_frame_nr, (int)l_lo, (int)l_hi);
     }
     
     while(l_lo >= l_insert_frame_nr)
     {     
       if(0 != p_rename_frame(ainfo_ptr, l_lo, l_hi))
       {
          gchar *tmp_errtxt;
          tmp_errtxt = g_strdup_printf(_("Error: could not rename frame %ld to %ld"), l_lo, l_hi);
          p_msg_win(ainfo_ptr->run_mode, tmp_errtxt);
	  g_free(tmp_errtxt);
          return -1;
       }
       l_lo--;
       l_hi--;
     }
  }

  l_basename = p_get_video_paste_name();
  l_dst_frame_nr = l_insert_frame_nr;
  for(l_frame_nr = 1; l_frame_nr <= l_cnt2; l_frame_nr++)
  {
     l_fname = p_alloc_fname(ainfo_ptr->basename,
                             l_dst_frame_nr,
                             ainfo_ptr->extension);
     l_fname_copy = g_strdup_printf("%s%04ld.xcf", l_basename, (long)l_frame_nr);

     l_tmp_image_id = p_load_image(l_fname_copy);
     
     /* check size and resize if needed */
     if((gimp_image_width(l_tmp_image_id) != gimp_image_width(image_id))
     || (gimp_image_height(l_tmp_image_id) != gimp_image_height(image_id)))
     {
         GimpParam     *l_params;
         gint        l_retvals;
	 gint32      l_size_x, l_size_y;

         l_size_x = gimp_image_width(image_id);
         l_size_y = gimp_image_height(image_id);
	 if(gap_debug) printf("DEBUG: scale to size %d %d\n", (int)l_size_x, (int)l_size_y);
 
         l_params = gimp_run_procedure ("gimp_image_scale",
			         &l_retvals,
			         GIMP_PDB_IMAGE,    l_tmp_image_id,
			         GIMP_PDB_INT32,    l_size_x,
			         GIMP_PDB_INT32,    l_size_y,
			         GIMP_PDB_END);


     }
     
     /* check basetype and convert if needed */
     l_orig_basetype = gimp_image_base_type(image_id);
     if(gimp_image_base_type(l_tmp_image_id) != l_orig_basetype)
     {
       switch(l_orig_basetype)
       {
           gchar      *l_palette_filename;
           gchar      *l_gimp_dir;
           guchar     *l_cmap;
           gint        l_ncolors;
 
           /* convert tmp image to dest type */
           case GIMP_INDEXED:
             l_cmap = gimp_image_get_cmap(image_id, &l_ncolors);
             if(gap_debug) printf("DEBUG: convert to INDEXED %d colors\n", (int)l_ncolors);

             l_params = gimp_run_procedure ("gimp_gimprc_query",
			                    &l_retvals,
			                    GIMP_PDB_STRING, "gimp_dir",
			                    GIMP_PDB_END);

             l_gimp_dir = g_strdup(l_params[1].data.d_string);
             gimp_destroy_params(l_params, l_retvals);

             l_palette_filename = g_strdup_printf("%s%spalettes%s%s"
                                                 , l_gimp_dir
                                                 , G_DIR_SEPARATOR_S
                                                 , G_DIR_SEPARATOR_S
                                                 , CUSTOM_PALETTE_NAME);
 
             l_rc = p_custom_palette_file(l_palette_filename, l_cmap, l_ncolors);
             if(l_rc == 0)
             {
               l_params = gimp_run_procedure ("gimp_palette_refresh",
			                    &l_retvals,
			                    GIMP_PDB_END);
               gimp_destroy_params(l_params, l_retvals);
               
               l_params = gimp_run_procedure ("gimp_convert_indexed",
			                    &l_retvals,
			                    GIMP_PDB_IMAGE,    l_tmp_image_id,
			                    GIMP_PDB_INT32,    1,               /* dither  value 1== floyd-steinberg */
			                    GIMP_PDB_INT32,    4,               /* palette_type 4 == CUSTOM_PALETTE */
			                    GIMP_PDB_INT32,    l_ncolors,       /* number of colors */
			                    GIMP_PDB_INT32,    0,               /* alpha_dither */
			                    GIMP_PDB_INT32,    0,               /* remove_unused */
			                    GIMP_PDB_STRING,   CUSTOM_PALETTE_NAME, /* name of the custom palette */
			                    GIMP_PDB_END);
               gimp_destroy_params(l_params, l_retvals);
             }
             else
             {
               printf("ERROR: gap_vid_edit_paste: could not save custom palette %s\n", l_palette_filename);
             }
             g_free(l_cmap);
             g_free(l_palette_filename);
             g_free(l_gimp_dir);          
             break;

           case GIMP_GRAY:
             if(gap_debug) printf("DEBUG: convert to GRAY'\n");
             l_params = gimp_run_procedure ("gimp_convert_grayscale",
			                  &l_retvals,
			                  GIMP_PDB_IMAGE,    l_tmp_image_id,
			                  GIMP_PDB_END);
             gimp_destroy_params(l_params, l_retvals);
             break;

           case GIMP_RGB:
             if(gap_debug) printf("DEBUG: convert to RGB'\n");
             l_params = gimp_run_procedure ("gimp_convert_rgb",
			                  &l_retvals,
			                  GIMP_PDB_IMAGE,    l_tmp_image_id,
			                  GIMP_PDB_END);
             gimp_destroy_params(l_params, l_retvals);
             break;

           default:
             printf( "DEBUG: unknown image type\n");
             return -1;
             break;
        }
     }
     rc = p_save_named_frame(l_tmp_image_id, l_fname);
     gimp_image_delete(l_tmp_image_id);
     g_free(l_fname);
     g_free(l_fname_copy);

     l_dst_frame_nr++;
  }
  
  if((rc >= 0)  && (paste_mode != VID_PASTE_INSERT_AFTER))
  {
    /* load from the "new" current frame */   
    if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
    ainfo_ptr->new_filename = p_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->curr_frame_nr,
                                      ainfo_ptr->extension);
    rc = p_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->new_filename);
  }
  
  p_free_ainfo(&ainfo_ptr);
  
  return(rc);
}	/* end gap_vid_edit_paste */
