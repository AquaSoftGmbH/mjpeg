/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * lavencode.c done by Bernhard Praschinger
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <signal.h>
#include <gdk/gdkx.h>
#include "studio.h"
#include "pipes.h"
#include "gtkfunctions.h"

/* Constanst defined here */
/* Limits of the Encoding selections */
#define Encselect_x 2
#define Encselect_y 7

/* Some variables defined here */
GtkWidget *input_entry, *output_entry, *sound_entry, *video_entry;
GtkWidget *execute_status;
GtkWidget *remove_files_after_completion;

/* For progress-meter */
GtkWidget *progress_label;
GtkWidget *progress_button_label;
GtkWidget *progress_status_label;
GtkWidget *progress_bar;
int progress_encoding = 0;
int num_frames = -1;
char standard;
GtkObject *progress_adj;
int go_all_the_way = 0; /* if 1, it will do audio/video/mplex, else only 1 of them */
int use_yuvscaler_pipe; /* tells us whether to use yuvscaler or not */
int use_yuvplay_pipe = 0; /* see preview window while video is being encoded or not */
GtkWidget *progress_window;
GtkWidget *tv_preview;
int error = 0;

/* lists with  encodingoptions */
GList *samples_old = NULL;
// GList *outputformat = NULL;
GList *yuvscalermode = NULL;
/*-------------------------------------------------------------*/

void lavencode_callback(int number, char *input);
void continue_encoding(void);
int number_of_frames(char *editlist);
void status_progress_window(void);
void stop_encoding_process(GtkWidget *widget, gpointer data);
void show_executing(char *command);
void command2string(char **command, char *string);
void callback_pipes(gpointer data, gint source,GdkInputCondition condition);
void audio_convert(void);
void video_convert(void);
void mplex_convert(void);
void do_vcd(GtkWidget *widget, gpointer data);
void do_svcd(GtkWidget *widget, gpointer data);
void do_defaults(GtkWidget *widget, gpointer data);
void input_callback( GtkWidget *widget, GtkWidget *input_select );
void output_callback( GtkWidget *widget, GtkWidget *output_select );
void sound_callback( GtkWidget *widget, GtkWidget *sound_select );
void file_ok_sound ( GtkWidget *w, GtkFileSelection *fs );
void create_file_sound(GtkWidget *widget, gpointer data);
void file_ok_input( GtkWidget *w, GtkFileSelection *fs );
void create_file_input(GtkWidget *widget, gpointer data);
void file_ok_output( GtkWidget *w, GtkFileSelection *fs );
void create_in_out (GtkWidget *hbox1, GtkWidget *vbox);
void create_vcd_svcd (GtkWidget *hbox1, GtkWidget *vbox);
// void set_audiobitrate (GtkWidget *widget, gpointer data);
void set_samplebitrate (GtkWidget *widget, gpointer data);
// void force_options (GtkWidget *widget, gpointer data);
void create_sound_encoding_old (GtkWidget *table);
// void set_videobit (GtkWidget *widget, gpointer data);
void set_searchrad (GtkWidget *widget, gpointer data);
void mpeg_option (GtkWidget *widget, gpointer data);
void create_video_layout (GtkWidget *table);
void create_file_output(GtkWidget *widget, gpointer data);
// void set_noisefilter (GtkWidget *widget, gpointer data);
// void set_drop_samples (GtkWidget *widget, gpointer data);
// void set_outputformat (GtkWidget *widget, gpointer data);
void create_yuvscaler_options (GtkWidget *hbox1, GtkWidget *vbox);
void video_callback ( GtkWidget *widget, GtkWidget *video_entry );
void file_ok_video ( GtkWidget *w, GtkFileSelection *fs );
void create_file_video(GtkWidget *widget, gpointer data);
void create_fileselect_video (GtkWidget *hbox1, GtkWidget *vbox);
void create_video (GtkWidget *table);
// void set_mplex_muxfmt (GtkWidget *widget, gpointer data);
void create_mplex_options (GtkWidget *table);
void encode_all (GtkWidget *widget, gpointer data);
void create_buttons1 (GtkWidget *hbox1);
void create_buttons2 (GtkWidget *hbox1);
void create_mplex (GtkWidget *table);
void create_status (GtkWidget *hbox1, GtkWidget *vbox);
// void create_yuvscaler_layout (GtkWidget *table);
// void set_activewindow (GtkWidget *widget, gpointer data);
// void set_scalerstrings (GtkWidget *widget, gpointer data);
void create_option_button(GSList *task_group, GtkWidget *table, 
       char task[LONGOPT], int encx, int ency);
void create_task_layout(GtkWidget *table);
void create_temp_files (GtkWidget *hbox1, GtkWidget *vbox);

/*************************** Programm starts here **************************/

int number_of_frames(char *editlist)
{
   FILE *fd;
   int n,num,i,j;
   char buffer[256];

   fd = fopen(editlist, "r");
   if (fd == NULL)
   {
      printf("Error opening %s\n",editlist);
      return -1;
   }

   if (NULL != fgets(buffer,255,fd))
   {
      buffer[strlen(buffer)-1] = '\0'; /* get rid of \n */
      if (strcmp(buffer, "LAV Edit List") != 0)
      {
         /* not an editlist! So let's make our own :-) */
         char command_temp[256], file_temp[100];

         fclose(fd);
         sprintf(file_temp, "%s/.studio/.temp.eli", getenv("HOME"));
         /* this is for certainty to prevent circles */
         if (strcmp(editlist, file_temp)==0) return -1;
         sprintf(command_temp, "%s -S %s -T -1 %s%s",
            LAV2YUV_LOCATION, file_temp, editlist,
            verbose?"":" >> /dev/null 2>&1");
         system(command_temp);
         num = number_of_frames(file_temp);
         unlink(file_temp);
         return(num);
      }
   }

   fgets(buffer,255,fd);
   for(i=0;i<strlen(buffer);i++)
   {
      if(buffer[i]=='\n')
      {
         buffer[i] = '\0';
         break;
      }
   }
   if(strcmp(buffer, "PAL")==0)
      standard = 'p';
   else
      standard = 'n';

   fgets(buffer,255,fd); /* number of files */
   sscanf(buffer, "%d", &num);
   for (n=0;n<num;n++)
   {
      fgets(buffer,255,fd); /* skip movie-lines */
   }

   num = 0;
   /* Now, it gets interesting */
   while (NULL != fgets(buffer,255,fd))
   {
      sscanf(buffer, "%d %d %d", &n, &i, &j);
      num += (j-i+1); /* counts frames */
   }

   fclose(fd);

   return num; /* number of frames */
}

void lavencode_callback(int number, char *input) 
{
  if (progress_status_label)
  {
    int n1, n2;
    char c;
    if (number == MPEG2ENC)
    {
      if ( (encoding_syntax_style == 140) && 
           (sscanf(input, "   INFO: Frame %d %c %d", &n1, &c, &n2)==3) )
      {
       gtk_label_set_text(GTK_LABEL(progress_status_label), input+9);
       if (num_frames>0) gtk_adjustment_set_value(GTK_ADJUSTMENT(progress_adj), n1);
      }
      
      if ( (encoding_syntax_style == 150) && 
           (sscanf(input, "    INFO: Frame start %d %c %d", &n1, &c, &n2)==3) )
      {
       gtk_label_set_text(GTK_LABEL(progress_status_label), input+9);
       if (num_frames>0) gtk_adjustment_set_value(GTK_ADJUSTMENT(progress_adj), n1);
      }
    }
    else if (number == MP2ENC && sscanf(input, "--DEBUG: %d seconds done", &n1)==1)
    {
       gtk_label_set_text(GTK_LABEL(progress_status_label), input+9);
       if (num_frames>0) gtk_adjustment_set_value(GTK_ADJUSTMENT(progress_adj), standard=='p'?25*n1:30*n1);
    }
  }

  if (strncmp(input, "**ERROR:", 8) == 0)
  {
     char temp[256];

     /* Error handling */
     if (!error) stop_encoding_process(NULL, (gpointer)progress_window);
     sprintf(temp, "%s returned an error:", app_name(number));
     gtk_show_text_window(STUDIO_ERROR, temp, input+9);
     error++;
  }
}

void continue_encoding() 
{
   if (go_all_the_way) switch (progress_encoding ) {
      case 1:
         video_convert();
         progress_encoding = 2;
         break;
      case 2:
         mplex_convert();
         progress_encoding = 3;
         break;
      case 3:
         progress_encoding = 0;
         if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label), "Finished!");
         if (progress_button_label) gtk_label_set_text(GTK_LABEL(progress_button_label), " Close ");
         if (progress_bar) gtk_progress_bar_update(GTK_PROGRESS_BAR(progress_bar), 1.0);
         show_executing("");

         if (GTK_TOGGLE_BUTTON(remove_files_after_completion)->active) {
            /* remove temp files */
            unlink(enc_audiofile);
            unlink(enc_videofile);
         }

         break;
   }
   else {
      progress_encoding = 0;
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label), "Finished!");
      if (progress_button_label) gtk_label_set_text(GTK_LABEL(progress_button_label), " Close ");
      if (progress_bar) gtk_progress_bar_update(GTK_PROGRESS_BAR(progress_bar),
1.0);

      if (GTK_TOGGLE_BUTTON(remove_files_after_completion)->active) {
         /* remove temp files */
         unlink(enc_audiofile);
         unlink(enc_videofile);
      }
   }
}


void stop_encoding_process(GtkWidget *widget, gpointer data) {
   GtkWidget *window = (GtkWidget*)data;

   go_all_the_way = 0;

   if (progress_encoding) {
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label), "Cancelled");
      show_executing("Cancelled");
   }

   switch(progress_encoding) {
   case 1:
      close_pipe(LAV2WAV);
      close_pipe(MP2ENC);
      break;
   case 2:
      close_pipe(LAV2YUV);
      if (use_yuvscaler_pipe) close_pipe(YUVSCALER);
      close_pipe(MPEG2ENC);
      break;
   case 3:
      close_pipe(MPLEX);
      break;
   }

   progress_encoding = 0;

   progress_label = NULL;
   progress_button_label = NULL;
   progress_status_label = NULL;
   progress_bar = NULL;
   progress_adj = NULL;
   progress_encoding = 0;
   gtk_widget_destroy(window);

   if (GTK_TOGGLE_BUTTON(remove_files_after_completion)->active) {
      /* remove temp files */
      unlink(enc_audiofile);
      unlink(enc_videofile);
   }
}

void status_progress_window() {
   GtkWidget *vbox, *hbox, *label, *button;

   num_frames = number_of_frames(enc_inputfile);

   progress_window = gtk_window_new(GTK_WINDOW_DIALOG);
   vbox = gtk_vbox_new(FALSE, 5);

   gtk_window_set_title (GTK_WINDOW(progress_window), "Linux Video Studio - MPEG Encoding status");
   gtk_container_set_border_width (GTK_CONTAINER (progress_window), 20);

   if (use_yuvplay_pipe)
   {
      hbox = gtk_hbox_new(TRUE, 0);

      tv_preview = gtk_event_box_new();
      gtk_box_pack_start (GTK_BOX (hbox), tv_preview, TRUE, FALSE, 0);
      gtk_widget_set_usize(GTK_WIDGET(tv_preview), 240, 180); /* TODO: make sizes configurable */
      gtk_widget_show(tv_preview);
      set_background_color(tv_preview,0,0,0);

      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      gtk_widget_show(hbox);
   }

   hbox = gtk_hbox_new(FALSE, 0);
   label = gtk_label_new ("Status: ");
   gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE,FALSE, 0);
   gtk_widget_show (label);
   progress_label = gtk_label_new ("Starting up...");
   gtk_misc_set_alignment(GTK_MISC(progress_label), 0.0, GTK_MISC(progress_label)->yalign);
   gtk_box_pack_start (GTK_BOX (hbox), progress_label, TRUE, TRUE, 0);
   gtk_widget_show (progress_label);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   gtk_widget_show(hbox);

   progress_status_label = gtk_label_new(" ");
   gtk_box_pack_start (GTK_BOX (vbox), progress_status_label, FALSE,FALSE, 10);
   gtk_widget_show (progress_status_label);

   progress_bar = gtk_progress_bar_new();
   gtk_box_pack_start (GTK_BOX (vbox), progress_bar, FALSE, FALSE, 0);
   gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(progress_bar), GTK_PROGRESS_LEFT_TO_RIGHT);
   gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(progress_bar), GTK_PROGRESS_CONTINUOUS);
   gtk_progress_set_show_text(GTK_PROGRESS(progress_bar), 1);
   gtk_progress_set_format_string(GTK_PROGRESS(progress_bar), "%p\%");
   if (num_frames > 0)
   {
      progress_adj = gtk_adjustment_new(0,0,num_frames,1,1,0);
      gtk_progress_set_adjustment(GTK_PROGRESS(progress_bar), GTK_ADJUSTMENT(progress_adj));
   }
   gtk_widget_show(progress_bar);

   button = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      (GtkSignalFunc)stop_encoding_process, (gpointer)progress_window);
   progress_button_label = gtk_label_new(" Cancel ");
   gtk_container_add (GTK_CONTAINER (button), progress_button_label);
   gtk_widget_show(progress_button_label);
   gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   gtk_container_add (GTK_CONTAINER (progress_window), vbox);
   gtk_widget_show(vbox);

   gtk_grab_add(progress_window);
   gtk_widget_show(progress_window);
}

/* print text */
void show_executing(char command[500])
{
  gtk_label_set_text(GTK_LABEL(execute_status),command);
  gtk_widget_show (execute_status);
}

void command2string(char **command, char *string)
{
   int i;

   string[0] = '\0';
   for (i=0;;i++)
   {
      if(command[i]!=NULL)
         sprintf(string,"%s %s",string, command[i]); 
      else
         return;
   }
}

/* here the audio command is created */
void audio_convert()
{
   int n;
   char *mp2enc_command[256];
   char *lav2wav_command[256];
   char command[500];
   char command_temp[256];
   static char temp1[16], temp2[16];

   n=0;
   error = 0;
   progress_encoding = 1;

   if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
      "Encoding audio: lav2wav | mp2enc");

   mp2enc_command[n] = MP2ENC_LOCATION; n++;
   mp2enc_command[n] = "-v 2"; n++;
   if (encoding.forcevcd[0] == '-')
     {
       mp2enc_command[n] = encoding.forcevcd;
       n++;
     }
   else                   /* If VCD is set no other Options are needed */
     {
        mp2enc_command[n] = "-b"; n++;
        sprintf(temp1,"%i", encoding.audiobitrate);
        mp2enc_command[n] = temp1; n++;
        mp2enc_command[n] = "-r"; n++;
        sprintf(temp2, "%i00", encoding.outputbitrate);
        mp2enc_command[n] = temp2; n++;
       if (encoding.forcemono[0] == '-')
         {
           mp2enc_command[n] = encoding.forcemono;
           n++;
         }
       if (encoding.forcestereo[0] == '-')
         {
           mp2enc_command[n] = encoding.forcestereo;
           n++;
         }
     }
   mp2enc_command[n] = "-o"; n++;         /* Setting output file name */
   mp2enc_command[n] = enc_audiofile; n++;
   mp2enc_command[n] = NULL;
   
   start_pipe_command(mp2enc_command, MP2ENC); /* mp2enc */

   n = 0;
   lav2wav_command[n] = LAV2WAV_LOCATION; n++;
   lav2wav_command[n] = enc_inputfile; n++;
   lav2wav_command[n] = NULL;
 
   start_pipe_command(lav2wav_command, LAV2WAV); /* lav2wav */

   command2string(lav2wav_command, command_temp);
   sprintf(command, "%s |", command_temp);
   command2string(mp2enc_command, command_temp);
   sprintf(command, "%s %s", command, command_temp);
   if (verbose)
      printf("Executing: %s\n", command);
   show_executing(command);
}

/* video encoding */
void video_convert()
{
   int n;
   char *mpeg2enc_command[256];
   char *lav2yuv_command[256];
   char *yuvscaler_command[256];
   char *yuvplay_command[256];
   char command[600];
   char command_temp[256];
   static char temp1[16],temp2[16],temp3[16],temp4[16],temp5[16],temp6[16], temp7[4];

   error = 0;
   progress_encoding = 2;
   if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
      "Encoding video");

   n = 0;
   mpeg2enc_command[n] = MPEG2ENC_LOCATION; n++;
   mpeg2enc_command[n] = "-v1"; n++;
   mpeg2enc_command[n] = "-b"; n++;
   sprintf(temp4, "%i", encoding.bitrate);
   mpeg2enc_command[n] =  temp4; n++;
   if(encoding.searchradius != 16) {
      sprintf(temp6, "%i", encoding.searchradius);
      mpeg2enc_command[n] =  "-r"; n++;
      mpeg2enc_command[n] =  temp6; n++;
   }

/* And here the support fpr the different versions vo mjpeg tools */
   if (encoding_syntax_style == 140)
   {
     if (encoding.muxformat >= 3)
       {
         mpeg2enc_command[n] = "-m"; n++;
         mpeg2enc_command[n] = "2"; n++; 
       }
     if ((encoding.muxformat != 0) && (encoding.muxformat != 3))
       {
         mpeg2enc_command[n] = "-s"; n++;
       }
   }
   if (encoding_syntax_style == 150)
   {
     if(encoding.muxformat != 0) 
       {
         sprintf(temp5, "%i",encoding.muxformat);
         mpeg2enc_command[n] =  "-f"; n++;
         mpeg2enc_command[n] =  temp5; n++;
       }
     
   }

/* And here again some common stuff */
   mpeg2enc_command[n] = "-o"; n++;
   mpeg2enc_command[n] = enc_videofile; n++;
   mpeg2enc_command[n] = NULL;

   start_pipe_command(mpeg2enc_command, MPEG2ENC); /* mpeg2enc */

   /* let's start yuvplay to show video while it's being encoded */
   if (use_yuvplay_pipe)
   {
      char SDL_windowhack[32];

      sprintf(SDL_windowhack,"%ld",GDK_WINDOW_XWINDOW(tv_preview->window));
      setenv("SDL_WINDOWID", SDL_windowhack, 1);

      n = 0;
      yuvplay_command[n] = "yuvplay"; n++;
      yuvplay_command[n] = "-s"; n++;
      yuvplay_command[n] = "240x180"; n++;
      yuvplay_command[n] = NULL;
      start_pipe_command(yuvplay_command, YUVPLAY); /* yuvplay */
   }

   /* Here, the command for the pipe for yuvscaler may be added */
/* Old Version now checking for "as is"          */
//   if ((strlen(encoding.input_use) > 0) ||
//       (strlen(encoding.output_size) > 0) ||
//       (strlen(encoding.mode_keyword) > 0) )
     if ((strcmp(encoding.input_use,"as is") != 0) ||
         (strcmp(encoding.output_size,"as is") != 0) ||
         (strcmp(encoding.mode_keyword,"as is") != 0) )
   {
      n = 0;
      yuvscaler_command[n] = YUVSCALER_LOCATION; n++;
      if (strlen(encoding.input_use) > 0 &&
          strcmp(encoding.input_use,"as is") )
      {
         yuvscaler_command[n] = "-I"; n++;
         yuvscaler_command[n] = encoding.input_use; n++;
      }
      if (strlen(encoding.ininterlace_type) > 0)
      {
         yuvscaler_command[n] = "-I"; n++;
         yuvscaler_command[n] = encoding.ininterlace_type; n++;
      }
      if (strlen(encoding.mode_keyword) > 0 &&
          strcmp(encoding.mode_keyword,"as is") )
      {
         yuvscaler_command[n] = "-M"; n++;
         yuvscaler_command[n] = encoding.mode_keyword; n++;
      }
      if (strlen(encoding.output_size) > 0 &&
          strcmp(encoding.output_size,"as is") )
      {
         yuvscaler_command[n] = "-O"; n++;
         yuvscaler_command[n] = encoding.output_size; n++;
      }
      if (encoding.addoutputnorm == 1)
      {
         sprintf(temp7, "-n%c", standard);
         yuvscaler_command[n] = temp7; n++;
      }
      yuvscaler_command[n] = NULL;

      start_pipe_command(yuvscaler_command, YUVSCALER); /* yuvscaler */

      /* set variable in pipes.h to tell to use yuvscaler */
      use_yuvscaler_pipe = 1;
   }
   else
   {
      /* set variable in pipes.h to tell not to use yuvscaler */
      use_yuvscaler_pipe = 0;
   }

   n = 0;
   lav2yuv_command[n] = LAV2YUV_LOCATION; n++;
   if (strlen(encoding.notblacksize) > 0 && 
       strcmp(encoding.notblacksize,"as is") != 0) 
     {
      lav2yuv_command[n] = "-a"; n++;
      lav2yuv_command[n] = encoding.notblacksize; n++;
     }
   if (encoding.outputformat > 0) 
     {
      sprintf(temp1, "%i", encoding.outputformat);
      lav2yuv_command[n] = "-s"; n++;
      lav2yuv_command[n] = temp1; n++;
     }
   if (encoding.droplsb > 0) 
     {
      sprintf(temp2, "%i", encoding.droplsb);
      lav2yuv_command[n] = "-d"; n++;
      lav2yuv_command[n] = temp2; n++;
     }
   if (encoding.noisefilter > 0) 
     {
      sprintf(temp3, "%i", encoding.noisefilter);
      lav2yuv_command[n] = "-n"; n++;
      lav2yuv_command[n] = temp3; n++;
     }
   lav2yuv_command[n] = enc_inputfile; n++;
   lav2yuv_command[n] = NULL;

   start_pipe_command(lav2yuv_command, LAV2YUV); /* lav2yuv */

   command2string(lav2yuv_command, command_temp);
   sprintf(command, "%s |", command_temp);
   if (use_yuvscaler_pipe)
   {
      command2string(yuvscaler_command, command_temp);
      sprintf(command, "%s %s |", command, command_temp);
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
         "Encoding video: lav2yuv | yuvscaler | mpeg2enc");
   }
   else
      if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
         "Encoding video: lav2yuv | mpeg2enc");

   command2string(mpeg2enc_command, command_temp);
   sprintf(command, "%s %s", command, command_temp);
   if (verbose)
      printf("Executing : %s\n", command);
   show_executing(command);
}

/* mplexing the encodet streams */
void mplex_convert()
{
   char *mplex_command[256];
   char command[256];
   static char temp1[16];
   int n;

   error = 0;
   progress_encoding = 3;

   if (progress_label) gtk_label_set_text(GTK_LABEL(progress_label),
      "Multiplexing: mplex");

   n = 0;
   mplex_command[n] = MPLEX_LOCATION; n++;
   if (encoding.muxformat != 0) {
      sprintf(temp1, "%i", encoding.muxformat);
      mplex_command[n] = "-f"; n++;
      mplex_command[n] = temp1; n++;
   }
   mplex_command[n] = enc_audiofile; n++;
   mplex_command[n] = enc_videofile; n++;
   mplex_command[n] = "-o"; n++;
   mplex_command[n] = enc_outputfile; n++;
   mplex_command[n] = NULL;

   start_pipe_command(mplex_command, MPLEX); /* mplex */

   command2string(mplex_command, command);
   if (verbose)
     printf("\nExecuting : %s\n", command);
   show_executing(command);
}

/* set the VCD options */ 
void do_vcd(GtkWidget *widget, gpointer data)
{
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_audiobitrate),"224");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_samplebitrate),"44100");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_outputformat), "as is");
/* *** Removed because if noting to scale yuvscaler prevents encoding *** */
/*  gtk_entry_set_text(GTK_ENTRY(combo_entry_scaleroutput), "VCD");  */
//  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_force_vcd),TRUE );
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_videobitrate),"1152");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_searchradius),"16");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_muxfmt),"standard VCD");
}

/* set the SVCD options */
void do_svcd(GtkWidget *widget, gpointer data)
{
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_audiobitrate),"224");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_samplebitrate),"44100");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_outputformat), "as is");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_scaleroutput), "SVCD");
//  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_force_vcd),TRUE );
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_videobitrate),"2500");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_searchradius),"16");
//  gtk_entry_set_text(GTK_ENTRY(combo_entry_muxfmt),"SVCD");
}

/* set the default options */
void do_defaults(GtkWidget *widget, gpointer data)
{
  if (strcmp((char*)data, "save")==0)
    save_config_encode(); 

  if (strcmp((char*)data, "load")==0)
    {
      load_config_encode(); 

      gtk_entry_set_text(GTK_ENTRY(input_entry), enc_inputfile);
      gtk_entry_set_text(GTK_ENTRY(output_entry),enc_outputfile);
      gtk_entry_set_text(GTK_ENTRY(sound_entry), enc_audiofile);
      gtk_entry_set_text(GTK_ENTRY(video_entry), enc_videofile);
    }
 
}

/* Changes to the input field are here processed */
void input_callback( GtkWidget *widget, GtkWidget *input_select )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(input_select));
  strcpy(enc_inputfile, file);

  if (verbose)
    printf("\nEntry contents: %s\n", enc_inputfile);
}

/* Changes to the output field are here processed */
void output_callback( GtkWidget *widget, GtkWidget *output_select )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(output_select));
  strcpy(enc_outputfile, file);

  if (verbose)
    printf("\nEntry contents: %s\n", enc_outputfile);
}

/* Changes to the audio file field are here processed */
void sound_callback( GtkWidget *widget, GtkWidget *sound_select )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(sound_select));
  strcpy(enc_audiofile, file);

  if (verbose) 
    printf("\nEntry contents: %s\n", enc_audiofile);
}

/* Set the selected audio filename */
void file_ok_sound ( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(sound_entry),
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Create a new file selection widget for the output file*/
void create_file_sound(GtkWidget *widget, gpointer data)
{
GtkWidget *filew;

  filew = gtk_file_selection_new ("Linux Video Studio - Audio File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_sound, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Set the selected input filename */
void file_ok_input( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(input_entry),
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Create a new file selection widget for the input file*/
void create_file_input(GtkWidget *widget, gpointer data)
{ 
GtkWidget *filew;
  
  filew = gtk_file_selection_new ("Linux Video Studio - Input File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_input, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Set the selected output filename */
void file_ok_output( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(output_entry), 
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Create a new file selection widget for the output file*/
void create_file_output(GtkWidget *widget, gpointer data)
{
GtkWidget *filew;

  filew = gtk_file_selection_new ("Linux Video Studio - Output File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_output, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Create Layout for the input and output fileselection */
void create_in_out (GtkWidget *hbox1, GtkWidget *vbox)
{
GtkWidget *label1, *label2;   
GtkWidget *input_select, *output_select;

  label1 = gtk_label_new ("  Input file : ");
  gtk_widget_set_usize (label1, 70, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0); 
  gtk_widget_show (label1);

  input_entry = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(input_entry), enc_inputfile); 
  gtk_signal_connect(GTK_OBJECT(input_entry), "changed",
                     GTK_SIGNAL_FUNC(input_callback), input_entry);
  gtk_widget_set_usize (input_entry, 200, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), input_entry, FALSE, TRUE, 0);
  gtk_widget_show (input_entry);

  input_select = gtk_button_new_with_label ("Select");
  gtk_signal_connect(GTK_OBJECT(input_select), "clicked", 
                     GTK_SIGNAL_FUNC(create_file_input), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), input_select, FALSE, FALSE, 0);
  gtk_widget_show (input_select);

  label2 = gtk_label_new ("  Output file : ");
  gtk_widget_set_usize (label1, 70, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0); 
  gtk_widget_show (label2);
     
  output_entry = gtk_entry_new (); 
  gtk_entry_set_text(GTK_ENTRY(output_entry), enc_outputfile); 
  gtk_signal_connect(GTK_OBJECT(output_entry), "changed",
                     GTK_SIGNAL_FUNC(output_callback), output_entry);
  gtk_widget_set_usize (output_entry, 200, -2);
  gtk_widget_show (output_entry);
  gtk_box_pack_start (GTK_BOX (hbox1), output_entry, FALSE, TRUE, 0);

  output_select = gtk_button_new_with_label ("Select");
  gtk_signal_connect(GTK_OBJECT(output_select), "clicked", 
                     GTK_SIGNAL_FUNC(create_file_output), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), output_select, FALSE, FALSE, 0);
  gtk_widget_show (output_select);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0); 
  gtk_widget_show (hbox1);

  create_temp_files (hbox1, vbox);

}

/* Set the mpeg option */
void mpeg_option (GtkWidget *widget, gpointer data)
{
char temp[3];
int i;

for (i = 0; i < 3; i++)
  temp[i]='\0';

  if (strcmp((char*)data,"-m 1") ==0 )
    encoding.mpeglevel = 1;
  else if (strcmp((char*)data,"-m 2") ==0 ) 
    encoding.mpeglevel = 2;

  sprintf(temp,"%c",enc_videofile[strlen(enc_videofile)-2]);

  if ( (strcmp(temp , "1") == 0) || (strcmp(temp , "2") == 0) )
    {
      sprintf(temp,"%i",encoding.mpeglevel);
      enc_videofile[strlen(enc_videofile)-2] = temp[0];
      gtk_entry_set_text(GTK_ENTRY(video_entry), enc_videofile);
    }

  if (verbose)
    printf("\n MPEG Level set to: %i \n",encoding.mpeglevel);
}

/* Ups, no comment, how could this happen, change me */
void video_callback ( GtkWidget *widget, GtkWidget *video_entry )
{
 gchar *file;

  file = gtk_entry_get_text(GTK_ENTRY(video_entry));
  strcpy(enc_videofile, file);

  if (verbose)
    printf("\nEntry contents: %s\n", enc_videofile);
}

/* Set the selected video filename */
void file_ok_video ( GtkWidget *w, GtkFileSelection *fs )
{
  gtk_entry_set_text(GTK_ENTRY(video_entry),
                gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

/* Select the video filename */
void create_file_video(GtkWidget *widget, gpointer data)
{
GtkWidget *filew;

  filew = gtk_file_selection_new ("Linux Video Studio - Video File Selection");
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
                         "clicked", (GtkSignalFunc) file_ok_video, filew );
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),           "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
  gtk_widget_show(filew);
}

/* Create Layout for the video conversation */
void create_video (GtkWidget *table)
{
  //create_video_options_layout(table);
  //create_yuvscaler_layout(table);
  create_video_layout(table);
}

/* creating the layout for temp files */
void create_temp_files (GtkWidget *hbox1, GtkWidget *vbox)
{
GtkWidget *label1, *video_select, *sound_select;

  hbox1 = gtk_hbox_new (FALSE, 0);

  /* sound temp file */
  label1 = gtk_label_new ("  Audio file: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_widget_set_usize (label1, 70, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0); 
  gtk_widget_show (label1);

  sound_entry = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(sound_entry), enc_audiofile);
  gtk_signal_connect(GTK_OBJECT(sound_entry), "changed",
                     GTK_SIGNAL_FUNC(sound_callback), sound_entry);
  gtk_widget_set_usize (sound_entry, 200, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), sound_entry, FALSE, FALSE, 0);
  gtk_widget_show (sound_entry);

  sound_select = gtk_button_new_with_label ("Select");
  gtk_widget_show (sound_select);
  gtk_signal_connect(GTK_OBJECT(sound_select), "clicked",
                     GTK_SIGNAL_FUNC(create_file_sound), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), sound_select, FALSE, FALSE, 0);
  gtk_widget_show (sound_select);

  label1 = gtk_label_new ("  Video file : ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_widget_set_usize (label1, 75, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0); 
  gtk_widget_show (label1);

  video_entry = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(video_entry), enc_videofile);
  gtk_signal_connect(GTK_OBJECT(video_entry), "changed",
                     GTK_SIGNAL_FUNC(video_callback), video_entry);
  gtk_widget_set_usize (video_entry, 200, -2);
  gtk_box_pack_start (GTK_BOX (hbox1), video_entry, FALSE, FALSE, 0);
  gtk_widget_show (video_entry);

  video_select = gtk_button_new_with_label ("Select");
  gtk_widget_show (video_select);
  gtk_signal_connect(GTK_OBJECT(video_select), "clicked",
                     GTK_SIGNAL_FUNC(create_file_video), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), video_select, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  hbox1 = gtk_hbox_new (FALSE, 0);

  remove_files_after_completion = gtk_check_button_new_with_label
                                  ("Delete temp files after encoding");
  gtk_widget_ref (remove_files_after_completion);
  gtk_box_pack_start(GTK_BOX(hbox1), remove_files_after_completion, FALSE, FALSE,0);
  gtk_widget_show (remove_files_after_completion);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);
}

/* here all steps of the convert are started */
void encode_all (GtkWidget *widget, gpointer data)
{
  status_progress_window();
  go_all_the_way = 1;
  audio_convert();   /* using go_all_the_way only one call is needed */
}

/* the start of the encoding and mplex */
void create_buttons1 (GtkWidget *hbox1)
{
  GtkWidget *mplex_only, *do_all;
  GtkWidget *create_sound, *do_video;

  do_all = gtk_button_new_with_label (" Full create ");
  gtk_signal_connect (GTK_OBJECT (do_all), "clicked",
                      GTK_SIGNAL_FUNC (encode_all), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), do_all, TRUE, TRUE, 0);
  gtk_widget_show(do_all);

  create_sound = gtk_button_new_with_label ("Audio Only");
  gtk_signal_connect (GTK_OBJECT (create_sound), "clicked",
                      GTK_SIGNAL_FUNC (status_progress_window), NULL);
  gtk_signal_connect (GTK_OBJECT (create_sound), "clicked",
                      GTK_SIGNAL_FUNC (audio_convert), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), create_sound, TRUE, TRUE, 0);
  gtk_widget_show(create_sound);

  do_video = gtk_button_new_with_label ("Video Only");
  gtk_signal_connect (GTK_OBJECT (do_video), "clicked",
                      GTK_SIGNAL_FUNC (status_progress_window), NULL);
  gtk_signal_connect (GTK_OBJECT (do_video), "clicked",
                      GTK_SIGNAL_FUNC (video_convert), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), do_video, TRUE, TRUE, 0);
  gtk_widget_show(do_video);

  mplex_only = gtk_button_new_with_label ("Mplex Only");
  gtk_signal_connect (GTK_OBJECT (mplex_only), "clicked",
                      GTK_SIGNAL_FUNC (status_progress_window), NULL);
  gtk_signal_connect (GTK_OBJECT (mplex_only), "clicked",
                      GTK_SIGNAL_FUNC (mplex_convert), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), mplex_only, TRUE, TRUE, 0);
  gtk_widget_show(mplex_only);
}

void create_buttons2 (GtkWidget *hbox1)
{
  GtkWidget *create_vcd, *create_svcd, *set_defaults;

  create_vcd = gtk_button_new_with_label ("Set Options for VCD");
  gtk_signal_connect (GTK_OBJECT (create_vcd), "clicked",
                      GTK_SIGNAL_FUNC (do_vcd), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), create_vcd, TRUE, TRUE, 0);
  gtk_widget_show(create_vcd);

  create_svcd = gtk_button_new_with_label ("Set Options for SVCD");
  gtk_signal_connect (GTK_OBJECT (create_svcd), "clicked",
                      GTK_SIGNAL_FUNC (do_svcd), NULL);
  gtk_box_pack_start (GTK_BOX (hbox1), create_svcd, TRUE, TRUE, 0);
  gtk_widget_show(create_svcd);

  set_defaults = gtk_button_new_with_label ("Load Default Options");
  gtk_signal_connect (GTK_OBJECT (set_defaults), "clicked",
                      GTK_SIGNAL_FUNC (do_defaults), "load");
  gtk_box_pack_start (GTK_BOX (hbox1), set_defaults, TRUE, TRUE, 0);
  gtk_widget_show(set_defaults);

  set_defaults = gtk_button_new_with_label ("Save Default Options");
  gtk_signal_connect (GTK_OBJECT (set_defaults), "clicked",
                      GTK_SIGNAL_FUNC (do_defaults), "save");
  gtk_box_pack_start (GTK_BOX (hbox1), set_defaults, TRUE, TRUE, 0);
  gtk_widget_show(set_defaults);
}

/* Here some parts of status layout are done */
void create_status (GtkWidget *hbox1, GtkWidget *vbox)
{
GtkWidget *label1;

  label1 = gtk_label_new (" Executing : ");
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 5);
  gtk_widget_show (label1);

  execute_status = gtk_label_new("");
  gtk_widget_set_usize (execute_status, 550 , 40 );
  gtk_label_set_line_wrap (GTK_LABEL(execute_status), TRUE);
  gtk_label_set_justify (GTK_LABEL(execute_status), GTK_JUSTIFY_LEFT); 
  gtk_box_pack_start (GTK_BOX (hbox1), execute_status, TRUE, TRUE, 5);
  gtk_widget_show (execute_status);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_widget_show(vbox);
  hbox1 = gtk_hbox_new (FALSE, FALSE);

  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);
}

/* Create the Option Button with work distribution*/
void create_option_button(GSList *task_group, GtkWidget *table, 
       char task[LONGOPT], int encx, int ency)
{
GtkWidget *button_option;

  button_option = gtk_button_new_with_label (" Option ");
  gtk_signal_connect (GTK_OBJECT (button_option), "clicked",
                      GTK_SIGNAL_FUNC (open_mpeg_window), task);
  gtk_table_attach_defaults (GTK_TABLE (table), button_option, 
                                    encx, encx+1, ency, ency+1);
  gtk_widget_show(button_option);

}

/* Create the task layout, and the Option buttons */
/* Also Work distribution */
void create_task_layout(GtkWidget *table)
{
int encx, ency;
GtkWidget *button_mpeg1, *button_mpeg2, *button_vcd, *button_svcd;
GSList *task_group;

encx=0;
ency=2;


  button_mpeg1 = gtk_radio_button_new_with_label (NULL, "MPEG - 1");
/* what I set here hast to bue used by the options screen to set the correct
 * struct. I will also need it for the encoding process.change:force_options*/
//  gtk_signal_connect (GTK_OBJECT (button_mpeg1), "toggled",
//                      GTK_SIGNAL_FUNC (force_options), (gpointer) "mpeg1");
  gtk_table_attach_defaults (GTK_TABLE (table), button_mpeg1, 
                                    encx, encx+1, ency, ency+1);
  task_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button_mpeg1));
  gtk_widget_show (button_mpeg1);
  create_option_button(task_group, table, "mpeg1", encx+1, ency);
  ency++;

  button_mpeg2 = gtk_radio_button_new_with_label(task_group, "MPEG - 2");
//  gtk_signal_connect (GTK_OBJECT (button_mpeg2), "toggled",
//                      GTK_SIGNAL_FUNC (force_options), (gpointer) "mpeg2");
  gtk_table_attach_defaults (GTK_TABLE (table), button_mpeg2, 
                                    encx, encx+1, ency, ency+1);
  task_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button_mpeg2));
  gtk_widget_show (button_mpeg2);
  create_option_button(task_group, table, "mpeg2", encx+1, ency);
  ency++;

  button_vcd = gtk_radio_button_new_with_label(task_group, " VCD (MPEG-1) ");
//  gtk_signal_connect (GTK_OBJECT (button_vcd), "toggled",
//                      GTK_SIGNAL_FUNC (force_options), (gpointer) "vcd");
  gtk_table_attach_defaults (GTK_TABLE (table), button_vcd, 
                                    encx, encx+1, ency, ency+1);
  task_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button_vcd));
  gtk_widget_show (button_vcd);
  create_option_button(task_group, table, "vcd", encx+1, ency);
  ency++;
 
  button_svcd = gtk_radio_button_new_with_label(task_group, "SVCD (MPEG-2) ");
//  gtk_signal_connect (GTK_OBJECT (button_svcd), "toggled",
//                      GTK_SIGNAL_FUNC (force_options), (gpointer) "svcd");
  gtk_table_attach_defaults (GTK_TABLE (table), button_svcd, 
                                    encx, encx+1, ency, ency+1);
  task_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button_svcd));
  gtk_widget_show (button_svcd);
  create_option_button(task_group, table, "svcd", encx+1, ency);
  ency++;

}

/* Here all the work is distributed, and some basic parts of the layout done */
GtkWidget *create_lavencode_layout()
{
GtkWidget *vbox, *hbox1, *hbox, *vbox_main, *vbox1;
GtkWidget *separator, *label, *table; 
int enc_x,enc_y; 

  enc_x=0;
  enc_y=0; 
  /* main box (with borders) */
  hbox = gtk_hbox_new(FALSE,0);
  vbox_main = gtk_vbox_new(FALSE,0);

  vbox = gtk_vbox_new(FALSE,5);
  hbox1 = gtk_hbox_new(FALSE,2);

  /* buttons */
  hbox1 = gtk_hbox_new (TRUE, 20);
  create_buttons1 (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, TRUE, TRUE, 0);
  gtk_widget_show (hbox1);

  hbox1 = gtk_hbox_new (TRUE, 20);
  create_buttons2 (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, TRUE, TRUE, 0);
  gtk_widget_show (hbox1);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

  /* in/output layout */
  label = gtk_label_new (" Input and Output selection : ");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox1 = gtk_hbox_new (FALSE, 0);
  create_in_out (hbox1,vbox);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);
/* Till here most of the things could be reused */

  /* the table containing sound/mplex layout */
  hbox1 = gtk_hbox_new (FALSE, 0);
  vbox1 = gtk_vbox_new (FALSE, 0);

  /* the table containing the video layout */
  table = gtk_table_new (Encselect_x, Encselect_y, FALSE);

  /* video layout */
  label = gtk_label_new ("Select the task:");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_table_attach_defaults (GTK_TABLE (table), 
                              label, enc_x, enc_x+2, enc_y, enc_y+2);
  gtk_widget_show (label);
 
/* Old remove when the other version works  
 * create_video (table);                   */
  create_task_layout(table);
  gtk_box_pack_start (GTK_BOX (hbox1), table, FALSE, FALSE, 0);
  gtk_widget_show(table);

  /* and finaly, show the hbox in the vbox */
  gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, TRUE, 5);
  gtk_widget_show (hbox1);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

/* We can also use the status window */
  /* the status */
  hbox1 = gtk_hbox_new (FALSE, TRUE);
  create_status (hbox1,vbox);

  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 15);
  gtk_widget_show(vbox);
  gtk_box_pack_start (GTK_BOX (vbox_main), hbox, FALSE, TRUE, 15);
  gtk_widget_show(hbox);

  /* loading the configuration */
  do_defaults(NULL, "load");

  return (vbox_main);
}

