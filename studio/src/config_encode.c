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
void set_structs_default(struct encodingoptions *point);
void save_common(FILE *fp);
void save_section(FILE *fp, struct encodingoptions *point,char section[LONGOPT]);
void set_common(void);
void load_common(void);
void load_section(char section[LONGOPT],struct encodingoptions *point);
void print_encoding_options(char section[LONGOPT],struct encodingoptions *point);
void do_preset_mpeg2(struct encodingoptions *point);
void do_preset_vcd(struct encodingoptions *point);
void do_preset_svcd(struct encodingoptions *point);
void change_four(GtkAdjustment *adjust_scale);
void change_two(GtkAdjustment *adjust_scale);
void set_bicubicuse(GtkWidget *widget, gpointer data);
void player_callback( GtkWidget *widget, GtkWidget *player_field);

/* used from config.c */
int chk_dir(char *name);

/* Here the filename for the configuration is set, should be changed to
 * something that can be set in the command line, or at least in studio.c 
 * or the configuration is saved to the existing studio.conf
 */
#define encodeconfigfile "encode.conf"

/* other variables */
#define Encoptions_Table_x 2
#define Encoptions_Table_y 9
int t_use_yuvplay_pipe, t_encoding_syntax_style, t_addoutputnorm;
int t_fourpelmotion, t_twopelmotion, t_use_bicubic;
char t_selected_player[FILELEN];
char t_ininterlace_type[LONGOPT];
GtkWidget *fourpel_scale, *twopel_scale;

/* ================================================================= */

/* Set up the common values */
void set_common(void)
{
int i; 

for (i=0; i < FILELEN; i++)
  {
     enc_inputfile[i]  ='\0';
     enc_outputfile[i] ='\0';
     enc_audiofile[i]  ='\0';
     enc_videofile[i]  ='\0';
     selected_player[i]='\0';
  }

use_yuvplay_pipe = 0;
encoding_syntax_style = 0;
fourpelmotion = 2;
twopelmotion  = 3;
use_bicubic   = 0;
}

/* set the structs to default values */
void set_structs_default(struct encodingoptions *point)
{
int i;

  for (i=0; i < LONGOPT; i++)
    {
      (*point).notblacksize[i] ='\0';
      (*point).input_use[i]    ='\0';
      (*point).output_size[i]  ='\0';
      (*point).mode_keyword[i] ='\0';
      (*point).ininterlace_type[i] ='\0';
    }

  for (i=0; i < SHORTOPT; i++)
    {
      (*point).forcestereo[i]    ='\0';
      (*point).forcemono[i]      ='\0';
      (*point).forcevcd[i]       ='\0';
      (*point).muxvbr[i]         ='\0';
    }

  (*point).addoutputnorm  = 0;
  (*point).outputformat   = 0;
  (*point).droplsb        = 0;
  (*point).noisefilter    = 0;
  (*point).audiobitrate   = 0;
  (*point).outputbitrate  = 0;
  (*point).bitrate        = 1152;
  (*point).searchradius   = 0;
  (*point).muxformat      = 0;
  (*point).streamdatarate = 0;
  (*point).decoderbuffer  = 46;
  (*point).qualityfactor  = 0;
  (*point).minGop         = 12;
  (*point).maxGop         = 12;
  (*point).sequencesize   = 0;
  (*point).nonvideorate   = 0;

  sprintf((*point).output_size,"as is");
}

/* set some mpeg2 specific options */
void do_preset_mpeg2(struct encodingoptions *point)
{
  (*point).muxformat = 3;
  (*point).bitrate = 2500;
}

/* set some VCD specific options */
void do_preset_vcd(struct encodingoptions *point)
{
  (*point).muxformat   = 1;
  sprintf((*point).forcevcd,"-V");
}

/* set some SVCD specific options */
void do_preset_svcd(struct encodingoptions *point)
{
  (*point).muxformat = 4;
  sprintf((*point).forcevcd,"-V");
  sprintf((*point).output_size,"SVCD");
  (*point).bitrate = 2500;
}

/* load the common things */
void load_common()
{
char *val;
int i;

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

  if (NULL != (val = cfg_get_str("Studio","Encode_Player_use")))
      sprintf(selected_player, val);
  else 
      sprintf(selected_player, "no player selected");

  if (NULL != (val = cfg_get_str("Studio","Encode_Video_Preview")))
    if ( 0 == strcmp(val,"yes"))
        use_yuvplay_pipe = 1;
    else 
        use_yuvplay_pipe = 0;

  if (-1 != (i = cfg_get_int("Studio","Encoding_Syntax_Style")))
    encoding_syntax_style = i;
  else
    encoding_syntax_style = 140;

  if (-1 != (i = cfg_get_int("Studio","Encoding_four_pel_motion_compensation")))
    fourpelmotion = i;

  if (-1 != (i = cfg_get_int("Studio","Encoding_two_pel_motion_compensation")))
    twopelmotion = i;

  if (NULL != (val = cfg_get_str("Studio","Encode_Bicubic_Scaling")))
    if ( 0 == strcmp(val,"yes"))
        use_yuvplay_pipe = 1;
    else 
        use_yuvplay_pipe = 0;
}

void load_section(char section[LONGOPT],struct encodingoptions *point)
{
char *val;
int i;

  if (NULL != (val = cfg_get_str(section,"Encode_Notblack_size")))
        sprintf((*point).notblacksize, val);
  else
      sprintf((*point).notblacksize,"as is");

  if (NULL != (val = cfg_get_str(section,"Encode_Input_use")))
      sprintf((*point).input_use, val);
  else
      sprintf((*point).input_use,"as is");

  if (NULL != (val = cfg_get_str(section,"Encode_Output_size")))
      sprintf((*point).output_size, val);

  if (NULL != (val = cfg_get_str(section,"Encode_Mode_keyword")))
      sprintf((*point).mode_keyword, val);
  else
      sprintf((*point).mode_keyword,"as is");

  if (-1 != (i = cfg_get_int(section,"Encode_Outputformat")))
        (*point).outputformat = i;
  else
        (*point).outputformat = 0;

  if (-1 != (i = cfg_get_int(section,"Encode_Droplsb")))
        (*point).droplsb = i;
  else
        (*point).droplsb = 0;

  if (-1 != (i = cfg_get_int(section,"Encode_Noisefilter")))
        (*point).noisefilter = i;
  else
        (*point).noisefilter = 0;

  if (-1 != (i = cfg_get_int(section,"Encode_Audiobitrate")))
        (*point).audiobitrate = i;
  else
        (*point).audiobitrate = 224;

  if (-1 != (i = cfg_get_int(section,"Encode_Outputbitrate")))
        (*point).outputbitrate = i;
  else
        (*point).outputbitrate = 441;

  if (NULL != (val = cfg_get_str(section,"Encode_Force_Stereo")))
    if ( 0 != strcmp(val,"as is"))
      sprintf((*point).forcestereo, val);

  if (NULL != (val = cfg_get_str(section,"Encode_Force_Mono")))
    if ( 0 != strcmp(val,"as is"))
      sprintf((*point).forcemono, val);

  if (NULL != (val = cfg_get_str(section,"Encode_Force_Vcd")))
    if ( 0 != strcmp(val,"as is"))
      sprintf((*point).forcevcd, val);

  if (-1 != (i = cfg_get_int(section,"Encode_Video_Bitrate")))
        (*point).bitrate = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Quality_Factor")))
        (*point).qualityfactor = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Minimum_GOP_Size")))
        (*point).minGop = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Maximum_GOP_Size")))
        (*point).maxGop = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Sequence_Size")))
        (*point).sequencesize = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Non_Video_Bitrate")))
        (*point).nonvideorate = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Searchradius")))
        (*point).searchradius = i;
  else
        (*point).searchradius = 16;

  if (-1 != (i = cfg_get_int(section,"Encode_Muxformat")))
        (*point).muxformat = i;

  if (NULL != (val = cfg_get_str(section,"Encode_Mux_VBR")))
    if ( 0 != strcmp(val,"as is"))
      sprintf((*point).muxvbr, val);

  if (-1 != (i = cfg_get_int(section,"Encode_Stream_Datarate")))
        (*point).streamdatarate = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Decoder_Buffer")))
        (*point).decoderbuffer = i;
}

/* show the current options */
void print_encoding_options(char section[LONGOPT], struct encodingoptions *point)
{
  printf("\n Encoding options of %s \n",section);
  printf("Encoding Active Window set to \'%s\' \n",    (*point).notblacksize);
  printf("Encoding Input use to \'%s\' \n",               (*point).input_use);
  printf("Encoding Output size to \'%s\' \n",           (*point).output_size);
  printf("Encoding yvscaler scaling Mode to \'%s\' \n",(*point).mode_keyword);
  printf("Encoding ininterlace type \'%s\' \n",    (*point).ininterlace_type);
  printf("Encoding lav2yuv output Format \'%i\' \n",   (*point).outputformat);
  printf("Encoding Dropping \'%i\'LSB \n",                  (*point).droplsb);
  printf("Encoding Noise filter strength: \'%i\' \n",   (*point).noisefilter);
  printf("Encoding Audio Bitrate : \'%i\' \n",         (*point).audiobitrate);
  printf("Encoding Audio Samplerate : \'%i\' \n",     (*point).outputbitrate);
  printf("Encoding Audio force stereo : \'%s\' \n",     (*point).forcestereo);
  printf("Encoding Audio force mono : \'%s\' \n",         (*point).forcemono);
  printf("Encoding Audio force VCD : \'%s\' \n",           (*point).forcevcd);
  printf("Encoding Video Bitrate : \'%i\' \n",              (*point).bitrate);
  printf("Encoding Quality Factor : \'%i\' \n",       (*point).qualityfactor);
  printf("Encoding Minimum GOP size : \'%i\' \n",            (*point).minGop);
  printf("Encoding Maximum GOP size : \'%i\' \n",            (*point).maxGop);
  printf("Encoding sequence every MB in final:\'%i\'\n",(*point).sequencesize);
  printf("Encoding Non-video data bitrate \'%i\' \n",  (*point).nonvideorate);
  printf("Encoding Searchradius : \'%i\' \n",          (*point).searchradius);
  printf("Encoding Mplex Format : \'%i\' \n",             (*point).muxformat);
  printf("Encoding Mux VBR : \'%s\' \n",                     (*point).muxvbr);
  printf("Encoding Stream Datarate : \'%i\' \n",     (*point).streamdatarate);
  printf("Encoding Decoder Buffer Size : \'%i\' \n",  (*point).decoderbuffer);
}

void load_config_encode()
{
struct encodingoptions *point;
char filename[100];
char section[LONGOPT];
int have_config;

  sprintf(filename,"%s/%s/%s",getenv("HOME"),".studio", encodeconfigfile );
  if (0 == cfg_parse_file(filename))
    have_config = 1;

  point = &encoding; 
  set_structs_default(point);  /* set struct to the defaults */

  point = &encoding2;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_mpeg2(point);     /* set some mpeg2 specific options */

  point = &encoding_vcd;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_vcd(point);     /* set some VCD specific options */

  point = &encoding_svcd;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_svcd(point);     /* set some SVCD specific options */
 
  /* Saved bu not in the structs */
  encoding_syntax_style = 0;

  set_common();
  load_common();
  
  if (verbose)
    {
      printf("Encode input file set to \'%s\' \n",enc_inputfile);
      printf("Encode output file set to \'%s\' \n",enc_outputfile);
      printf("Encode input file set to \'%s\' \n",enc_audiofile);
      printf("Encode video file set to \'%s\' \n",enc_videofile);
      printf("Video player set to \'%s\' \n",selected_player);
      printf("Encoding Preview with yuvplay : \'%i\' \n",use_yuvplay_pipe);
      printf("Encoding Syntax Style :\'%i\' \n",encoding_syntax_style);
      printf("Encoding 4*4-pel motion compensation :\'%i\' \n",fourpelmotion);
      printf("Encoding 2*2-pel motion compensation :\'%i\' \n",twopelmotion);
    }

  strncpy(section,"MPEG1",LONGOPT);
  point = &encoding; 
  load_section(section,point);
  if (verbose) 
    print_encoding_options(section,point);

  strncpy(section,"MPEG2",LONGOPT);
  point = &encoding2; 
  load_section("MPEG2",point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"VCD",LONGOPT);
  point = &encoding_vcd; 
  load_section("VCD",point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"SVCD",LONGOPT);
  point = &encoding_svcd; 
  load_section("SVCD",point);
  if (verbose)
    print_encoding_options(section,point);

}

void save_common(FILE *fp)
{

  fprintf(fp,"[Studio]\n");
  fprintf(fp,"Encode_Input_file = %s\n", enc_inputfile);
  fprintf(fp,"Encode_Output_file = %s\n", enc_outputfile);
  fprintf(fp,"Encode_Audio_file = %s\n", enc_audiofile);
  fprintf(fp,"Encode_Video_file = %s\n", enc_videofile);
  fprintf(fp,"Encode_Player_use = %s\n", selected_player);

  if (use_yuvplay_pipe == 1)
    fprintf(fp,"Encode_Video_Preview = %s\n", "yes");
  else
    fprintf(fp,"Encode_Video_Preview = %s\n", "no");

  fprintf(fp,"Encoding_Syntax_Style = %i\n", encoding_syntax_style);

  fprintf(fp,"Encoding_four_pel_motion_compensation = %i\n", fourpelmotion);
  fprintf(fp,"Encoding_two_pel_motion_compensation = %i\n", twopelmotion);

  if (use_bicubic == 1)
    fprintf(fp,"Encode_Bicubic_Scaling = %s\n", "yes");
  else
    fprintf(fp,"Encode_Bicubic_Scaling = %s\n", "no");
}

void save_section(FILE *fp, struct encodingoptions *point, char section[LONGOPT])
{
  fprintf(fp,"[%s]\n",section);

  if (strlen ((*point).notblacksize) > 0)
    fprintf(fp,"Encode_Notblack_size = %s\n", (*point).notblacksize);
  else
    fprintf(fp,"Encode_Notblack_size = %s\n", "as is");

  if (strlen ((*point).input_use) > 0)
    fprintf(fp,"Encode_Input_use = %s\n",(*point).input_use);
  else
    fprintf(fp,"Encode_Input_use = %s\n", "as is");

  if (strlen ((*point).output_size) > 0)
    fprintf(fp,"Encode_Output_size = %s\n", (*point).output_size);
  else
    fprintf(fp,"Encode_Output_size = %s\n", "as is");

  if (strlen ((*point).mode_keyword) > 0)
    fprintf(fp,"Encode_Mode_keyword = %s\n", (*point).mode_keyword);
  else
    fprintf(fp,"Encode_Mode_keyword = %s\n", "as is");

  fprintf(fp,"Encode_Outputformat = %i\n", (*point).outputformat);
  fprintf(fp,"Encode_Droplsb = %i\n", (*point).droplsb);
  fprintf(fp,"Encode_Noisefilter = %i\n", (*point).noisefilter);
  fprintf(fp,"Encode_Audiobitrate = %i\n", (*point).audiobitrate);
  fprintf(fp,"Encode_Outputbitrate = %i\n", (*point).outputbitrate);

  if ((*point).forcestereo[0] == '-')
    fprintf(fp,"Encode_Force_Stereo = %s\n", (*point).forcestereo);
  else
    fprintf(fp,"Encode_Force_Stereo = %s\n", "as is");

  if ((*point).forcemono[0] == '-')
    fprintf(fp,"Encode_Force_Mono = %s\n", (*point).forcemono);
  else
    fprintf(fp,"Encode_Force_Mono = %s\n", "as is");

  if ((*point).forcevcd[0] == '-')
    fprintf(fp,"Encode_Force_Vcd = %s\n", (*point).forcevcd);
  else
    fprintf(fp,"Encode_Force_Vcd = %s\n", "as is");

  fprintf(fp,"Encode_Video_Bitrate = %i\n", (*point).bitrate);
  fprintf(fp,"Encode_Quality_Factor = %i\n", (*point).qualityfactor);
  fprintf(fp,"Encode_Minimum_GOP_Size = %i\n", (*point).minGop);
  fprintf(fp,"Encode_Maximum_GOP_Size = %i\n", (*point).maxGop);
  fprintf(fp,"Encode_Sequence_Size = %i\n", (*point).sequencesize);
  fprintf(fp,"Encode_Non_Video_Bitrate = %i\n", (*point).nonvideorate);
  fprintf(fp,"Encode_Searchradius = %i\n", (*point).searchradius);

  fprintf(fp,"Encode_Muxformat = %i\n", (*point).muxformat);

  if ((*point).muxvbr[0] == '-')
    fprintf(fp,"Encode_Mux_VBR = %s\n", (*point).muxvbr);
  else
    fprintf(fp,"Encode_Mux_VBR = %s\n", "from stream");

  fprintf(fp,"Encode_Stream_Datarate = %i\n", (*point).streamdatarate);
  fprintf(fp,"Encode_Decoder_Buffer = %i\n", (*point).decoderbuffer);

}

/* Save the current encoding configuration */
void save_config_encode()
{
char filename[100], directory[100];
struct encodingoptions *point;
FILE *fp;

  sprintf(filename,"%s/%s/%s",getenv("HOME"),".studio/", encodeconfigfile);
  sprintf(directory,"%s/%s",getenv("HOME"),".studio/");

  if (chk_dir(directory) == 0)
    {
      fprintf(stderr,"cant open config file %s\n",filename);
      return;
    }
 
  unlink(filename);

  /* write new one... */
  fp = fopen(filename,"w");
  if (NULL == fp)
    {
       fprintf(stderr,"cant open config file %s\n",filename);
       return;
    }

  /* Save common things like: filenames, preview, ... */
  save_common(fp);

  /* Save the working options of the encoding parameters */
  point = &encoding; 
  save_section(fp,point,"MPEG1");
 
  point = &encoding2; 
  save_section(fp,point,"MPEG2");
 
  point = &encoding_vcd; 
  save_section(fp,point,"VCD");
 
  point = &encoding_svcd; 
  save_section(fp,point,"SVCD");
 
  if (verbose) printf("Configuration of the encoding options saved\n");

  fclose(fp);
}

/* This is done the the OK Button was pressed */
void accept_encoptions(GtkWidget *widget, gpointer data)
{
  if (verbose)
    {
      printf ("\nThe OK button in the Encoding Options was pressed, changing options:\n");
      if (t_use_yuvplay_pipe != use_yuvplay_pipe)
        printf(" Encoding Preview set to %i \n ",t_use_yuvplay_pipe);

      if (t_addoutputnorm != encoding.addoutputnorm)
        printf(" Output norm is added %i \n ",t_addoutputnorm);

      if (strcmp(t_ininterlace_type, encoding.ininterlace_type) != 0 )
        printf(" yuvscaler interlace type set to %s \n ", t_ininterlace_type);
    
      if (t_encoding_syntax_style != encoding_syntax_style)
        printf(" Encoding Syntax set to %i \n", t_encoding_syntax_style);

      if (t_use_bicubic != use_bicubic)
        printf(" For scaling use bicubic %i \n", t_use_bicubic);

      if (t_fourpelmotion != fourpelmotion)
        printf(" 4*4-pel subsampled motion compensation : %i \n", t_fourpelmotion);
      if (t_twopelmotion != twopelmotion)
        printf(" 2*2-pel subsampled motion compensation : %i\n",t_twopelmotion);
      
      if (strcmp(t_selected_player,selected_player) != 0)
        printf(" Setting the player to : %s \n", t_selected_player);
    }
 
  use_yuvplay_pipe = t_use_yuvplay_pipe;
  encoding.addoutputnorm = t_addoutputnorm;
  strcpy(encoding.ininterlace_type, t_ininterlace_type);
  use_bicubic = t_use_bicubic;
  encoding_syntax_style = t_encoding_syntax_style;  
  fourpelmotion = t_fourpelmotion;
  twopelmotion = t_twopelmotion;
  strcpy(selected_player,t_selected_player);
}

/* Set the value of the Slider 4 to the variable */
void change_four ( GtkAdjustment *adjust_scale)
{
  t_fourpelmotion = adjust_scale->value;
}

/* Set the value of the Slider 2 to the variable */
void change_two ( GtkAdjustment *adjust_scale)
{
  t_twopelmotion = adjust_scale->value;
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

/* Set if the bicubic rescaling algorithm is used */ 
void set_bicubicuse(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_use_bicubic = 1;
  else
    t_use_bicubic = 0;
}

/* Set the player for the video */
void player_callback( GtkWidget *widget, GtkWidget *player_field)
{
gchar *name;

  name = gtk_entry_get_text(GTK_ENTRY(player_field));

  strcpy(t_selected_player,name);
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
GtkWidget *bicubic_button, *player_field;
GSList *interlace_type, *encoding_style;
GtkObject *adjust_scale, *adjust_scale_n;
int table_line;

table_line = 0;

  t_use_yuvplay_pipe = use_yuvplay_pipe;
  t_addoutputnorm = encoding.addoutputnorm;
  strcpy(t_ininterlace_type, encoding.ininterlace_type);
  t_use_bicubic = use_bicubic; 
  t_encoding_syntax_style = encoding_syntax_style;
  t_fourpelmotion = fourpelmotion;
  t_twopelmotion = twopelmotion;
  sprintf(t_selected_player, "%s", selected_player);

  label = gtk_label_new ("Show video while encoding : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
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

  label = gtk_label_new ("Add norm when using yuvscaler : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
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

  label = gtk_label_new ("Interlacing type for yuvscaler : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
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

  label = gtk_label_new ("only for yuvscaler 1.4.x");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
  gtk_widget_show (label);
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

  label = gtk_label_new ("Always use bicubic for scaling : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
  gtk_widget_show (label);
  bicubic_button = gtk_check_button_new();
//  gtk_widget_ref (bicubic_button);
  if (use_bicubic)  /* <- Does the preset of the values -v */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bicubic_button), TRUE);
  gtk_signal_connect (GTK_OBJECT (bicubic_button), "toggled",
                      GTK_SIGNAL_FUNC (set_bicubicuse), NULL );
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             bicubic_button, 1, 2, table_line, table_line+1);
  gtk_widget_show (bicubic_button);
  table_line++;

  label = gtk_label_new ("Syntax style for encoding : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
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

  label = gtk_label_new (" 4*4-pel subsampled motion comp : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
  gtk_widget_show (label);

  adjust_scale = gtk_adjustment_new (1.0, 1.0, 5.0, 1, 1.0, 1.0);

  fourpel_scale = gtk_hscale_new (GTK_ADJUSTMENT (adjust_scale));
  gtk_scale_set_value_pos (GTK_SCALE(fourpel_scale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (fourpel_scale),0);
  gtk_signal_connect (GTK_OBJECT (adjust_scale), "value_changed",
                         GTK_SIGNAL_FUNC (change_four), adjust_scale);
  gtk_table_attach_defaults (GTK_TABLE (table),
                            fourpel_scale, 1, 2, table_line, table_line+1);
  gtk_widget_show (fourpel_scale);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adjust_scale), t_fourpelmotion);
  table_line++;

  label = gtk_label_new (" 2*2-pel subsampled motion comp : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
  gtk_widget_show (label);

  adjust_scale_n = gtk_adjustment_new (1.0, 1.0, 5.0, 1, 1.0, 1.0);
  twopel_scale = gtk_hscale_new (GTK_ADJUSTMENT (adjust_scale_n));
  gtk_scale_set_value_pos (GTK_SCALE(twopel_scale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (twopel_scale),0);
  gtk_signal_connect (GTK_OBJECT (adjust_scale_n), "value_changed",
                         GTK_SIGNAL_FUNC (change_two), adjust_scale_n);
  gtk_table_attach_defaults (GTK_TABLE (table),
                            twopel_scale, 1, 2, table_line, table_line+1);
  gtk_widget_show (twopel_scale);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adjust_scale_n), t_twopelmotion);
  table_line++;

  label = gtk_label_new (" Player and options for playback : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
  gtk_widget_show (label);
  
  player_field = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(player_field), selected_player); 
  gtk_signal_connect(GTK_OBJECT(player_field), "changed",
                     GTK_SIGNAL_FUNC(player_callback), player_field);
  gtk_widget_set_usize (player_field, 100, -2);
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             player_field, 1, 2, table_line, table_line+1);
  gtk_widget_show (player_field);
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


