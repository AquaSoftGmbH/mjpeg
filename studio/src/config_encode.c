/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * config_encode done by Bernhard
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

/* 
 * Here the configuration for the encoding is loaded and saved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <glib.h>

#include "parseconfig.h"
#include "studio.h"

/* Forward declarations */
void load_config_encode(void);
void save_config_encode(void);
void open_encoptions_window(GtkWidget *widget, gpointer data);
void accept_encoptions(GtkWidget *widget, gpointer data);
void set_encoding_preview(GtkWidget *widget, gpointer data);
void create_encoding_layout(GtkWidget *table);
void set_interlacein_type(GtkWidget *widget, gpointer data);
void set_addoutputnorm(GtkWidget *widget, gpointer data);
void set_encoding_syntax(GtkWidget *widget, gpointer data);

/* config.c */
int chk_dir(char *name);

/* Here the filename for the configuration is set, should be changed to something
 * that can be set in the command line, or at least in studio.c 
 * or the configuration is saved to the existing studio.conf
 */
#define encodeconfigfile "encode.conf"

/* other variables*/
#define Encoptions_Table_x 2
#define Encoptions_Table_y 7
int t_use_yuvplay_pipe, t_encoding_syntax_style, t_addoutputnorm;
char t_ininterlace_type[LONGOPT];

/* ================================================================= */

void load_config_encode()
{
char filename[100];
char *val;
int  have_config, i;

  sprintf(filename,"%s/%s/%s",getenv("HOME"),".studio", encodeconfigfile );
  if (0 == cfg_parse_file(filename))
    have_config = 1;
   
  for (i=0; i < LONGOPT; i++)
    {
      encoding.notblacksize[i] ='\0';
      encoding.input_use[i]    ='\0';
      encoding.output_size[i]  ='\0';
      encoding.mode_keyword[i] ='\0';
      encoding.ininterlace_type[i] ='\0';
    }

  for (i=0; i < SHORTOPT; i++)
    {
      encoding.forcestereo[i]    ='\0';
      encoding.forcemono[i]      ='\0';
      encoding.forcevcd[i]       ='\0';
    }

  encoding.addoutputnorm = 0;
  encoding_syntax_style = 0;

  if (NULL != (val = cfg_get_str("Studio","Encode_Input_file")))
      sprintf(enc_inputfile, val);
  else 
      sprintf(enc_inputfile, "test.avi");
 
  if (NULL != (val = cfg_get_str("Studio","Encode_Output_file")))
      sprintf(enc_outputfile, val);
  else 
      sprintf(enc_outputfile,"%s/output.mpg", getenv("HOME"));

  if (NULL != (val = cfg_get_str("Studio","Encode_Audio_file")))
      sprintf(enc_audiofile, val);
  else 
      sprintf(enc_audiofile, "/tmp/audio.mp2");

  if (NULL != (val = cfg_get_str("Studio","Encode_Video_file")))
      sprintf(enc_videofile, val);
  else 
      sprintf(enc_videofile, "/tmp/video.m1v");

  if (NULL != (val = cfg_get_str("Studio","Encode_Notblack_size")))
        sprintf(encoding.notblacksize, val);
  else 
      sprintf(encoding.notblacksize,"as is");

  if (NULL != (val = cfg_get_str("Studio","Encode_Input_use")))
      sprintf(encoding.input_use, val);
  else 
      sprintf(encoding.input_use,"as is");

  if (NULL != (val = cfg_get_str("Studio","Encode_Output_size")))
      sprintf(encoding.output_size, val);
  else 
      sprintf(encoding.output_size,"as is");

  if (NULL != (val = cfg_get_str("Studio","Encode_Mode_keyword")))
      sprintf(encoding.mode_keyword, val);
  else 
      sprintf(encoding.mode_keyword,"as is");

  if (-1 != (i = cfg_get_int("Studio","Encode_Outputformat")))
        encoding.outputformat = i;
  else 
        encoding.outputformat = 0;

  if (-1 != (i = cfg_get_int("Studio","Encode_Droplsb")))
        encoding.droplsb = i;
  else 
        encoding.droplsb = 0;

  if (-1 != (i = cfg_get_int("Studio","Encode_Noisefilter")))
        encoding.noisefilter = i;
  else 
        encoding.noisefilter = 0;

  if (-1 != (i = cfg_get_int("Studio","Encode_Audiobitrate")))
        encoding.audiobitrate = i;
  else 
        encoding.audiobitrate = 224;

  if (-1 != (i = cfg_get_int("Studio","Encode_Outputbitrate")))
        encoding.outputbitrate = i;
  else 
        encoding.outputbitrate = 441;
  
  if (NULL != (val = cfg_get_str("Studio","Encode_Force_Stereo")))
    if ( 0 != strcmp(val,"as is"))
      sprintf(encoding.forcestereo, val);
  
  if (NULL != (val = cfg_get_str("Studio","Encode_Force_Mono")))
    if ( 0 != strcmp(val,"as is"))
      sprintf(encoding.forcemono, val);
  
  if (NULL != (val = cfg_get_str("Studio","Encode_Force_Vcd")))
    if ( 0 != strcmp(val,"as is"))
      sprintf(encoding.forcevcd, val);

  if (-1 != (i = cfg_get_int("Studio","Encode_Mpeglevel")))
        encoding.mpeglevel = i;
  else 
        encoding.mpeglevel = 1;
  
  if (-1 != (i = cfg_get_int("Studio","Encode_Video_Bitrate")))
        encoding.bitrate = i;
  else 
        encoding.bitrate = 1152;
  
  if (-1 != (i = cfg_get_int("Studio","Encode_Searchradius")))
        encoding.searchradius = i;
  else 
        encoding.searchradius = 16;

  if (-1 != (i = cfg_get_int("Studio","Encode_Muxformat")))
        encoding.muxformat = i;
  else 
        encoding.muxformat = 0;

  if (NULL != (val = cfg_get_str("Studio","Encode_Video_Preview")))
    if ( 0 == strcmp(val,"yes"))
        use_yuvplay_pipe = 1;
    else 
        use_yuvplay_pipe = 0;

  if (-1 != (i = cfg_get_int("Studio","Encoding_Syntax_style")))
    encoding_syntax_style = i;
  else
    encoding_syntax_style = 140;
    

  if (verbose)
    {
      printf("Encode input file set to \'%s\' \n",enc_inputfile);
      printf("Encode output file set to \'%s\' \n",enc_outputfile);
      printf("Encode input file set to \'%s\' \n",enc_audiofile);
      printf("Encode video file set to \'%s\' \n",enc_videofile);
      printf("Encoding Active Window set to \'%s\' \n", encoding.notblacksize);
      printf("Encoding Input use to \'%s\' \n",encoding.input_use);
      printf("Encoding Output size to \'%s\' \n",encoding.output_size);
      printf("Encoding yvscaler scaling Mode to \'%s\' \n",encoding.mode_keyword);
      printf("Encoding lav2yuv output Format \'%i\' \n",encoding.outputformat);
      printf("Encoding Dropping \'%i\'LSB \n",encoding.droplsb);
      printf("Encoding Noise filter strength: \'%i\' \n",encoding.noisefilter);
      printf("Encoding Audio force stereo: \'%s\' \n",encoding.forcestereo);
      printf("Encoding Audio force mono: \'%s\' \n",encoding.forcemono);
      printf("Encoding Audio force Vcd: \'%s\' \n",encoding.forcevcd);
      printf("Encoding Mpeg level: \'%i\' \n",encoding.mpeglevel);
      printf("Encoding Video Bitrate: \'%i\' \n",encoding.bitrate);
      printf("Encoding Searchradius : \'%i\' \n",encoding.searchradius);
      printf("Encoding Mplex Format : \'%i\' \n",encoding.muxformat);
      printf("Encoding Preview with yuvplay : \'%i\' \n",use_yuvplay_pipe);
      printf("Encoding Syntax Style :\'%i\' \n",encoding_syntax_style);
    }
}

/* Save the current encoding configuration */
void save_config_encode()
{
char filename[100], directory[100];
FILE *fp;

  sprintf(filename,"%s/%s/%s",getenv("HOME"),".studio/", encodeconfigfile);
  sprintf(directory,"%s/%s",getenv("HOME"),".studio/");

  if (chk_dir(directory) == 0)
    {
      fprintf(stderr,"can't open config file %s\n",filename);
      return;
    }
 
  unlink(filename);

  /* write new one... */
  fp = fopen(filename,"w");
  if (NULL == fp)
    {
       fprintf(stderr,"can't open config file %s\n",filename);
       return;
    }

  fprintf(fp,"[Studio]\n");
  fprintf(fp,"Encode_Input_file = %s\n", enc_inputfile);
  fprintf(fp,"Encode_Output_file = %s\n", enc_outputfile);
  fprintf(fp,"Encode_Audio_file = %s\n", enc_audiofile);
  fprintf(fp,"Encode_Video_file = %s\n", enc_videofile);
  if (strlen (encoding.notblacksize) > 0)
    fprintf(fp,"Encode_Notblack_size = %s\n", encoding.notblacksize);
  else 
    fprintf(fp,"Encode_Notblack_size = %s\n", "as is");

  if (strlen (encoding.input_use) > 0)
    fprintf(fp,"Encode_Input_use = %s\n",encoding.input_use);
  else 
    fprintf(fp,"Encode_Input_use = %s\n", "as is");

  if (strlen (encoding.output_size) > 0)
    fprintf(fp,"Encode_Output_size = %s\n", encoding.output_size);
  else 
    fprintf(fp,"Encode_Output_size = %s\n", "as is");

  if (strlen (encoding.mode_keyword) > 0)
    fprintf(fp,"Encode_Mode_keyword = %s\n", encoding.mode_keyword);
  else 
    fprintf(fp,"Encode_Mode_keyword = %s\n", "as is");

  fprintf(fp,"Encode_Outputformat = %i\n", encoding.outputformat);
  fprintf(fp,"Encode_Droplsb = %i\n", encoding.droplsb);
  fprintf(fp,"Encode_Noisefilter = %i\n", encoding.noisefilter);
  fprintf(fp,"Encode_Audiobitrate = %i\n", encoding.audiobitrate);
  fprintf(fp,"Encode_Outputbitrate = %i\n", encoding.outputbitrate);

  if (encoding.forcestereo[0] == '-')
    fprintf(fp,"Encode_Force_Stereo = %s\n", encoding.forcestereo);
  else
    fprintf(fp,"Encode_Force_Stereo = %s\n", "as is");

  if (encoding.forcemono[0] == '-')
    fprintf(fp,"Encode_Force_Mono = %s\n", encoding.forcemono);
  else
    fprintf(fp,"Encode_Force_Mono = %s\n", "as is");

  if (encoding.forcevcd[0] == '-')
    fprintf(fp,"Encode_Force_Vcd = %s\n", encoding.forcevcd);
  else
    fprintf(fp,"Encode_Force_Vcd = %s\n", "as is");

  fprintf(fp,"Encode_Mpeglevel = %i\n", encoding.mpeglevel);
  fprintf(fp,"Encode_Video_Bitrate = %i\n", encoding.bitrate);
  fprintf(fp,"Encode_Searchradius = %i\n", encoding.searchradius);

  fprintf(fp,"Encode_Muxformat = %i\n", encoding.muxformat);
 
  if (use_yuvplay_pipe == 1)
    fprintf(fp,"Encode_Video_Preview = %s\n", "yes");
  else
    fprintf(fp,"Encode_Video_Preview = %s\n", "no");

   if (verbose) printf("Configuration of the encoding options saved\n");

  fprintf(fp,"Encoding_Syntax_style = %i\n", encoding_syntax_style);

  fclose(fp);
}

/* This is done the the OK Button was pressed */
void accept_encoptions(GtkWidget *widget, gpointer data)
{
  if (verbose)
    {
      printf ("\nThe OK button in the Encoding Options was pressed, changing options:\n");
      if (t_use_yuvplay_pipe != use_yuvplay_pipe)
        printf(" Encoding Preview set to %i \n ",use_yuvplay_pipe);

      if (t_addoutputnorm != encoding.addoutputnorm)
        printf(" Output norm is added %i \n ",encoding.addoutputnorm);

      if (strcmp(t_ininterlace_type, encoding.ininterlace_type) != 0 )
        printf(" yuvscaler interlace type set to %s \n ",
                                      encoding.ininterlace_type);
    
      if (t_encoding_syntax_style != encoding_syntax_style)
        printf(" Encoding Syntax set to %i \n", t_encoding_syntax_style);
    }
  
  use_yuvplay_pipe = t_use_yuvplay_pipe;
  encoding.addoutputnorm = t_addoutputnorm;
  strcpy(encoding.ininterlace_type, t_ininterlace_type);
  encoding_syntax_style = t_encoding_syntax_style;   
}

/* Set if the video window is shown */
void set_encoding_preview(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_use_yuvplay_pipe = 1;
  else
    t_use_yuvplay_pipe = 0;
}

/* Set if the output norm is added */
void set_addoutputnorm(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_addoutputnorm = 1;
  else
    t_addoutputnorm = 0;
}

/* Process the settings for the odd/even input option of yuvscaler */
void set_interlacein_type(GtkWidget *widget, gpointer data)
{
int i;
 
  for (i=0; i < LONGOPT; i++)
    t_ininterlace_type[i]='\0';

  if (strcmp((char*)data,"INTERLACED_ODD_FIRST") == 0 )
    sprintf(t_ininterlace_type,"%s",(char*)data);

  if (strcmp((char*)data,"INTERLACED_EVEN_FIRST") == 0 )
    sprintf(t_ininterlace_type,"%s",(char*)data);
}

/* Set the encoding syntax syle */
void set_encoding_syntax(GtkWidget *widget, gpointer data)
{

  t_encoding_syntax_style = atoi ((char*)data);
}

/* Here the fist page of the layout is created */
void create_encoding_layout(GtkWidget *table)
{
GtkWidget *preview_button, *label, *addnorm_button, *int_asis_button;
GtkWidget *int_odd_button, *int_even_button, *style_14x, *style_15x;
GSList *interlace_type, *encoding_style;
int table_line;

table_line = 0;

  t_use_yuvplay_pipe = use_yuvplay_pipe;
  t_addoutputnorm = encoding.addoutputnorm;
  strcpy(t_ininterlace_type, encoding.ininterlace_type);
  t_encoding_syntax_style = encoding_syntax_style;

  label = gtk_label_new ("Show video while encoding ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_widget_show (label);
  preview_button = gtk_check_button_new();
  gtk_widget_ref (preview_button);
  if (use_yuvplay_pipe) 
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preview_button), TRUE);
  gtk_signal_connect (GTK_OBJECT (preview_button), "toggled",
                      GTK_SIGNAL_FUNC (set_encoding_preview), NULL );
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             preview_button, 1, 2, table_line, table_line+1);
  gtk_widget_show (preview_button);
  table_line++;

  label = gtk_label_new ("Add norm when using yuvscaler ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_widget_show (label);
  addnorm_button = gtk_check_button_new();
  gtk_widget_ref (addnorm_button);
  if (encoding.addoutputnorm) 
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (addnorm_button), TRUE);
  gtk_signal_connect (GTK_OBJECT (addnorm_button), "toggled",
                      GTK_SIGNAL_FUNC (set_addoutputnorm), NULL );
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             addnorm_button, 1, 2, table_line, table_line+1);
  gtk_widget_show (addnorm_button);
  table_line++;

  label = gtk_label_new ("Interlacing type for yuvscaler ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_widget_show (label);
  int_asis_button = gtk_radio_button_new_with_label (NULL, " from header");
  gtk_signal_connect (GTK_OBJECT (int_asis_button), "toggled",
                      GTK_SIGNAL_FUNC (set_interlacein_type), (gpointer) "header");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             int_asis_button, 1, 2, table_line, table_line+1);
  interlace_type = gtk_radio_button_group (GTK_RADIO_BUTTON (int_asis_button));
  gtk_widget_show (int_asis_button);
  if (strcmp(encoding.ininterlace_type,"") == 0)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(int_asis_button), TRUE);
  table_line++;

  int_odd_button=gtk_radio_button_new_with_label(interlace_type," odd frame first");
  gtk_signal_connect (GTK_OBJECT (int_odd_button), "toggled",
      GTK_SIGNAL_FUNC (set_interlacein_type), (gpointer) "INTERLACED_ODD_FIRST");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             int_odd_button, 1, 2, table_line, table_line+1);
  interlace_type = gtk_radio_button_group (GTK_RADIO_BUTTON (int_odd_button));
  gtk_widget_show (int_odd_button);
  if (strcmp(encoding.ininterlace_type,"INTERLACED_ODD_FIRST") == 0)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(int_odd_button), TRUE);
  table_line++;

  int_even_button=gtk_radio_button_new_with_label(interlace_type," even frame first");
  gtk_signal_connect (GTK_OBJECT (int_even_button), "toggled",
      GTK_SIGNAL_FUNC (set_interlacein_type ), (gpointer) "INTERLACED_EVEN_FIRST");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             int_even_button, 1, 2, table_line, table_line+1);
  interlace_type = gtk_radio_button_group (GTK_RADIO_BUTTON (int_even_button));
  gtk_widget_show (int_even_button);
  if (strcmp(encoding.ininterlace_type,"INTERLACED_EVEN_FIRST") == 0)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(int_even_button), TRUE);
  table_line++;

  label = gtk_label_new ("Syntax style for encoding ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_widget_show (label);

  style_14x = gtk_radio_button_new_with_label (NULL, " 1.4.x");
  gtk_signal_connect (GTK_OBJECT (style_14x), "toggled",
                      GTK_SIGNAL_FUNC (set_encoding_syntax), (gpointer) "140");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             style_14x, 1, 2, table_line, table_line+1); 
  encoding_style = gtk_radio_button_group (GTK_RADIO_BUTTON (style_14x)); 
  gtk_widget_show (style_14x);
  if (encoding_syntax_style == 140)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(style_14x), TRUE);
  table_line++;

  style_15x = gtk_radio_button_new_with_label (encoding_style, " 1.5.x");
  gtk_signal_connect (GTK_OBJECT (style_15x), "toggled",
                      GTK_SIGNAL_FUNC (set_encoding_syntax), (gpointer) "150");
  gtk_table_attach_defaults (GTK_TABLE (table),
                             style_15x, 1, 2, table_line, table_line+1);
  encoding_style = gtk_radio_button_group (GTK_RADIO_BUTTON (style_15x));
  gtk_widget_show (style_15x);
  if (encoding_syntax_style == 150)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(style_15x), TRUE);
  table_line++;
 
}

/* Encoding options dialog */
void open_encoptions_window(GtkWidget *widget, gpointer data)
{
        GtkWidget *options_window, *table, *button, *options_notebook;
        GtkWidget *vbox, *hbox;


        options_window = gtk_window_new(GTK_WINDOW_DIALOG);
        vbox = gtk_vbox_new (FALSE, 10);
        options_notebook = gtk_notebook_new ();

        gtk_window_set_title (GTK_WINDOW(options_window), 
                            "Linux Video Studio - Encoding Options");
        gtk_container_set_border_width (GTK_CONTAINER (options_window), 15);

        hbox = gtk_hbox_new(FALSE, 20);
        table = gtk_table_new (Encoptions_Table_x, Encoptions_Table_y, FALSE);
        create_encoding_layout(table);
        gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 20);
        gtk_widget_show(table);
        gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook), hbox, 
                                  gtk_label_new("Encoding Options"));
        gtk_widget_show (hbox);

/* Maybe needed if there are more options needed *
 *       hbox = gtk_hbox_new(TRUE, 20);
 *       table = gtk_table_new (2,8, FALSE);
 *       gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 20);
 *       gtk_widget_show(table);
 *       gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook), hbox, 
 *                                     gtk_label_new("More Options"));
 *       gtk_widget_show (hbox);
 * ***********************************************/

        gtk_notebook_set_tab_pos (GTK_NOTEBOOK (options_notebook), GTK_POS_TOP);        gtk_box_pack_start (GTK_BOX (vbox), options_notebook, FALSE, FALSE, 0);
        gtk_widget_show(options_notebook);

/* Create the OK and CANCEL buttons */
        hbox = gtk_hbox_new(TRUE, 20);

        button = gtk_button_new_with_label("OK");
        gtk_signal_connect(GTK_OBJECT(button), "clicked", 
                           GTK_SIGNAL_FUNC (accept_encoptions), NULL);
        gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                gtk_widget_destroy, GTK_OBJECT(options_window));
        gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
        gtk_widget_show(button);

        button = gtk_button_new_with_label("Cancel");
        gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                gtk_widget_destroy, GTK_OBJECT(options_window));
        gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
        gtk_widget_show(button);

        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        gtk_widget_show (hbox);

        gtk_container_add (GTK_CONTAINER (options_window), vbox);
        gtk_widget_show(vbox);

        gtk_widget_show(options_window);

}


