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
void set_machine_default(struct machine *point);
void set_distributed(void);
void set_scripting_defaults (void);
void save_common(FILE *fp);
void save_section(FILE *fp,struct encodingoptions *point,char section[LONGOPT]);
void save_machine_data(FILE *fp, char section[LONGOPT]);
void save_machine_section(FILE *fp,struct machine *point,char section[LONGOPT]);
void save_script_data(FILE *fp, char section[LONGOPT]);
void set_common(void);
void load_common(void);
void show_common(void);
void load_machine_data(char section[LONGOPT], struct machine *pointm);
void load_machine_names(void);
void load_section(char section[LONGOPT],struct encodingoptions *point);
void load_script_data(void);
void print_encoding_options(char section[LONGOPT],struct encodingoptions *point);
void print_machine_names(void);
void print_machine_data(char section[LONGOPT], struct machine *pointm);
void do_preset_mpeg2(struct encodingoptions *point);
void do_preset_vcd(struct encodingoptions *point);
void do_preset_svcd(struct encodingoptions *point);
void do_preset_dvd(struct encodingoptions *point);
void do_preset_divx(struct encodingoptions *point);
void do_preset_yuv2lav(struct encodingoptions *point);
void change_four(GtkAdjustment *adjust_scale);
void change_two(GtkAdjustment *adjust_scale);
void set_bicubicuse(GtkWidget *widget, gpointer data);
void player_callback( GtkWidget *widget, GtkWidget *player_field);
void set_saveonexit(GtkWidget *widget, gpointer data);

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
int t_use_yuvplay_pipe, t_addoutputnorm;
int t_fourpelmotion, t_twopelmotion, t_use_bicubic, t_saveonexit;
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
fourpelmotion = 2;
twopelmotion  = 3;
use_bicubic   = 0;
saveonexit    = 1;

enhanced_settings = 0; /* used in the distributed setup */

}

/**
    Set the struct (encodingoptions) to default values
 @param point points to the struct to set to the defaults */
void set_structs_default(struct encodingoptions *point)
{
int i;

  for (i=0; i < LONGOPT; i++)
    {
      (*point).notblacksize[i]  ='\0';
      (*point).interlacecorr[i] ='\0';
      (*point).input_use[i]     ='\0';
      (*point).output_size[i]   ='\0';
      (*point).mode_keyword[i]  ='\0';
      (*point).codec[i]  ='\0';
    }

  for (i=0; i < SHORTOPT; i++)
    {
      (*point).forcestereo[i]    ='\0';
      (*point).forcemono[i]      ='\0';
      (*point).forcevcd[i]       ='\0';
      (*point).muxvbr[i]         ='\0';
    }

  (*point).addoutputnorm  = 0;
  (*point).audiobitrate   = 224;
  (*point).outputbitrate  = 441;
  (*point).use_yuvdenoise = 0;
  (*point).deinterlace    = 0;
  (*point).sharpness      = 125;
  (*point).denois_thhold  = 5;
  (*point).average_frames = 3;
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

/** Set the machine struct to a predefines value 
 @param point points to the struct to set to the defaults */
void set_machine_default(struct machine *point)
{
  (*point).lav2wav   = 0;
  (*point).mp2enc    = 0;
  (*point).lav2yuv   = 0;
  (*point).yuvdenoise= 0;
  (*point).yuvscaler = 0;
  (*point).mpeg2enc  = 0;
  (*point).yuv2divx  = 0;
  (*point).yuv2lav   = 0;
}

/** Set up the default values for the distributed encoding */
void set_distributed()
{
struct machine *point;

  point = &machine4mpeg1;
  set_machine_default(point);  /* set struct to the defaults */
  point = &machine4mpeg2;
  set_machine_default(point);  /* set struct to the defaults */
  point = &machine4generic;
  set_machine_default(point);  /* set struct to the defaults */
  point = &machine4vcd;
  set_machine_default(point);  /* set struct to the defaults */
  point = &machine4svcd;
  set_machine_default(point);  /* set struct to the defaults */
  point = &machine4dvd;
  set_machine_default(point);  /* set struct to the defaults */
  point = &machine4divx;
  set_machine_default(point);  /* set struct to the defaults */
  point = &machine4yuv2lav;
  set_machine_default(point);  /* set struct to the defaults */

}

/** Set the defaults values for the scripting */
void set_scripting_defaults (void)
{
int i; 

for (i=0; i < FILELEN; i++)
   script_name[i]  ='\0';
 
  script_use_distributed = 0;

  script.mpeg1   = 0;
  script.mpeg2   = 0;
  script.generic = 0;
  script.vcd     = 0;
  script.svcd    = 0;
  script.dvd     = 0;
  script.divx    = 0;
  script.yuv2lav = 0;
}

/** set some mpeg2 specific options
 @param point points to the struct we use */
void do_preset_mpeg2(struct encodingoptions *point)
{
  (*point).muxformat = 3;
  (*point).bitrate = 2500;
}

/** set some VCD specific options
 @param point points to the struct we use */
void do_preset_vcd(struct encodingoptions *point)
{
  (*point).muxformat   = 1;
  sprintf((*point).forcevcd,"-V");
}

/** set some SVCD specific options
 @param point points to the struct we use */
void do_preset_svcd(struct encodingoptions *point)
{
  (*point).muxformat = 4;
  sprintf((*point).forcevcd,"-V");
  sprintf((*point).output_size,"SVCD");
  (*point).bitrate = 2500;
}

/** set some DVD specific options
 @param point points to the struct we use */
void do_preset_dvd(struct encodingoptions *point)
{
  (*point).outputbitrate = 480;
  (*point).muxformat = 8;
}

/** set some divx specific options
 @param point points to the struct we use */
void do_preset_divx(struct encodingoptions *point)
{
  (*point).audiobitrate = 0;
  (*point).bitrate = 0;
  sprintf((*point).codec,"DIV3");
}

/** set some yuv2lav specific options
 @param point points to the struct we use */
void do_preset_yuv2lav(struct encodingoptions *point)
{
  (*point).qualityfactor = 80;
  (*point).minGop = 3;
  sprintf((*point).codec,"AVI");
}

/* Load the machine number for the encoding process */
void load_machine_data(char section[LONGOPT],struct machine *pointm)
{
int i,j;
i=0;
j=0;

  j = g_list_length (machine_names);

  if (-1 != (i = cfg_get_int(section,"Machine_running_lav2wav")))
    {
      if ( i <= j )
         (*pointm).lav2wav = i;
      else if (verbose) 
         printf("Wrong value for lav2wav %i, setting to value: 0\n", i); 
    }
  
  if (-1 != (i = cfg_get_int(section,"Machine_running_mp2enc")))
    {
      if ( i <= j )
        (*pointm).mp2enc = i;
      else if (verbose)
         printf("Wrong value for mp2enc %i, setting to value: 0\n", i); 
    }

  if (-1 != (i = cfg_get_int(section,"Machine_running_lav2yuv")))
    {
      if ( i <= j )
        (*pointm).lav2yuv = i;
      else if (verbose) 
         printf("Wrong value for lav2yuv %i, setting to value: 0\n", i); 
    }

  if (-1 != (i = cfg_get_int(section,"Machine_running_yuvdenoise")))
    {
      if ( i <= j )
        (*pointm).yuvdenoise = i;
      else if (verbose) 
         printf("Wrong value for yuvdenoise %i, setting to value: 0\n", i); 
    }

  if (-1 != (i = cfg_get_int(section,"Machine_running_yuvscaler")))
    {
      if ( i <= j )
        (*pointm).yuvscaler = i;
      else if (verbose) 
         printf("Wrong value for yuvscaler %i, setting to value: 0\n", i); 
    }

  if (-1 != (i = cfg_get_int(section,"Machine_running_mpeg2enc")))
    {
      if ( i <= j )
        (*pointm).mpeg2enc = i;
      else if (verbose) 
         printf("Wrong value for mpeg2enc %i, setting to value: 0\n", i); 
    }

  if (-1 != (i = cfg_get_int(section,"Machine_running_yuv2divx")))
    {
      if ( i <= j )
        (*pointm).yuv2divx = i;
      else if (verbose) 
         printf("Wrong value for yuv2divx %i, setting to value: 0\n", i); 
    }

  if (-1 != (i = cfg_get_int(section,"Machine_running_yuv2lav")))
    {
      if ( i <= j )
        (*pointm).yuv2lav = i;
      else if (verbose) 
         printf("Wrong value for yuv2lav %i, setting to value: 0\n", i); 
    }


}

/* load the Machine names used for distributed encoding */
void load_machine_names()
{
char *val;
int i;
char test[50];

  g_list_free (machine_names);
  machine_names = NULL;
  machine_names = g_list_append (machine_names, "localhost");

  for ( i=1; i < 50 ; i++)
  {
    sprintf(test,"Encode_Machine_%i",i);
 
    if ( NULL != (val = cfg_get_str("Machinenames",test)) )
       machine_names = g_list_append (machine_names, val);
    else
       break; 
  }

   machine_names = g_list_first (machine_names);
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

  if (-1 != (i = cfg_get_int("Studio","Encoding_four_pel_motion_compensation")))
    fourpelmotion = i;

  if (-1 != (i = cfg_get_int("Studio","Encoding_two_pel_motion_compensation")))
    twopelmotion = i;

  if (NULL != (val = cfg_get_str("Studio","Encode_Bicubic_Scaling")))
    if ( 0 == strcmp(val,"yes"))
        use_yuvplay_pipe = 1;
    else 
        use_yuvplay_pipe = 0;
  
  if (-1 != (i = cfg_get_int("Studio","Encoding_save_on_exit")))
    saveonexit = i;
  
  if (-1 != (i = cfg_get_int("Studio","Encoding_dist_enhanced_settings")))
    enhanced_settings = i;
}

void load_section(char section[LONGOPT],struct encodingoptions *point)
{
char *val;
int i;

  if (NULL != (val = cfg_get_str(section,"Encode_Notblack_size")))
        sprintf((*point).notblacksize, val);
  else
      sprintf((*point).notblacksize,"as is");

  if (NULL != (val = cfg_get_str(section,"Encode_Interlacing_Correction")))
      sprintf((*point).interlacecorr, val);
  else
      sprintf((*point).interlacecorr,"not needed");

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

  if (-1 != (i = cfg_get_int(section,"Encode_Audiobitrate")))
        (*point).audiobitrate = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Outputbitrate")))
        (*point).outputbitrate = i;

  if (NULL != (val = cfg_get_str(section,"Encode_Force_Stereo")))
    if ( 0 != strcmp(val,"as is"))
      sprintf((*point).forcestereo, val);

  if (NULL != (val = cfg_get_str(section,"Encode_Force_Mono")))
    if ( 0 != strcmp(val,"as is"))
      sprintf((*point).forcemono, val);

  if (NULL != (val = cfg_get_str(section,"Encode_Force_Vcd")))
    if ( 0 != strcmp(val,"as is"))
      sprintf((*point).forcevcd, val);

  if (-1 != (i = cfg_get_int(section,"Encode_Use_Yuvdenoise")))
        (*point).use_yuvdenoise = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Denoise_Deinterlace")))
        (*point).deinterlace = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Denoise_Sharpness")))
        (*point).sharpness = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Denoise_Threshold")))
        (*point).denois_thhold = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Denoise_Average")))
        (*point).average_frames = i;

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
    if ( 0 != strcmp(val,"from stream"))
      sprintf((*point).muxvbr, val);

  if (-1 != (i = cfg_get_int(section,"Encode_Stream_Datarate")))
        (*point).streamdatarate = i;

  if (-1 != (i = cfg_get_int(section,"Encode_Decoder_Buffer")))
        (*point).decoderbuffer = i;

  if (NULL != (val = cfg_get_str(section,"Encode_Codec")))
    if ( 0 != strcmp(val,"not used"))
      sprintf((*point).codec, val);
}

void load_script_data(void)
{
int i;
char *val;

  if ( NULL != (val = cfg_get_str("Scriptdata","Script_name")))
    sprintf(script_name, val);
  else 
    sprintf(script_name,"script.sh");

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_distributed")))
    if ( (i == 0) || (i == 1) )
      script_use_distributed = i;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_MPEG1")))
    if ( i >= 0 || i <= 8)
      script.mpeg1 = i;;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_MPEG2")))
    if ( i >= 0 || i <= 8)
      script.mpeg2 = i;;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_GENERIC")))
    if ( i >= 0 || i <= 8)
      script.generic = i;;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_VCD")))
    if ( i >= 0 || i <= 8)
      script.vcd = i;;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_SVCD")))
    if ( i >= 0 || i <= 8)
      script.svcd = i;;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_DVD")))
    if ( i >= 0 || i <= 8)
      script.dvd = i;;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_DIVX")))
    if ( i >= 0 || i <= 8)
      script.divx = i;;

  if ( -1 != (i = cfg_get_int("Scriptdata","Script_YUV2LAV")))
    if ( i >= 0 || i <= 8)
      script.yuv2lav = i;;

}

/* Print the names of the machines */
void print_machine_names(void)
{
char machnames[1024];
int i;

for (i=0;i<1024;i++)
  machnames[i] = '\0';

  for(i=0; i<g_list_length(machine_names); i++)
    {
      sprintf(machnames, "%s%i=%s%s", machnames, i,
      (char*) g_list_nth_data(machine_names, i),
      (i==g_list_length(machine_names)-1)?"":", ");
    }

  machine_names = g_list_first (machine_names);

  printf("\nMachine names: %s\n", machnames);
}

/* Print the loaded machine data */
void print_machine_data(char section[LONGOPT], struct machine *pointm)
{
int i, len;
char temp[LONGOPT];

i=0;
len=0;

for (i = 0; i < LONGOPT; i++)
  temp[i]='\0';

  len = strlen (section);  

  for ( i = 9; i < len ; i++)
     sprintf( temp,"%s%c",temp, section[i] ); 

  printf("\nMachines set for the %s process \n", temp);
  printf("Machine for lav2wav %i,  for mp2enc %i,  for lav2yuv %i, \
  for yuvdenoise %i \n", (*pointm).lav2wav, (*pointm).mp2enc, 
          (*pointm).lav2yuv, (*pointm).yuvdenoise);
  printf("Machine for yuvscaler %i,  for mpeg2enc %i,  for yuv2divx %i, \
  for yuv2lav %i \n", (*pointm).yuvscaler, (*pointm).mpeg2enc, 
          (*pointm).yuv2divx, (*pointm).yuv2lav);
}

/* show the current options */
void print_encoding_options(char section[LONGOPT], struct encodingoptions *point)
{
  printf("\n Encoding options of %s \n",section);
  printf("Encoding Active Window set to \'%s\' \n",    (*point).notblacksize);
  printf("Encoding Interlacing correction \'%s\' \n", (*point).interlacecorr);
  printf("Encoding Input use to \'%s\' \n",               (*point).input_use);
  printf("Encoding Output size to \'%s\' \n",           (*point).output_size);
  printf("Encoding yvscaler scaling Mode to \'%s\' \n",(*point).mode_keyword);
  printf("Encoding Audio Bitrate : \'%i\' \n",         (*point).audiobitrate);
  printf("Encoding Audio Samplerate : \'%i\' \n",     (*point).outputbitrate);
  printf("Encoding Audio force stereo : \'%s\' \n",     (*point).forcestereo);
  printf("Encoding Audio force mono : \'%s\' \n",         (*point).forcemono);
  printf("Encoding Audio force VCD : \'%s\' \n",           (*point).forcevcd);
  printf("Encoding Use yuvdenoise : \'%i\' \n",      (*point).use_yuvdenoise);
  printf("Encoding Deinterlace : \'%i\' \n",            (*point).deinterlace);
  printf("Encoding Denoise sharpness : \'%i\' \n",        (*point).sharpness);
  printf("Encoding Denoise threshold : \'%i\' \n",    (*point).denois_thhold);
  printf("Encoding Denoise average : \'%i\' \n",     (*point).average_frames);
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
  printf("Encoding Codec : \'%s\' \n",                        (*point).codec);
}

/* Show the common variabels if wanted/needed */
void show_common()
{
  printf("Encode input file set to \'%s\' \n",enc_inputfile);
  printf("Encode output file set to \'%s\' \n",enc_outputfile);
  printf("Encode input file set to \'%s\' \n",enc_audiofile);
  printf("Encode video file set to \'%s\' \n",enc_videofile);
  printf("Video player set to \'%s\' \n",selected_player);
  printf("Encoding Preview with yuvplay : \'%i\' \n",use_yuvplay_pipe);
  printf("Encoding 4*4-pel motion compensation :\'%i\' \n",fourpelmotion);
  printf("Encoding 2*2-pel motion compensation :\'%i\' \n",twopelmotion);
  printf("Encoding save on exit :\'%i\' \n",saveonexit);
  printf("Encoding distributed enhanced settings :\'%i\' \n",enhanced_settings);

}

void load_config_encode()
{
struct encodingoptions *point;
struct machine *pointm;
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

  point = &encoding_gmpeg;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_mpeg2(point);     /* set some mpeg2 specific options */

  point = &encoding_vcd;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_vcd(point);     /* set some VCD specific options */

  point = &encoding_svcd;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_svcd(point);     /* set some SVCD specific options */

  point = &encoding_dvd;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_dvd(point);     /* set some DVD specific options */

  point = &encoding_divx;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_divx(point);     /* set some DIVX specific options */

  point = &encoding_yuv2lav;
  set_structs_default(point);  /* set struct to the defaults */
  do_preset_yuv2lav(point);     /* set some DIVX specific options */

  set_distributed(); /*set the distributed encoding variabels to the defaults*/

  /* set the defaults for the scripting options */
  set_scripting_defaults ();

  set_common();
  load_common();
  
  if (verbose)
    show_common();

  strncpy(section,"MPEG1",LONGOPT);
  point = &encoding; 
  load_section(section,point);
  if (verbose) 
    print_encoding_options(section,point);

  strncpy(section,"MPEG2",LONGOPT);
  point = &encoding2; 
  load_section(section,point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"GENERIC",LONGOPT);
  point = &encoding_gmpeg; 
  load_section(section,point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"VCD",LONGOPT);
  point = &encoding_vcd; 
  load_section(section,point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"SVCD",LONGOPT);
  point = &encoding_svcd; 
  load_section(section,point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"DVD",LONGOPT);
  point = &encoding_dvd; 
  load_section(section,point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"DIVX",LONGOPT);
  point = &encoding_divx; 
  load_section(section,point);
  if (verbose)
    print_encoding_options(section,point);

  strncpy(section,"YUV2LAV",LONGOPT);
  point = &encoding_yuv2lav; 
  load_section(section,point);
  if (verbose)
    print_encoding_options(section,point);

  load_machine_names();  /* fill the GList with machine names */
  if (verbose)
    print_machine_names(); 
 
  strncpy(section,"Machines4MPEG1",LONGOPT);
  pointm = &machine4mpeg1; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  strncpy(section,"Machines4MPEG2",LONGOPT);
  pointm = &machine4mpeg2; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  strncpy(section,"Machines4GENERIC",LONGOPT);
  pointm = &machine4mpeg2; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  strncpy(section,"Machines4VCD",LONGOPT);
  pointm = &machine4vcd; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  strncpy(section,"Machines4SVCD",LONGOPT);
  pointm = &machine4svcd; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  strncpy(section,"Machines4DVD",LONGOPT);
  pointm = &machine4svcd; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  strncpy(section,"Machines4DIVX",LONGOPT);
  pointm = &machine4divx; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  strncpy(section,"Machines4YUV2LAV",LONGOPT);
  pointm = &machine4yuv2lav; 
  load_machine_data(section, pointm);
  if (verbose)
    print_machine_data(section,pointm);

  load_script_data();

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

  fprintf(fp,"Encoding_four_pel_motion_compensation = %i\n", fourpelmotion);
  fprintf(fp,"Encoding_two_pel_motion_compensation = %i\n", twopelmotion);

  if (use_bicubic == 1)
    fprintf(fp,"Encode_Bicubic_Scaling = %s\n", "yes");
  else
    fprintf(fp,"Encode_Bicubic_Scaling = %s\n", "no");
  
  if (saveonexit == 2)
    saveonexit = 0;  /* part 2 of the workaround, take a look accept_options */
  fprintf(fp,"Encoding_save_on_exit = %i\n",saveonexit);

  fprintf(fp,"Encoding_dist_enhanced_settings = %i\n",enhanced_settings);

}

void save_section(FILE *fp, struct encodingoptions *point, char section[LONGOPT])
{
  fprintf(fp,"[%s]\n",section);

  if (strlen ((*point).notblacksize) > 0)
    fprintf(fp,"Encode_Notblack_size = %s\n", (*point).notblacksize);
  else
    fprintf(fp,"Encode_Notblack_size = %s\n", "as is");

  if (strlen ((*point).notblacksize) > 0)
    fprintf(fp,"Encode_Interlacing_Correction = %s\n", (*point).interlacecorr);
  else
    fprintf(fp,"Encode_Interlacing_Correction = %s\n", "not needed");

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

  fprintf(fp,"Encode_Use_Yuvdenoise = %i\n", (*point).use_yuvdenoise);
  fprintf(fp,"Encode_Denoise_Deinterlace = %i\n", (*point).deinterlace);
  fprintf(fp,"Encode_Denoise_Sharpness = %i\n", (*point).sharpness);
  fprintf(fp,"Encode_Denoise_Threshold = %i\n", (*point).denois_thhold);
  fprintf(fp,"Encode_Denoise_Average = %i\n", (*point).average_frames);
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

  if (strlen((*point).codec) > 0)
    fprintf(fp,"Encode_Codec = %s\n", (*point).codec);
  else 
    fprintf(fp,"Encode_Codec = not used\n");
}

void save_machine_section(FILE *fp,struct machine *point, char section[LONGOPT])
{
  fprintf(fp,"[%s]\n",section);

  fprintf(fp, "Machine_running_lav2wav = %i\n", (*point).lav2wav);
  fprintf(fp, "Machine_running_mp2enc = %i\n", (*point).mp2enc);
  fprintf(fp, "Machine_running_lav2yuv = %i\n", (*point).lav2yuv);
  fprintf(fp, "Machine_running_yuvdenoise = %i\n", (*point).yuvdenoise);
  fprintf(fp, "Machine_running_yuvscaler = %i\n", (*point).yuvscaler);
  fprintf(fp, "Machine_running_mpeg2enc = %i\n", (*point).mpeg2enc);
  fprintf(fp, "Machine_running_yuv2divx = %i\n", (*point).yuv2divx);
  fprintf(fp, "Machine_running_yuv2lav = %i\n", (*point).yuv2lav);
}

/* Saving the Glist with all the machine names and the machine options */
void save_machine_data(FILE *fp, char section[LONGOPT])
{
struct machine *point;
int i, length;
char test[50];

  fprintf(fp,"\n[%s]\n",section);

  machine_names = g_list_first(machine_names);
  length = g_list_length(machine_names);

  if (length > 1) 
  { 
    for ( i=1; i < length ;i++)
      {
       strcpy(test,g_list_nth_data(machine_names,i));
       fprintf(fp,"Encode_Machine_%i = %s\n", i, test); 
      }
  }

  point = &machine4mpeg1;
  save_machine_section(fp,point,"Machines4MPEG1");
  point = &machine4mpeg2;
  save_machine_section(fp,point,"Machines4MPEG2");
  point = &machine4generic;
  save_machine_section(fp,point,"Machines4GENERIC");
  point = &machine4vcd;
  save_machine_section(fp,point,"Machines4VCD");
  point = &machine4svcd;
  save_machine_section(fp,point,"Machines4SVCD");
  point = &machine4dvd;
  save_machine_section(fp,point,"Machines4DVD");
  point = &machine4divx;
  save_machine_section(fp,point,"Machines4DIVX");
  point = &machine4yuv2lav;
  save_machine_section(fp,point,"Machines4YUV2LAV");

}

/* Save the Data used fo script generation */
void save_script_data(FILE *fp, char section[LONGOPT])
{

  fprintf(fp, "[%s]\n",section);

  fprintf(fp, "Script_name = %s\n", script_name);
  fprintf(fp, "Script_distributed = %i\n", script_use_distributed);
  fprintf(fp, "Script_MPEG1 = %i\n", script.mpeg1);
  fprintf(fp, "Script_MPEG2 = %i\n", script.mpeg2);
  fprintf(fp, "Script_GENERIC = %i\n", script.generic);
  fprintf(fp, "Script_VCD = %i\n", script.vcd);
  fprintf(fp, "Script_SVCD = %i\n", script.svcd);
  fprintf(fp, "Script_DVD = %i\n", script.dvd);
  fprintf(fp, "Script_DIVX = %i\n", script.divx);
  fprintf(fp, "Script_YUV2LAV = %i\n", script.yuv2lav);

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
 
  point = &encoding_gmpeg; 
  save_section(fp,point,"GENERIC");
 
  point = &encoding_vcd; 
  save_section(fp,point,"VCD");
 
  point = &encoding_svcd; 
  save_section(fp,point,"SVCD");
 
  point = &encoding_dvd; 
  save_section(fp,point,"DVD");
 
  point = &encoding_divx; 
  save_section(fp,point,"DIVX");
 
  point = &encoding_yuv2lav; 
  save_section(fp,point,"YUV2LAV");

  save_machine_data(fp,"Machinenames");

  save_script_data(fp,"Scriptdata");
 
  if (verbose) printf("Configuration of the encoding options saved\n");

  fclose(fp);
}

/* This is done the the OK Button was pressed */
void accept_encoptions(GtkWidget *widget, gpointer data)
{
  if (verbose)
    {
      printf ("\nThe OK button in the Options was pressed, changing options that effect the encoding:\n");
      if (t_use_yuvplay_pipe != use_yuvplay_pipe)
        printf(" Encoding Preview set to %i \n",t_use_yuvplay_pipe);

      if (t_addoutputnorm != encoding.addoutputnorm)
        printf(" Output norm is added %i \n",t_addoutputnorm);

      if (t_use_bicubic != use_bicubic)
        printf(" For scaling use bicubic %i \n", t_use_bicubic);

      if (t_fourpelmotion != fourpelmotion)
        printf(" 4*4-pel subsampled motion compensation : %i \n", t_fourpelmotion);
      if (t_twopelmotion != twopelmotion)
        printf(" 2*2-pel subsampled motion compensation : %i\n",t_twopelmotion);
      
      if (strcmp(t_selected_player,selected_player) != 0)
        printf(" Setting the player to : %s \n", t_selected_player);

      if (t_saveonexit != saveonexit)
        printf(" Save on when exit set to  %i \n", t_saveonexit);
    }

  use_yuvplay_pipe = t_use_yuvplay_pipe;
  encoding.addoutputnorm = t_addoutputnorm;
  use_bicubic = t_use_bicubic;
  fourpelmotion = t_fourpelmotion;
  twopelmotion = t_twopelmotion;
  strcpy(selected_player,t_selected_player);

  /* how is the configuration saven when you dont want this feature ? */
  /* this should solve the problem properly, als check save_common () */
  if ( (t_saveonexit == 0) && (saveonexit == 1))
     saveonexit = 2;
  else  
     saveonexit =t_saveonexit;
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

/* Set if the saveing of the option when exiting */
void set_saveonexit(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_saveonexit = 1;
  else
    t_saveonexit = 0;
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

/* Here the fist page of the layout is created */
void create_encoding_layout(GtkWidget *table)
{
GtkWidget *preview_button, *label, *addnorm_button;
GtkWidget *bicubic_button, *player_field, *saveonexit_button;
GtkObject *adjust_scale, *adjust_scale_n;
int table_line;

table_line = 0;

  t_use_yuvplay_pipe = use_yuvplay_pipe;
  t_addoutputnorm = encoding.addoutputnorm;
  t_use_bicubic = use_bicubic; 
  t_fourpelmotion = fourpelmotion;
  t_twopelmotion = twopelmotion;
  sprintf(t_selected_player, "%s", selected_player);
  t_saveonexit = saveonexit;

  label = gtk_label_new ("Save the encoding options when exiting : ");
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             label, 0, 1, table_line, table_line+1);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
  gtk_widget_show (label);
  saveonexit_button = gtk_check_button_new();
  gtk_widget_ref (saveonexit_button);
  if (saveonexit != 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (saveonexit_button), TRUE);
  gtk_signal_connect (GTK_OBJECT (saveonexit_button), "toggled",
                      GTK_SIGNAL_FUNC (set_saveonexit), NULL );
  gtk_table_attach_defaults (GTK_TABLE (table), 
                            saveonexit_button, 1, 2, table_line, table_line+1);
  gtk_widget_show (saveonexit_button);
  table_line++;

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
  gtk_table_attach_defaults (GTK_TABLE (table), 
                             player_field, 1, 2, table_line, table_line+1);
  gtk_widget_show (player_field);
}

