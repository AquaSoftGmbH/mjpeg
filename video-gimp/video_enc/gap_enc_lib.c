/* gap_enc_audio.c
 *
 *  GAP common encoder tool procedures
 *  with no dependencies to external libraries.
 *
 */

 
/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include <stdplugins-intl.h>
#include <libgimp/gimp.h>

/* GAP includes */
#include <gap_arr_dialog.h>


/*************************************************************
 *          TOOL FUNCTIONS                                   *
 *************************************************************/

/* ============================================================================
 * p_info_win
 *   print a message both to stderr
 *   and to a gimp info window with OK button (only when run INTERACTIVE)
 *in: run_mode: If RUN_INTERACTIVE, a message dialog is generated, too.
 *    msg: The message to write. 
 *    button_text: The text on the button.
 * ============================================================================
 */

void p_info_win(GRunModeType run_mode, char *msg, char *button_text)
{
  static t_but_arg  l_argv[1];
  int               l_argc;  
  
  l_argv[0].but_txt  = button_text;
  l_argv[0].but_val  = 0;
  l_argc             = 1;

  if(msg)
  {
    if(*msg)
    {
       fwrite(msg, 1, strlen(msg), stderr);
       fputc('\n', stderr);

       if(run_mode == RUN_INTERACTIVE) p_buttons_dialog  ("GAP Message", msg, l_argc, l_argv, -1);
    }
  }
}    /* end  p_info_win */


/* ============================================================================
 * p_get_filesize
 *   get the filesize of a file.
 *in: fname: The filename of the file to check.
 *returns: The filesize.
 * ============================================================================
 */

gint32
p_get_filesize(char *fname)
{
  struct stat  stat_buf;
 
  if (0 != stat(fname, &stat_buf))
  {
    printf ("stat error on '%s'\n", fname);
    return(0);
  }
  
  return((gint32)(stat_buf.st_size));
}


/* --------------------------------------------------
   p_load_file
     Load a file into a memory buffer
   in: fname: The name of the file to load
   returns: A pointer to the allocated buffer with the file content.
   --------------------------------------------------
 */


char *
p_load_file(char *fname)
{
  FILE	      *fp;
  char        *l_buff_ptr;
  long	       len;

  /* File Laenge ermitteln */
  len = p_get_filesize(fname);
  if (len < 1)
  {
    return(NULL);
  }

  /* Buffer allocate */
  l_buff_ptr=(char *)g_malloc(len+1); 
  if(l_buff_ptr == NULL)
  {
    printf ("calloc error (%d Bytes not available)\n", (int)len);
    return(NULL);
  }
  l_buff_ptr[len] = '\0';

  /* File in eine Buffer laden */
  fp = fopen(fname, "r");		    /* open read */
  if(fp == NULL)
  {
    printf ("open(read) error on '%s'\n", fname);
    return(NULL);
  }
  fread(l_buff_ptr, 1, (size_t)len, fp);	    /* read */
  fclose(fp);				    /* close */

  return(l_buff_ptr);
}	/* end LoadFile */


