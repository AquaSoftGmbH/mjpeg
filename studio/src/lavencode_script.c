/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * lavencode_script done by Bernhard Praschinger
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
 * Here we create scripts with the options used for encoding.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "studio.h"
#include "pipes.h"   /* needed for the funktion app_name */

#define bit_audio 1  /* here we define the bits that should be used for */
#define bit_video 2  /* comparing a script "job" */
#define bit_mplex 4
#define bit_full  8


/* Forward declarations */
void init_temp(void);
void create_scriptname(GtkWidget *hbox);
void use_distributed(GtkWidget *hbox);
void script_callback(GtkWidget *widget, GtkWidget *script_filename);
void script_select_dialog(GtkWidget *widget, gpointer data);
void file_ok_script(GtkWidget *w, GtkFileSelection *fs);
void set_usage (GtkWidget *widget, gpointer data);
void create_ok_cancel(GtkWidget *hbox, GtkWidget *script_window);
void accept_changes(GtkWidget *widget, gpointer data);
void create_scriptfield(GtkWidget *hbox); 
void create_script_button(GtkWidget *hbox); 
void command_2string(char **command, char *string);
void create_command_mp2enc(char *mp2enc_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void create_command_lav2wav(char *lav2wav_command[256], int use_rsh,
           struct encodingoptions *option, struct machine *machine4);
void create_command_lav2yuv(char *lav2yuv_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4);
void create_command_yuvscaler(char *yuvscaler_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4);
void create_command_yuvdenoise(char *yuvdenoise_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4);
void create_command_mp2enc(char *mp2enc_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void create_command_yuv2divx(char* yuv2divx_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void create_command_yuv2lav(char* yuv2divx_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void create_command_mplex(char* mplex_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void filename_ext(char ext[LONGOPT], char *extended_name, char filename[100]);


/* Only for this file, same names used in other files */ 
static void set_up_defaults(void);
static void create_checkbox_mpeg1(GtkWidget *table);
static void create_checkbox_mpeg2(GtkWidget *table);
static void create_checkbox_vcd(GtkWidget *table);
static void create_checkbox_svcd(GtkWidget *table);
static void create_checkbox_divx(GtkWidget *table);
static void create_checkbox_yuv2lav(GtkWidget *table);
static void set_mpeg1(GtkWidget *widget, gpointer data);
static void set_mpeg2(GtkWidget *widget, gpointer data);
static void set_vcd(GtkWidget *widget, gpointer data);
static void set_svcd(GtkWidget *widget, gpointer data);
static void set_divx(GtkWidget *widget, gpointer data);
static void set_yuv2lav(GtkWidget *widget, gpointer data);
static void create_script(GtkWidget *widget, gpointer data);
static void create_audio(FILE *fp, struct encodingoptions *option, 
                            struct machine *machine4, char ext[LONGOPT]);
static void create_video(FILE *fp, struct encodingoptions *option,
                            struct machine *machine4, char ext[LONGOPT]);
static void create_mplex(FILE *fp, struct encodingoptions *option,
                            struct machine *machine4, char ext[LONGOPT]);

/* Some variables defined here */
GtkWidget *script_filename, *check_usage;
GtkWidget *mpeg1_a, *mpeg1_v, *mpeg1_m, *mpeg1_f;
GtkWidget *mpeg2_a, *mpeg2_v, *mpeg2_m, *mpeg2_f;
GtkWidget *vcd_a, *vcd_v, *vcd_m, *vcd_f;
GtkWidget *svcd_a, *svcd_v, *svcd_m, *svcd_f;
GtkWidget *divx_f, *yuv2lav_v;
int temp_use_distributed;
char temp_scriptname[FILELEN];
struct f_script t_script;

/* used from config.c */
int chk_dir(char *name);

/* =============================================================== */
/* Start of the code */

/* Here we set up the values in the boxes*/
void set_up_defaults(void)
{
  gtk_entry_set_text(GTK_ENTRY(script_filename), script_name);

  if ( temp_use_distributed == 1)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(check_usage), TRUE);

  if ( (t_script.mpeg1 & bit_audio ) == bit_audio )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg1_a), TRUE);
  if ( (t_script.mpeg1 & bit_video ) == bit_video)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg1_v), TRUE);
  if ( (t_script.mpeg1 & bit_mplex ) == bit_mplex )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg1_m), TRUE);
  if ( (t_script.mpeg1 & bit_full  ) == bit_full  )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg1_f), TRUE);

  if ( (t_script.mpeg2 & bit_audio ) == bit_audio )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg2_a), TRUE);
  if ( (t_script.mpeg2 & bit_video ) == bit_video )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg2_v), TRUE);
  if ( (t_script.mpeg2 & bit_mplex ) == bit_mplex )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg2_m), TRUE);
  if ( (t_script.mpeg2 & bit_full  ) == bit_full  )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mpeg2_f), TRUE);

  if ( (t_script.vcd & bit_audio ) == bit_audio )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(vcd_a), TRUE);
  if ( (t_script.vcd & bit_video ) == bit_video )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(vcd_v), TRUE);
  if ( (t_script.vcd & bit_mplex ) == bit_mplex )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(vcd_m), TRUE);
  if ( (t_script.vcd & bit_full  ) == bit_full  )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(vcd_f), TRUE);

  if ( (t_script.svcd & bit_audio ) == bit_audio )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(svcd_a), TRUE);
  if ( (t_script.svcd & bit_video ) == bit_video )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(svcd_v), TRUE);
  if ( (t_script.svcd & bit_mplex ) == bit_mplex )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(svcd_m), TRUE);
  if ( (t_script.svcd & bit_full  ) == bit_full  )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(svcd_f), TRUE);

  if ( (t_script.divx & bit_full  ) == bit_full  )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(divx_f), TRUE);

  if ( (t_script.yuv2lav & bit_video ) == bit_video )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(yuv2lav_v), TRUE);
}

/* Set the selcted script filename */ 
void file_ok_script(GtkWidget *w, GtkFileSelection *fs)
{
  gtk_entry_set_text (GTK_ENTRY(script_filename),
                   gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Create a file selection widget for the script file name */
void script_select_dialog(GtkWidget *widget, gpointer data)
{
GtkWidget *filew;

  filew = gtk_file_selection_new("Linux Video Studio - Script File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_script, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(filew)->cancel_button)
         , "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);

}

/* Here we set the filname used for the script */
void script_callback(GtkWidget *widget, GtkWidget *script_filename)
{
gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(script_filename));
  strcpy(temp_scriptname,file);

  if (verbose)
    printf("Script filename set to: %s\n", temp_scriptname);

}

/* Here we create the script name dialog */
void create_scriptname (GtkWidget *hbox)
{
GtkWidget *label, *script_select;

  label = gtk_label_new (" Script Name : ");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  script_filename = gtk_entry_new ();
  gtk_signal_connect (GTK_OBJECT(script_filename), "changed",
                      GTK_SIGNAL_FUNC(script_callback), script_filename);
  gtk_widget_set_usize (script_filename, 200, -2);
  gtk_box_pack_start (GTK_BOX(hbox), script_filename, FALSE, FALSE, 0);
  gtk_widget_show (script_filename);

  script_select = gtk_button_new_with_label ("Select");
  gtk_signal_connect(GTK_OBJECT(script_select), "clicked",
                     GTK_SIGNAL_FUNC(script_select_dialog), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), script_select, FALSE, FALSE, 5);
  gtk_widget_show(script_select);
 
}

/**
   Here we add to the correct part of a filename the extension.
   @param the extension to add
   @param the extended filename
   @param the original filename                                   */
void filename_ext(char ext[LONGOPT], char *extended_name, char filename[100])
{
char temp1[200], temp2[100];
int i,j;

i = 0;
j = 0;
 
for ( i = 0; i < 200; i++)
   temp1[i]='\0';
for ( i = 0; i < 100; i++)
   temp2[i]='\0';
 
  i = strlen(enc_audiofile);
  for( i=0; i < strlen(filename); i++)
    {
      if (filename[i] == '/')
        j = i+1;
    }
  if (j != 0)
    {
       strncpy(temp1,filename,j);
       for (i = j; i < strlen(filename); i++)
          sprintf(temp2,"%s%c",temp2,filename[i]);
       sprintf(extended_name,"%s%s.%s", temp1, ext, temp2);
         }
  else
    sprintf(extended_name,"%s.%s", ext, filename);
}

/**
   Here we create a sting out of the command given
   @param command The ** means that we can change the pointer to the char 
   pointer. 
   @param string there will be the output of the content of the command 
   ( = pointer to char field ) */
void command_2string(char **command, char *string )
{
int i;
i=0;

  string[0] = '\0';
  for (i=0; command[i] != NULL; i++)
   sprintf(string, "%s %s", string, command[i]);
}

/* Here we create the audio encoding command */
void create_command_mp2enc(char *mp2enc_command[256], int use_rsh,
   struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT])
{
int i,j,n;
static char temp1[16], temp2[16], temp3[200];

i = 0;
j = 0;
n = 0;

  if ((use_rsh ==1) && ((machine4mpeg1.mp2enc !=0) || ((*machine4).mp2enc !=0)))
    {
      mp2enc_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           mp2enc_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.mp2enc); n++;
        }
      else 
        {
          mp2enc_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).mp2enc); n++;
        }
    }

  mp2enc_command[n] = app_name(MP2ENC); n++;
  mp2enc_command[n] = "-v 2"; n++;
  if ((*option).forcevcd[0] == '-')
    {
       mp2enc_command[n] = (*option).forcevcd;
       n++;
    }
  else                   /* If VCD is set no other Options are needed */
    {
       mp2enc_command[n] = "-b"; n++;
       sprintf(temp1,"%i", (*option).audiobitrate);
       mp2enc_command[n] = temp1; n++;
       mp2enc_command[n] = "-r"; n++;
       sprintf(temp2, "%i00", (*option).outputbitrate);
       mp2enc_command[n] = temp2; n++;
       if ((*option).forcemono[0] == '-')
         {
           mp2enc_command[n] = (*option).forcemono;
           n++;
         }
       if ((*option).forcestereo[0] == '-')
         {
           mp2enc_command[n] = (*option).forcestereo;
           n++;
         }
     }
  mp2enc_command[n] = "-o"; n++;         /* Setting output file name */
  if (strlen(ext) != 0)
       filename_ext(ext, temp3, enc_audiofile);
  else 
    sprintf(temp3,"%s",enc_audiofile);

  mp2enc_command[n] = temp3; n++;
  mp2enc_command[n] = NULL;
}

/* Here the yuv2lav command is set together with all options */
void create_command_yuv2lav(char* yuv2lav_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT])
{
int n; 
static char temp1[4], temp2[4], temp3[6], temp4[256];
n=0;

  if ((use_rsh ==1)&&((machine4mpeg1.yuv2lav!=0)||((*machine4).yuv2lav!=0)))
  {
      yuv2lav_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           yuv2lav_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.yuv2lav); n++;
        }
      else
        {
          yuv2lav_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).yuv2lav); n++;
        }
    }

yuv2lav_command[n] = app_name(YUV2LAV_E); n++;

if (strlen((*option).codec) > 3)
  { 
    yuv2lav_command[n] = "-f"; n++;

    if (strcmp ((*option).codec, "AVI fields reversed") == 0)
      yuv2lav_command[n] = "A";
    else if (strcmp ((*option).codec, "Quicktime") == 0)
      yuv2lav_command[n] = "q";
    else if (strcmp ((*option).codec, "Movtar") == 0)
      yuv2lav_command[n] = "m";

    n++;
  }

if ( (*option).minGop != 3 )
  {
    yuv2lav_command[n] = "-I"; n++;
    sprintf(temp1, "%i", (*option).minGop);
    yuv2lav_command[n] = temp1; n++;
  }

if ( (*option).qualityfactor != 80)
  {
    yuv2lav_command[n] = "-q"; n++;
    sprintf(temp2, "%i", (*option).qualityfactor);
    yuv2lav_command[n] = temp2; n++;
  }

if ( (*option).sequencesize != 0)
  {
    yuv2lav_command[n] = "-m"; n++;
    sprintf(temp3, "%i", (*option).sequencesize);
    yuv2lav_command[n] = temp3; n++;
  }


  yuv2lav_command[n] = "-o"; n++;
  if (strlen(ext) != 0)
    filename_ext(ext, temp4, enc_videofile);
  else
    sprintf(temp4,"%s",enc_videofile);
  yuv2lav_command[n] = temp4; n++;
  yuv2lav_command[n] = NULL;

}

/* Here the yuv2divx command is set together with all options */
void create_command_yuv2divx(char* yuv2divx_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT])
{
int n;
static char temp1[4], temp2[4], temp3[256];
n=0;

  if ((use_rsh ==1)&&((machine4mpeg1.yuv2divx!=0)||((*machine4).yuv2divx!=0)))    {
      yuv2divx_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           yuv2divx_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.yuv2divx); n++;
        }
      else
        {
          yuv2divx_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).yuv2divx); n++;
        }
    }

  yuv2divx_command[n] = app_name(YUV2DIVX); n++;

  if ((*option).audiobitrate != 0)
    {
      yuv2divx_command[n] = "-a"; n++;
      sprintf(temp1, "%i", (*option).audiobitrate);
      yuv2divx_command[n] = temp1; n++;
    }

  yuv2divx_command[n] = "-A"; n++;
  yuv2divx_command[n] = enc_inputfile; n++;

  if ((*option).bitrate != 0)
    {
      yuv2divx_command[n] = "-b"; n++;
      sprintf(temp2, "%i", (*option).bitrate);
      yuv2divx_command[n] = temp2; n++;
    }

  yuv2divx_command[n] = "-E"; n++;
  yuv2divx_command[n] = (*option).codec; n++;

  yuv2divx_command[n] = "-o"; n++;
  if (strlen(ext) != 0)
    filename_ext(ext, temp3, enc_videofile);
  else
    sprintf(temp3,"%s",enc_videofile);
  yuv2divx_command[n] = temp3; n++;
  yuv2divx_command[n] = NULL;

}

/* Here we create the command for mpeg2enc */
void create_command_mpeg2enc(char* mpeg2enc_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT])
{
int n;
static char temp1[6], temp2[4], temp3[4], temp4[4], temp5[4], temp6[4];
static char temp7[4], temp8[4], temp9[4], temp10[4], temp11[4], temp12[256];

n=0;

  if ((use_rsh ==1)&&((machine4mpeg1.mpeg2enc!=0)||((*machine4).mpeg2enc!=0)))    {
      mpeg2enc_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           mpeg2enc_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.mpeg2enc); n++;
        }
      else
        {
          mpeg2enc_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).mpeg2enc); n++;
        }
    }

mpeg2enc_command[n] = app_name(MPEG2ENC); n++;
mpeg2enc_command[n] = "-v1"; n++;

if((*option).bitrate != 0) {
   mpeg2enc_command[n] = "-b"; n++;
   sprintf(temp1, "%i", (*option).bitrate);
   mpeg2enc_command[n] =  temp1; n++;
  }
if((*option).qualityfactor != 0) {
   mpeg2enc_command[n] = "-q"; n++;
   sprintf(temp2, "%i", (*option).qualityfactor);
   mpeg2enc_command[n] =  temp2; n++;
  }
if((*option).searchradius != 16) {
   sprintf(temp3, "%i", (*option).searchradius);
   mpeg2enc_command[n] =  "-r"; n++;
   mpeg2enc_command[n] =  temp3; n++;
  }
if ( fourpelmotion != 2 )
  {
    sprintf(temp4,"%i", fourpelmotion);
    mpeg2enc_command[n] = "-4"; n++;
    mpeg2enc_command[n] = temp4; n++;
  }
if ( twopelmotion != 3 )
  {
    sprintf(temp5,"%i", twopelmotion);
    mpeg2enc_command[n] = "-2"; n++;
    mpeg2enc_command[n] = temp5; n++;
  }
if((*option).decoderbuffer != 46) {
   sprintf(temp6, "%i", (*option).decoderbuffer);
   mpeg2enc_command[n] =  "-V"; n++;
   mpeg2enc_command[n] =  temp6; n++;
  }
if((*option).sequencesize != 0) {
   sprintf(temp7, "%i", (*option).sequencesize);
   mpeg2enc_command[n] =  "-S"; n++;
   mpeg2enc_command[n] =  temp7; n++;
  }
if(((*option).sequencesize != 0i) && ((*option).nonvideorate != 0)) {
   sprintf(temp8, "%i", (*option).nonvideorate);
   mpeg2enc_command[n] =  "-B"; n++;
   mpeg2enc_command[n] =  temp8; n++;
  }
if ( ( (*option).minGop != 12 ) &&
     ( (*option).minGop <= (*option).maxGop) ) {
   sprintf(temp9, "%i", (*option).minGop);
   mpeg2enc_command[n] =  "-g"; n++;
   mpeg2enc_command[n] =  temp9; n++;
  }
if ( ( (*option).maxGop != 12 ) &&
     ( (*option).minGop <= (*option).maxGop) ) {
   sprintf(temp10, "%i", (*option).maxGop);
   mpeg2enc_command[n] =  "-G"; n++;
   mpeg2enc_command[n] =  temp10; n++;
  }

if((*option).muxformat != 0)
  {
    sprintf(temp11, "%i",(*option).muxformat);
    mpeg2enc_command[n] =  "-f"; n++;
    mpeg2enc_command[n] =  temp11; n++;
  }
if((*option).muxformat >= 3)
  {
    mpeg2enc_command[n] = "-P";
    n++;
  }

/* And here again some common stuff */
  mpeg2enc_command[n] = "-o"; n++;
  if (strlen(ext) != 0)
    filename_ext(ext, temp12, enc_videofile);
  else 
    sprintf(temp12,"%s",enc_videofile);
  mpeg2enc_command[n] = temp12; n++;
  mpeg2enc_command[n] = NULL;

}

/* Here we create the yuvscaler command */
void create_command_yuvdenoise(char *yuvdenoise_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4)
{
static int n;
static char temp1[4], temp2[4], temp3[4];
n = 0;

  if((use_rsh==1)&&((machine4mpeg1.yuvdenoise!=0)||((*machine4).yuvdenoise!=0)))    {
      yuvdenoise_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           yuvdenoise_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.yuvdenoise); n++;
        }
      else
        {
          yuvdenoise_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).yuvdenoise); n++;
        }
    }

  yuvdenoise_command[n] = "yuvdenoise"; n++;

  if ((*option).deinterlace == 1)
    {
      yuvdenoise_command[n] = "-F"; n++;
    }
 
  if ((*option).sharpness != 125)
    {
      yuvdenoise_command[n] = "-S"; n++;
      sprintf(temp1,"%i",(*option).sharpness);
      yuvdenoise_command[n] = temp1; n++;
    } 

  if ((*option).denois_thhold != 5)
    {
      yuvdenoise_command[n] = "-t"; n++;
      sprintf(temp2,"%i",(*option).denois_thhold);
      yuvdenoise_command[n] = temp2; n++;
    }

  if ((*option).average_frames != 3)
    {
      yuvdenoise_command[n] = "-l"; n++;
      sprintf(temp3,"%i",(*option).average_frames);
      yuvdenoise_command[n] = temp3; n++;
    }
 
  yuvdenoise_command[n] = NULL;

}

/* Here we create the yuvscaler command */
void create_command_yuvscaler(char *yuvscaler_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4)
{
static int n;
static char temp1[24], temp2[24];
n = 0;

  if ((use_rsh ==1)&&((machine4mpeg1.yuvscaler!=0)||((*machine4).yuvscaler!=0)))    {
      yuvscaler_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           yuvscaler_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.yuvscaler); n++;
        }
      else
        {
          yuvscaler_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).yuvscaler); n++;
        }
    }

    yuvscaler_command[n] = app_name(YUVSCALER); n++;
    yuvscaler_command[n] = "-v 0"; n++;
    if (strlen((*option).input_use) > 0 &&
        strcmp((*option).input_use,"as is") )
      {
         yuvscaler_command[n] = "-I"; n++;
         sprintf(temp1,"USE_%s", (*option).input_use);
         yuvscaler_command[n] = temp1; n++;
      }
    if (strlen((*option).notblacksize) > 0 &&
        strcmp((*option).notblacksize,"as is") != 0)
      {
       yuvscaler_command[n] = "-I"; n++;
       sprintf(temp2,"ACTIVE_%s", (*option).notblacksize);
       yuvscaler_command[n] = temp2; n++;
      }
    if (use_bicubic == 1)
      {
         yuvscaler_command[n] = "-M"; n++;
         yuvscaler_command[n] = "BICUBIC"; n++;
      }
    if (strcmp((*option).mode_keyword,"as is") != 0)
      {
         yuvscaler_command[n] = "-M"; n++;
         yuvscaler_command[n] = (*option).mode_keyword; n++;
      }
    if( strcmp((*option).interlacecorr,"not needed") != 0)
      {
       yuvscaler_command[n] = "-M"; n++;
        
       if (strcmp((*option).interlacecorr,"exchange fields") == 0)
           yuvscaler_command[n] = "LINE_SWITCH"; 
       if (strcmp((*option).interlacecorr,"shift bottom field forward") == 0)
           yuvscaler_command[n] = "BOTT_FORWARD"; 
       if (strcmp((*option).interlacecorr,"shift top field forward") == 0)
           yuvscaler_command[n] = "TOP_FORWARD"; 
       if (strcmp((*option).interlacecorr,"interlace top first") == 0)
           yuvscaler_command[n] = "INTERLACED_TOP_FIRST"; 
       if (strcmp((*option).interlacecorr,"interlace bottom first") == 0)
           yuvscaler_command[n] = "INTERLACED_BOTTOM_FIRST";  
       if (strcmp((*option).interlacecorr,"not interlaced") == 0)
           yuvscaler_command[n] = "NOT_INTERLACED";

         n++;
      }

    if (strlen((*option).output_size) > 0 &&
          strcmp((*option).output_size,"as is") )
      {
         yuvscaler_command[n] = "-O"; n++;
         yuvscaler_command[n] = (*option).output_size; n++;
      }

    yuvscaler_command[n] = NULL;
}

/* Here we create the lav2yuv command */
void create_command_lav2yuv(char *lav2yuv_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4)
{
int n;
n = 0;

  if ((use_rsh ==1) && ((machine4mpeg1.lav2yuv!=0) || ((*machine4).lav2yuv!=0)))    {
      lav2yuv_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           lav2yuv_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.lav2yuv); n++;
        }
      else
        {
          lav2yuv_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).lav2yuv); n++;
        }
    }

   lav2yuv_command[n] = "lav2yuv"; n++;
   lav2yuv_command[n] = enc_inputfile; n++;
   lav2yuv_command[n] = NULL;
}

/* Here we create the lav2wav command */
void create_command_lav2wav(char *lav2wav_command[256], int use_rsh,
           struct encodingoptions *option, struct machine *machine4)
{
int n;
n = 0;

  if ((use_rsh ==1) && ((machine4mpeg1.lav2wav!=0) || ((*machine4).lav2wav!=0)))    {
      lav2wav_command[n] = "rsh"; n++;
      if (enhanced_settings == 0)
        {
           lav2wav_command[n] = (char*)
                g_list_nth_data(machine_names, machine4mpeg1.lav2wav); n++;
        }
      else
        {
          lav2wav_command[n] = (char*)
                g_list_nth_data(machine_names, (*machine4).lav2wav); n++;
        }
    }

  lav2wav_command[n] = app_name(LAV2WAV); n++;
  lav2wav_command[n] = enc_inputfile; n++;
  lav2wav_command[n] = NULL;
}

/* Here we create the command for the audio encoding */
void create_audio(FILE *fp, struct encodingoptions *option, 
                            struct machine *machine4, char ext[LONGOPT])
{
char *mp2enc_command[256];
char *lav2wav_command[256];
char mp2enc_string[256];
char lav2wav_string[256];

  create_command_lav2wav(lav2wav_command,temp_use_distributed,option, machine4);
  command_2string(lav2wav_command, lav2wav_string);
 
  create_command_mp2enc(mp2enc_command, temp_use_distributed, option, machine4,
              ext);
  command_2string(mp2enc_command, mp2enc_string);

  fprintf(fp," %s | %s\n", lav2wav_string, mp2enc_string);

}

/* Here we create the command for the video encoding */
void create_video(FILE *fp, struct encodingoptions *option,
		struct machine *machine4, char ext[LONGOPT])
{
char *lav2yuv_command[256];
char *yuvscaler_command[256];
char *yuvdenoise_command[256];
char *mpeg2enc_command[256];
char *yuv2divx_command[256];
char *yuv2lav_command[256];
char lav2yuv_string[800];
char yuvscaler_string[256];
char yuvdenoise_string[256];
char mpeg2enc_string[256];
char yuv2divx_string[256];
char yuv2lav_string[256];

create_command_lav2yuv(lav2yuv_command,temp_use_distributed,option, machine4);
command_2string(lav2yuv_command, lav2yuv_string);

/* Here, the command for the pipe for yuvscaler may be added */
  if (  (strcmp((*option).input_use,"as is") != 0)   ||
        (strcmp((*option).output_size,"as is") != 0) ||
        (strcmp((*option).mode_keyword,"as is") != 0) ||
        (use_bicubic == 1) ||
        ( strlen((*option).interlacecorr) > 0 &&
          strcmp((*option).interlacecorr,"not needed") != 0 ) ||
        ( strlen((*option).notblacksize) > 0 &&
          strcmp((*option).notblacksize,"as is") != 0 )          )
   {

     create_command_yuvscaler (yuvscaler_command, temp_use_distributed,
                                                      option, machine4);
     command_2string(yuvscaler_command, yuvscaler_string);
     sprintf(lav2yuv_string,"%s |%s", lav2yuv_string, yuvscaler_string);
   }   

  if ( (*option).use_yuvdenoise == 1)
    {
       create_command_yuvdenoise(yuvdenoise_command, temp_use_distributed,
                                                         option, machine4);
       command_2string(yuvdenoise_command, yuvdenoise_string);
       sprintf(lav2yuv_string,"%s |%s", lav2yuv_string, yuvdenoise_string);
    }

  if ( (strcmp(ext,"mpeg1") == 0) || (strcmp(ext,"mpeg2") == 0) || 
       (strcmp(ext,"vcd")   == 0) || (strcmp(ext,"svcd")  == 0))
    {
      create_command_mpeg2enc(mpeg2enc_command, temp_use_distributed,
                                                    option, machine4, ext);
      command_2string(mpeg2enc_command, mpeg2enc_string); 
      sprintf(lav2yuv_string,"%s |%s", lav2yuv_string, mpeg2enc_string);
    }
  else if ( strcmp(ext,"divx") == 0 )
    {
      create_command_yuv2divx(yuv2divx_command, temp_use_distributed,
                                                    option, machine4, ext);
      command_2string(yuv2divx_command, yuv2divx_string); 
      sprintf(lav2yuv_string,"%s |%s", lav2yuv_string, yuv2divx_string);
    }
  else if ( strcmp(ext,"mjpeg") == 0 )
    {
      create_command_yuv2lav(yuv2lav_command, temp_use_distributed,
                                                    option, machine4, ext);
      command_2string(yuv2lav_command, yuv2lav_string); 
      sprintf(lav2yuv_string,"%s |%s", lav2yuv_string, yuv2lav_string);
    }

  fprintf(fp,"%s\n", lav2yuv_string);

}

/* Here we create the command for the mplexing */
void create_command_mplex(char *mplex_command[256], int use_rsh,
   struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT])
{
int n;
static char temp1[16], temp2[16], temp3[16], temp4[4];
static char temp5[128], temp6[128], temp7[128];

n=0;

   mplex_command[n] = app_name(MPLEX); n++;
   if ((*option).muxformat != 0)
   {
      sprintf(temp1, "%i", (*option).muxformat);
      mplex_command[n] = "-f"; n++;
      mplex_command[n] = temp1; n++;
   }
   if ((*option).streamdatarate != 0)
   {
      sprintf(temp2, "%i", (*option).streamdatarate);
      mplex_command[n] = "-r"; n++;
      mplex_command[n] = temp2; n++;
   }
   if ((*option).decoderbuffer != 46)
   {
      sprintf(temp3, "%i", (*option).decoderbuffer);
      mplex_command[n] = "-r"; n++;
      mplex_command[n] = temp3; n++;
   }
   if ((*option).muxvbr[0] == '-' )
   {
      mplex_command[n] = (*option).muxvbr;
      n++;
   }
   if( ((*option).muxformat == 3) && ((*option).muxvbr[0] != '-') )
   {
      sprintf(temp4,"-V");
      mplex_command[n] =  temp4; n++;
   }

  if (strlen(ext) != 0)
    {
      filename_ext(ext, temp5, enc_audiofile);
      mplex_command[n] = temp5; n++;

      filename_ext(ext, temp6, enc_videofile);
      mplex_command[n] = temp6; n++;

      mplex_command[n] = "-o"; n++;
      filename_ext(ext, temp7, enc_outputfile);
      mplex_command[n] = temp7; n++;
    }
  else
    {
      mplex_command[n] = enc_audiofile; n++;
      mplex_command[n] = enc_videofile; n++;
      mplex_command[n] = "-o"; n++;
      mplex_command[n] = enc_outputfile; n++;
    }
  mplex_command[n] = NULL; 

}

/* Here we create the command for the mplexing */
void create_mplex(FILE *fp, struct encodingoptions *option,
                            struct machine *machine4, char ext[LONGOPT])
{
char *mplex_command[256];
char mplex_string[800];

  create_command_mplex(mplex_command, temp_use_distributed,
                                                    option, machine4, ext);
  command_2string(mplex_command, mplex_string);
  fprintf(fp,"%s\n", mplex_string);


}

/* Here we finally create the script */
void create_script(GtkWidget *widget, gpointer data)
{
FILE *fp;

  unlink(temp_scriptname);

  /* write the script ... */
  fp = fopen(temp_scriptname,"w");
  if (NULL == fp)
    {
       fprintf(stderr,"cant open config file %s\n",temp_scriptname);
       return;
    }

  fprintf(fp,"#!/bin/bash\n");
  fprintf(fp,"# This script was created with LVS \n");

 /* Creating the MPEG1 lines */
  if( (t_script.mpeg1 & bit_full) || (t_script.mpeg1 & bit_audio) )
    create_audio(fp, &encoding, &machine4mpeg1, "mpeg1");

  if( (t_script.mpeg1 & bit_full) || (t_script.mpeg1 & bit_video) )
    create_video(fp, &encoding, &machine4mpeg1, "mpeg1");
  
  if( (t_script.mpeg1 & bit_full) || (t_script.mpeg1 & bit_mplex) )
    create_mplex(fp, &encoding, &machine4mpeg1, "mpeg1");

  /* Creating the MPEG2 lines */
  if( (t_script.mpeg2 & bit_full) || (t_script.mpeg2 & bit_audio) )
    create_audio(fp, &encoding2, &machine4mpeg2, "mpeg2");

  if( (t_script.mpeg2 & bit_full) || (t_script.mpeg2 & bit_video) )
    create_video(fp, &encoding2, &machine4mpeg2, "mpeg2");

  if( (t_script.mpeg2 & bit_full) || (t_script.mpeg2 & bit_mplex) )
    create_mplex(fp, &encoding2, &machine4mpeg2, "mpeg2");

  /* Creating the VCD lines */
  if( (t_script.vcd & bit_full) || (t_script.vcd & bit_audio) )
    create_audio(fp, &encoding_vcd, &machine4vcd, "vcd");

  if( (t_script.vcd & bit_full) || (t_script.vcd & bit_video) )
    create_video(fp, &encoding_vcd, &machine4vcd, "vcd");

  if( (t_script.vcd & bit_full) || (t_script.vcd & bit_mplex) )
    create_mplex(fp, &encoding_vcd, &machine4vcd, "vcd");

  /* Creating the SVCD lines */
  if( (t_script.svcd & bit_full ) || (t_script.svcd & bit_audio) )
    create_audio(fp, &encoding_svcd, &machine4svcd, "svcd");

  if( (t_script.svcd & bit_full ) || (t_script.svcd & bit_video) )
    create_video(fp, &encoding_svcd, &machine4svcd, "svcd");

  if( (t_script.svcd & bit_full ) || (t_script.svcd & bit_video) )
    create_mplex(fp, &encoding_svcd, &machine4svcd, "svcd");

  /* Creating the divx lines */
  if( t_script.divx & bit_full ) 
    create_video(fp, &encoding_divx, &machine4divx, "divx");

  /* Creating the yuv2lav lines */
  if( t_script.yuv2lav & bit_video )
    create_video(fp, &encoding_yuv2lav, &machine4yuv2lav, "mjpeg");

  fclose(fp);
}

/* Here we set if the distributed part should be used */
void set_usage (GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
   temp_use_distributed = 1;
  else  
   temp_use_distributed = 0;

  if (verbose)
    printf(" Set the usage of the distributed part to : %i \n",
                                              temp_use_distributed);
}

/* Here we create the check box for the distributed selection */
void use_distributed (GtkWidget *hbox)
{
  check_usage = gtk_check_button_new_with_label
                (" Use distribued encoding settings ");
  gtk_widget_ref (check_usage);
  gtk_box_pack_start (GTK_BOX (hbox), check_usage, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (check_usage), "toggled",
                      GTK_SIGNAL_FUNC (set_usage), NULL);
  gtk_widget_show (check_usage);

}

/* Here we copy the original values into a "working set" */
void init_temp ()
{
  sprintf(temp_scriptname,"%s",script_name);
  temp_use_distributed = script_use_distributed;
 
  t_script.mpeg1  = script.mpeg1;
  t_script.mpeg2  = script.mpeg2;
  t_script.vcd    = script.vcd;
  t_script.svcd   = script.svcd;
  t_script.divx   = script.divx;
  t_script.yuv2lav= script.yuv2lav;
}

/* This is done after the OK Button was pressed */
void accept_changes(GtkWidget *widget, gpointer data)
{
  sprintf(script_name,"%s",temp_scriptname);
  script_use_distributed = temp_use_distributed;
 
  script.mpeg1  = t_script.mpeg1;
  script.mpeg2  = t_script.mpeg2;
  script.vcd    = t_script.vcd;
  script.svcd   = t_script.svcd;
  script.divx   = t_script.divx;
  script.yuv2lav= t_script.yuv2lav;
}

/* Here we create the OK and Cancel Button */
void create_ok_cancel(GtkWidget *hbox, GtkWidget *script_window)
{
GtkWidget *button;

  button = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC (accept_changes), (gpointer) "test");
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                            gtk_widget_destroy, GTK_OBJECT(script_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
          gtk_widget_destroy, GTK_OBJECT(script_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

}

/* Here we have the callback for the mpeg1 settings */
void set_mpeg1(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_script.mpeg1 = (t_script.mpeg1 | ((gint)data)); 
  else 
    t_script.mpeg1 = (t_script.mpeg1 & (~(gint)data)); 
}

/* Here we have the callback for the mpeg2 settings */
void set_mpeg2(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_script.mpeg2 = (t_script.mpeg2 | ((gint)data));
  else
    t_script.mpeg2 = (t_script.mpeg2 & (~(gint)data));
}

/* Here we have the callback for the vcd settings */
void set_vcd(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_script.vcd = (t_script.vcd | ((gint)data));
  else
    t_script.vcd = (t_script.vcd & (~(gint)data));
}
 
/* Here we have the callback for the svcd settings */
void set_svcd(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_script.svcd = (t_script.svcd | ((gint)data));
  else
    t_script.svcd = (t_script.svcd & (~(gint)data));
}
 
/* Here we have the callback for the divx settings */
void set_divx(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_script.divx = (t_script.divx | ((gint)data));
  else
    t_script.divx = (t_script.divx & (~(gint)data));
}
 
/* Here we have the callback for the yuv2lav settings */
void set_yuv2lav(GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    t_script.yuv2lav = (t_script.yuv2lav | ((gint)data));
  else
    t_script.yuv2lav = (t_script.yuv2lav & (~(gint)data));
}

/* Here we create the check boxes for the field mpeg1 */
void create_checkbox_mpeg1(GtkWidget *table)
{
int tx, ty;

  tx = 1;
  ty = 1;

  mpeg1_a = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg1_a,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg1_a), "toggled", 
                      GTK_SIGNAL_FUNC (set_mpeg1), (gpointer) 1);
  gtk_widget_show (mpeg1_a);
  tx++; 

  mpeg1_v = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg1_v,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg1_v), "toggled", 
                      GTK_SIGNAL_FUNC (set_mpeg1), (gpointer) 2);
  gtk_widget_show (mpeg1_v);
  tx++; 

  mpeg1_m = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg1_m,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg1_m), "toggled", 
                      GTK_SIGNAL_FUNC (set_mpeg1), (gpointer) 4);
  gtk_widget_show (mpeg1_m);
  tx++; 

  mpeg1_f = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg1_f,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg1_f), "toggled", 
                      GTK_SIGNAL_FUNC (set_mpeg1), (gpointer) 8);
  gtk_widget_show (mpeg1_f);
  tx++; 

}

/* Here we create the check boxes for the field mpeg2 */
void create_checkbox_mpeg2(GtkWidget *table)
{
int tx, ty;

  tx = 1;
  ty = 2;

  mpeg2_a = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg2_a,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg2_a), "toggled",
                      GTK_SIGNAL_FUNC (set_mpeg2), (gpointer) 1);
  gtk_widget_show (mpeg2_a);
  tx++;

  mpeg2_v = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg2_v,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg2_v), "toggled",
                      GTK_SIGNAL_FUNC (set_mpeg2), (gpointer) 2);
  gtk_widget_show (mpeg2_v);
  tx++;

  mpeg2_m = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg2_m,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg2_m), "toggled",
                      GTK_SIGNAL_FUNC (set_mpeg2), (gpointer) 4);
  gtk_widget_show (mpeg2_m);
  tx++;

  mpeg2_f = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),mpeg2_f,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (mpeg2_f), "toggled",
                      GTK_SIGNAL_FUNC (set_mpeg2), (gpointer) 8);
  gtk_widget_show (mpeg2_f);
  tx++;
}

/* Here we create the check boxes for the field vcd */
void create_checkbox_vcd(GtkWidget *table)
{ 
int tx, ty;
  
  tx = 1;
  ty = 3;
  
  vcd_a = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),vcd_a,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (vcd_a), "toggled",
                      GTK_SIGNAL_FUNC (set_vcd), (gpointer) 1);
  gtk_widget_show (vcd_a);
  tx++;
  
  vcd_v = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),vcd_v,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (vcd_v), "toggled",
                      GTK_SIGNAL_FUNC (set_vcd), (gpointer) 2);
  gtk_widget_show (vcd_v);
  tx++;

  vcd_m = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),vcd_m,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (vcd_m), "toggled",
                      GTK_SIGNAL_FUNC (set_vcd), (gpointer) 4);
  gtk_widget_show (vcd_m);
  tx++;
  
  vcd_f = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),vcd_f,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (vcd_f), "toggled",
                      GTK_SIGNAL_FUNC (set_vcd), (gpointer) 8);
  gtk_widget_show (vcd_f);
  tx++;
}

/* Here we create the check boxes for the field svcd */
void create_checkbox_svcd(GtkWidget *table)
{
int tx, ty;

  tx = 1;
  ty = 4;

  svcd_a = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),svcd_a,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (svcd_a), "toggled",
                      GTK_SIGNAL_FUNC (set_svcd), (gpointer) 1);
  gtk_widget_show (svcd_a);
  tx++;

  svcd_v = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),svcd_v,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (svcd_v), "toggled",
                      GTK_SIGNAL_FUNC (set_svcd), (gpointer) 2);
  gtk_widget_show (svcd_v);
  tx++;

  svcd_m = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),svcd_m,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (svcd_m), "toggled",
                      GTK_SIGNAL_FUNC (set_svcd), (gpointer) 4);
  gtk_widget_show (svcd_m);
  tx++;

  svcd_f = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),svcd_f,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (svcd_f), "toggled",
                      GTK_SIGNAL_FUNC (set_svcd), (gpointer) 8);
  gtk_widget_show (svcd_f);
  tx++;
}

/* Here we create the check boxes for the field divx */
void create_checkbox_divx(GtkWidget *table) 
{
int tx, ty;
  
  tx = 4;
  ty = 5;

  divx_f = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),divx_f,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (divx_f), "toggled",
                      GTK_SIGNAL_FUNC (set_divx), (gpointer) 8);
  gtk_widget_show (divx_f);
  tx++;
}

/* Here we create the check boxes for the field yuv2lav */
void create_checkbox_yuv2lav(GtkWidget *table)
{
int tx, ty;

  tx = 2;
  ty = 6;

  yuv2lav_v = gtk_check_button_new ();
  gtk_table_attach_defaults (GTK_TABLE(table),yuv2lav_v,tx,tx+1,ty,ty+1);
  gtk_signal_connect (GTK_OBJECT (yuv2lav_v), "toggled",
                      GTK_SIGNAL_FUNC (set_yuv2lav), (gpointer) 2);
  gtk_widget_show (yuv2lav_v);
  tx++;
}

/* Here we create the selection check buttons for the script */
void create_scriptfield(GtkWidget *hbox)
{
GtkWidget *table, *label;
int tx, ty; /* table size x, y */

tx = 5;
ty = 7;

  table = gtk_table_new (tx, ty, FALSE);

  tx = 1;
  ty = 0;

  label = gtk_label_new (" Audio ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  label = gtk_label_new (" Video ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  label = gtk_label_new (" Mplex ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  label = gtk_label_new (" Full ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  tx = 0;
  ty = 1;

  label = gtk_label_new (" MPEG1 ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  label = gtk_label_new (" MPEG2 ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  label = gtk_label_new (" VCD ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  label = gtk_label_new (" SVCD ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  label = gtk_label_new (" DivX ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  label = gtk_label_new (" MJPEG ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  create_checkbox_mpeg1   (table);
  create_checkbox_mpeg2   (table);
  create_checkbox_vcd     (table);
  create_checkbox_svcd    (table);
  create_checkbox_divx    (table);
  create_checkbox_yuv2lav (table);

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);
}

/* Here we create the script generation Button */
void create_script_button(GtkWidget *hbox) 
{
GtkWidget *button;

  button = gtk_button_new_with_label("Create Script");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC (create_script), NULL );
  gtk_widget_set_usize (button, 200, -2);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, FALSE, 5);
  gtk_widget_show(button);


}

/* Here we create the window, menu basic layout ... */
void open_scriptgen_window(GtkWidget *widget, gpointer data)
{
GtkWidget *script_window, *vbox, *hbox, *separator, *label;

  init_temp();

  script_window = gtk_window_new (GTK_WINDOW_DIALOG);
  vbox = gtk_vbox_new (FALSE, 2);

  label = gtk_label_new (" Script Generation ");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 5);  
  gtk_widget_show (label);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);  
  gtk_widget_show (separator);

  hbox = gtk_hbox_new (FALSE, 10);
  create_scriptname(hbox);                /* creating the script name dialog */
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox); 

  hbox = gtk_hbox_new (TRUE, 10);
  use_distributed(hbox);  /* creating the check box for distributed encoding */
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox); 

  label = gtk_label_new (" Create Script for : ");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 5);  
  gtk_widget_show (label);

  hbox = gtk_hbox_new (TRUE, 10);
  create_scriptfield(hbox); /* Creating the selection window */
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
  gtk_widget_show (hbox);

  hbox = gtk_hbox_new (FALSE, 10);
  create_script_button (hbox); /* Creating the "Create Script" Button */
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox);

  hbox = gtk_hbox_new (TRUE, 10);
  create_ok_cancel(hbox, script_window);/*Creation of the OK an cancel Button*/
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox); 

  gtk_container_add (GTK_CONTAINER (script_window), vbox);
  gtk_widget_show (vbox);
  gtk_widget_show (script_window);

  set_up_defaults();

}

