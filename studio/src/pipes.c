/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
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

#include "pipes.h"
#include "yuv4mpeg.h"
#include "gtkfunctions.h"

extern int verbose;

static unsigned char *l2y_yuv[3];
static int l2y_h,l2y_w; /* width/height for lav2yuv YUV4MPEG data */

int pipe_out[NUM], pipe_in[NUM];
int pid[NUM];
int reader[NUM];
int active[NUM];

/* external callbacks, I'll make void-variables later on */
void continue_encoding(void);
void lavencode_callback(int number, char *input);
void lavplay_trimming_callback(char *msg);
void scene_detection_input_cb(char *input);
void scene_detection_finished(void);
void process_lavplay_edit_input(char *msg);
void quit_trimming(GtkWidget *widget, gpointer data);
void quit_lavplay_edit(void);
void continue_encoding(void);
void scene_detection_finished(void);
void dispatch_input(char *buff);
void lavrec_quit(void);
void process_lavplay_input(char *msg);
void lavplay_stopped(void);
void lavplay_edit_stopped(void);
void effects_finished(void);
void effects_callback(int number, char *msg);

/* some static private functions */
static void callback_pipes(gpointer data, gint source,GdkInputCondition condition);
static void read_lav2yuv_data(gint source);

int pipe_is_active(int number) {
   return active[number];
}

void write_pipe(int number, char *message) {
   if (active[number]) {
      write(pipe_out[number], message, strlen(message));
   }
   if (verbose) printf("  TO %10.10s: %s", app_name(number), message);
}

void close_pipe(int number) {
   if (active[number]) {
      kill(pid[number],SIGINT);
      waitpid (pid[number], NULL, 0);
      active[number] = 0;
   }
}

char *app_name(int number) {
   char *app;

   switch (number) {
      case LAV2YUV:
      case LAV2YUV_S:
        app = "lav2yuv";
        break;
      case LAVPLAY:
      case LAVPLAY_E:
      case LAVPLAY_T:
        app = "lavplay";
        break;
      case LAVREC:
        app = "lavrec";
        break;
      case MPLEX:
        app = "mplex";
        break;
      case MPEG2ENC:
        app = "mpeg2enc";
        break;
      case MP2ENC:
        app = "mp2enc";
        break;
      case LAV2WAV:
        app = "lav2wav";
        break;
      case YUVSCALER:
        app = "yuvscaler";
        break;
      case YUVPLAY:
      case YUVPLAY_E:
        app = "yuvplay";
        break;
      case LAVPIPE:
        app = "lavpipe";
        break;
      case YUV2LAV:
        app = "yuv2lav";
        break;
      default:
        app = "unknown";
        break;
   }

   return app;
}

static void read_lav2yuv_data(gint source)
{
   /* read lav2yuv data and write() it to mpeg2enc/yuvscaler and yuvplay */
   extern int use_yuvscaler_pipe;
   extern int use_yuvplay_pipe;

   if (!active[LAV2YUV_DATA])
   {
      int f;
      if (yuv_read_header (source, &l2y_w, &l2y_h, &f)<0)
      {
         /* auch, bad */
         gtk_show_text_window(STUDIO_ERROR,
            "Error reading the header from lav2yuv", NULL);
         close_pipe(LAV2YUV); /* let's quit - TODO: better solution */
         active[LAV2YUV_DATA] = 0;
      }
      else
      {
         l2y_yuv[0] = malloc(l2y_w * l2y_h * sizeof(unsigned char));
         l2y_yuv[1] = malloc(l2y_w * l2y_h * sizeof(unsigned char) / 4);
         l2y_yuv[2] = malloc(l2y_w * l2y_h * sizeof(unsigned char) / 4);

         /* write header to mpeg2enc/yuvscaler and yuvplay */
         if (use_yuvplay_pipe) {
            yuv_write_header (pipe_out[YUVPLAY], l2y_w, l2y_h, f); }
         if (use_yuvscaler_pipe)
            yuv_write_header (pipe_out[YUVSCALER], l2y_w, l2y_h, f);
         else
            yuv_write_header (pipe_out[MPEG2ENC], l2y_w, l2y_h, f);
      }
   }
   else
   {
      /* read frames */
      if (yuv_read_frame(source, l2y_yuv, l2y_w, l2y_h)==0)
      {
         if (active[LAV2YUV])
         {
            /* bad, bad, bad, something is wrong */
            gtk_show_text_window(STUDIO_ERROR,
               "Error reading frame data from lav2yuv", NULL);
            close_pipe(LAV2YUV); /* let's quit - TODO: better solution */
         }
         else
         {
            /* we're finished */
            close(pipe_in[LAV2YUV_DATA]);
            gdk_input_remove(reader[LAV2YUV_DATA]);
         }
         active[LAV2YUV_DATA] = 0;
      }
      else
      {
         if (use_yuvscaler_pipe)
            yuv_write_frame (pipe_out[YUVSCALER], l2y_yuv, l2y_w, l2y_h);
         else
            yuv_write_frame (pipe_out[MPEG2ENC], l2y_yuv, l2y_w, l2y_h);
         if (use_yuvplay_pipe) {
            yuv_write_frame (pipe_out[YUVPLAY], l2y_yuv, l2y_w, l2y_h); }
      }
   }
}

static void callback_pipes(gpointer data, gint source,
   GdkInputCondition condition)
{
   char input[4096];
   int n, i, number;
   char *app;

   number = (int)data;

   if (number == LAV2YUV_DATA) {
      if (active[LAV2YUV_DATA]<2)
         read_lav2yuv_data(source);
      active[LAV2YUV_DATA]++;
      if (active[LAV2YUV_DATA]==3)
         active[LAV2YUV_DATA] = 1;
      return;
   }

   app = app_name(number);

   n = read(source, input, 4095);
   if (n==0)
   {
      extern int use_yuvscaler_pipe;
      extern int use_yuvplay_pipe;
      extern int preview_or_render;

      /* program finished */
      if (verbose) printf("%s finished\n", app);

      /* close pipes/app */
      close(pipe_in[number]);
      if (number != MP2ENC && number != MPEG2ENC && number != YUVSCALER && number != YUVPLAY &&
         number != YUVPLAY_E && number != YUV2LAV) {
         close(pipe_out[number]);
      }
      close_pipe(number);

      if (number == LAV2WAV) {
         close(pipe_out[MP2ENC]);
      }
      if (number == LAV2YUV) {
         if (use_yuvscaler_pipe)
            close(pipe_out[YUVSCALER]);
         else
            close(pipe_out[MPEG2ENC]);
         if (use_yuvplay_pipe)
            close(pipe_out[YUVPLAY]);
      }
      if (number == YUVSCALER) {
         close(pipe_out[MPEG2ENC]);
      }
      if (number == LAVPIPE) {
         if (preview_or_render)
            close(pipe_out[YUV2LAV]);
         else
            close(pipe_out[YUVPLAY_E]);
      }

      /* trigger callback function for each specific app */
      if (number == MPEG2ENC || number == MP2ENC || number == MPLEX) {
         continue_encoding();
      }
      else if (number == LAV2YUV_S) {
         scene_detection_finished();
      }
      else if (number == LAVPLAY_E) {
         lavplay_edit_stopped();
      }
      else if (number == LAVPLAY_T) {
         quit_trimming(NULL,NULL);
      }
      else if (number == LAVREC) {
         lavrec_quit();
      }
      else if (number == LAVPLAY) {
         lavplay_stopped();
      }
      else if (number == YUV2LAV || number == YUVPLAY_E) {
         effects_finished();
      }

      /* officially, we should detach the gdk_input here */
      gdk_input_remove(reader[number]);
   }
   else
   {
      int x = 0;
      char temp[256], endsign;

      for(i=0;i<n;i++)
      {
         if(input[i]=='\n' || input[i]=='\r' || i==n-1)
         {
            strncpy(temp, input+x, i-x);
            if (i-x<255) {
               if (i==n-1 && input[i]!='\r' && input[i]!='\n') {
                  temp[i-x] = input[i];
                  if (i-x<254)
                     temp[i-x+1] = '\0';
               }
               else
                  temp[i-x] = '\0';
            }
            endsign = '\n';
            if (input[i] == '\n' || input[i] == '\r')
               endsign = input[i];
            if (input[x] == '@')
               endsign = '\r';
            if (number == LAV2YUV_S && strncmp(temp, "--DEBUG: frame", 14)==0)
               endsign = '\r';
            if (number == MPEG2ENC && strncmp(temp, "   INFO: Frame", 14)==0)
               endsign = '\r';

            if(!(number == LAVPLAY && strncmp(temp, "--DEBUG: frame=", 15)==0)) {
               if (verbose) {
                  fprintf(stdout, "FROM %10.10s: %s%c", app, temp, endsign);
                  fflush(stdout);
               }
            }

            switch(number) {
               case MPEG2ENC:
               case LAV2YUV:
               case LAV2WAV:
               case MP2ENC:
               case MPLEX:
               case YUVSCALER:
               case YUVPLAY:
                  lavencode_callback(number, temp);
                  break;
               case LAV2YUV_S:
                  scene_detection_input_cb(temp);
                  break;
               case LAVPLAY_E:
                  process_lavplay_edit_input(temp);
                  break;
               case LAVPLAY:
                  process_lavplay_input(temp);
                  break;
               case LAVPLAY_T:
                  lavplay_trimming_callback(temp);
                  break;
               case LAVREC:
                  dispatch_input(temp);
                  break;
               case YUV2LAV:
               case YUVPLAY_E:
               case LAVPIPE:
                  effects_callback(number, temp);
                  break;
            }

            x = i+1;
         }
      }
   }
}

void start_pipe_command(char *command[], int number)
{
   if (!active[number])
   {
      int ipipe[2], opipe[2], spipe[2];
      int n;

      if(pipe(ipipe) || pipe(opipe))
      {
         perror("pipe() failed");
         exit(1);
      }

      if (number == LAV2YUV)
      {
         if (pipe(spipe))
         {
            perror("pipe() failed");
            exit(1);
         }
      }

      pid[number] = fork();
      if(pid[number]<0)
      {
         perror("fork() failed");
         exit(1);
      }

      active[number] = 1;

      if (pid[number]) /* This is the parent process (i.e. LVS) */
      {
         /* parent */
         pipe_in[number] = opipe[0];
         close(opipe[1]);
         fcntl (pipe_in[number], F_SETFL, O_NONBLOCK);
         reader[number] = gdk_input_add (pipe_in[number],
            GDK_INPUT_READ, callback_pipes, (gpointer)number);

         if (number == LAV2YUV)
         {
            pipe_in[LAV2YUV_DATA] = spipe[0];
            close(spipe[1]);
            reader[LAV2YUV_DATA] = gdk_input_add (pipe_in[LAV2YUV_DATA],
               GDK_INPUT_READ, callback_pipes, (gpointer)LAV2YUV_DATA);
         }

         pipe_out[number] = ipipe[1]; /* don't O_NONBLOCK it! */
         close(ipipe[0]);
      }
      else /* This is the child process (i.e. lav2wav/mp2enc) */
      {
/*         extern int use_yuvscaler_pipe;*/
         extern int preview_or_render;

         /* child */
         close(ipipe[1]);
         close(opipe[0]);

         n = dup2(ipipe[0],0);
         if(n!=0) exit(1);
         close(ipipe[0]);

         if (number == LAV2WAV) {
            n = dup2(pipe_out[MP2ENC],1); /* writes lav2wav directly to mp2enc */
         }
#if 0
         else if (number == LAV2YUV) {
            if (use_yuvscaler_pipe)
               n = dup2(pipe_out[YUVSCALER],1); /* writes lav2wav directly to yuvscaler */
            else
               n = dup2(pipe_out[MPEG2ENC],1); /* writes lav2wav directly to mpeg2enc */
         }
#endif
         else if (number == LAV2YUV) {
            close(spipe[0]);
            n = dup2(spipe[1],1);
            close(spipe[1]);
         }
         else if (number == YUVSCALER) {
            n = dup2(pipe_out[MPEG2ENC],1); /* writes yuvscaler directly to mpeg2enc */
         }
         else if (number == LAVPIPE) {
            if (preview_or_render)
               n = dup2(pipe_out[YUV2LAV],1); /* writes lavpipe directly to yuv2lav */
            else
               n = dup2(pipe_out[YUVPLAY_E],1); /* writes lavpipe directly to yuvplay */
         }
         else {
            n = dup2(opipe[1],1);
         }
         if(n!=1) exit(1);

         n = dup2(opipe[1],2);
         if(n!=2) exit(1);
         close(opipe[1]);

         execvp(command[0], command);
         exit(1);
      }
   }
   else
   {
      printf("**ERROR: %s is already active\n", app_name(number));
   }
}

void init_pipes()
{
   int i;
   for (i=0;i<NUM;i++)
   {
      active[i] = 0;
   }
}
