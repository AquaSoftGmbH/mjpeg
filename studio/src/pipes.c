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
#define HAVE_STDINT_H
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
#include <pthread.h>
#include <errno.h>

#include "parseconfig.h"
#include "pipes.h"
#include "y4m12.h"
#include "gtkfunctions.h"

/* ugh */
#define LAV2WAV_APP    0
#define MP2ENC_APP     1
#define LAV2YUV_APP    2
#define YUVSCALER_APP  3
#define MPEG2ENC_APP   4
#define MPLEX_APP      5
#define LAVPLAY_APP    6
#define LAVREC_APP     7
#define YUVPLAY_APP    8
#define LAVPIPE_APP    9
#define YUV2LAV_APP    10
#define YUVDENOISE_APP 11
#define YUV2DIVX_APP   12
#define LAVADDWAV_APP  13
#define LAVTRANS_APP   14
#define NUM_APPS       15

static char *app_locations[NUM_APPS];
static char *app_names[NUM_APPS] = {
  "lav2wav", "mp2enc",     "lav2yuv",  "yuvscaler", "mpeg2enc",
  "mplex",   "lavplay",    "lavrec",   "yuvplay",   "lavpipe",
  "yuv2lav", "yuvdenoise", "yuv2divx", "lavaddwav", "lavtrans" };

extern int verbose;

y4m12_t *l2y_y4m12;

int pipe_out[NUM], pipe_in[NUM];
int pid[NUM];
int reader[NUM];
int active[NUM];

/* lav2yuv reading thread */
pthread_t lav2yuv_reading_thread;

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
static int read_lav2yuv_data(gint source);

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

static int num_to_name(int number)
{
   switch (number)
   {
      case LAV2YUV:
      case LAV2YUV_S:
        return LAV2YUV_APP;
      case LAVPLAY:
      case LAVPLAY_E:
      case LAVPLAY_T:
        return LAVPLAY_APP;
      case LAVREC:
        return LAVREC_APP;
      case MPLEX:
        return MPLEX_APP;
      case MPEG2ENC:
        return MPEG2ENC_APP;
      case MP2ENC:
        return MP2ENC_APP;
      case LAV2WAV:
        return LAV2WAV_APP;
      case YUVSCALER:
        return YUVSCALER_APP;
      case YUVPLAY:
      case YUVPLAY_E:
        return YUVPLAY_APP;
      case LAVPIPE:
        return LAVPIPE_APP;
      case YUV2LAV:
      case YUV2LAV_E:
        return YUV2LAV_APP;
      case YUVDENOISE:
        return YUVDENOISE_APP;
      case YUV2DIVX:
        return YUV2DIVX_APP;
      case LAVADDWAV:
        return LAVADDWAV_APP;
      case LAVTRANS:
        return LAVTRANS_APP;
   }

   return -1;
}

char *app_name(int number) {
  if (num_to_name(number) >= 0)
    return app_names[num_to_name(number)];
  return NULL;
}

char *app_location(int number)
{
  if (num_to_name(number) >= 0)
    return app_locations[num_to_name(number)];
  return NULL;
}

static int read_lav2yuv_data(gint source)
{
int n;
   /* read lav2yuv data and write() it to mpeg2enc/yuvscaler and yuvplay */
   extern int use_yuvdenoise_pipe;
   extern int use_yuvscaler_pipe;
   extern int use_yuvplay_pipe;
n=0;

   if (!active[LAV2YUV_DATA])
   {
      /*int f;*/
      if (y4m12_read_header (l2y_y4m12, source)<0)
      {
         /* auch, bad */
         gdk_threads_enter();
         gtk_show_text_window(STUDIO_ERROR,
           "Error reading the header from lav2yuv\n");
         gdk_threads_leave();
         close_pipe(LAV2YUV); /* let's quit - TODO: better solution */
         active[LAV2YUV_DATA] = 0;
         return 0;
      }
      else
      {

         l2y_y4m12->buffer[0] = malloc(l2y_y4m12->width * l2y_y4m12->height
                           * sizeof(unsigned char));
         l2y_y4m12->buffer[1] = malloc(l2y_y4m12->width * l2y_y4m12->height
                           * sizeof(unsigned char) / 4);
         l2y_y4m12->buffer[2] = malloc(l2y_y4m12->width * l2y_y4m12->height
                           * sizeof(unsigned char) / 4);

         /* write header to mpeg2enc/yuvscaler/yuvdenoise and yuvplay */
         if (use_yuvplay_pipe)
         {
            y4m12_write_header (l2y_y4m12, pipe_out[YUVPLAY]);
         }

         if (use_yuvdenoise_pipe)
            n = pipe_out[YUVDENOISE];
         else if (use_yuvscaler_pipe)
            n = pipe_out[YUVSCALER];
         else
         {
            extern int studio_enc_format;
            switch (studio_enc_format)
            {
               case STUDIO_ENC_FORMAT_MPEG:
                  n = pipe_out[MPEG2ENC];
                  break;
               case STUDIO_ENC_FORMAT_DIVX:
                  n = pipe_out[YUV2DIVX];
                  break;
               case STUDIO_ENC_FORMAT_MJPEG:
                  n = pipe_out[YUV2LAV_E];
                  break;
            }
         }

         y4m12_write_header (l2y_y4m12, n);
      }
   }
   else
   {
      /* read frames */
      if (y4m12_read_frame(l2y_y4m12, source)<0)
      {
         if (active[LAV2YUV])
         {
            /* bad, bad, bad, something is wrong */
            gdk_threads_enter();
            gtk_show_text_window(STUDIO_ERROR,
              "Error reading frame data from lav2yuv\n");
            gdk_threads_leave();
            close_pipe(LAV2YUV); /* let's quit - TODO: better solution */
         }
         else
         {
            /* we're finished */
            close(pipe_in[LAV2YUV_DATA]);
         }
         active[LAV2YUV_DATA] = 0;
         return 0;
      }
      else
      {
         int n;
         n=0;

         if (use_yuvdenoise_pipe)
            n = pipe_out[YUVDENOISE];
         else if (use_yuvscaler_pipe)
            n = pipe_out[YUVSCALER];
         else
         {
            extern int studio_enc_format;
            switch (studio_enc_format)
            {
               case STUDIO_ENC_FORMAT_MPEG:
                  n = pipe_out[MPEG2ENC];
                  break;
               case STUDIO_ENC_FORMAT_DIVX:
                  n = pipe_out[YUV2DIVX];
                  break;
               case STUDIO_ENC_FORMAT_MJPEG:
                  n = pipe_out[YUV2LAV_E];
                  break;
            }
         }

         y4m12_write_frame (l2y_y4m12, n);

         if (use_yuvplay_pipe)
         {
            y4m12_write_frame (l2y_y4m12, pipe_out[YUVPLAY]);
         }
      }
   }

   return 1;
}

static void * lav2yuv_read_thread(void *arg)
{
   int fd = (int)arg;
   int n;

   /* Allow easy shutting down by other processes... */
   pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
   pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

   while(1)
   {
      n = read_lav2yuv_data(fd);
      active[LAV2YUV_DATA]++;
      if (active[LAV2YUV_DATA]==3)
         active[LAV2YUV_DATA] = 1;
      if (!n) break;
   }

   pthread_exit(NULL);
}

static void callback_pipes(gpointer data, gint source,
   GdkInputCondition condition)
{
   char input[4096];
   int n, i, number;
   char *app;

   number = (int)data;

   app = app_name(number);

   n = read(source, input, 4095);
   if (n<=0)
   {
      gdk_input_remove(reader[number]);
   }
   if (n==0)
   {
      extern int use_yuvdenoise_pipe;
      extern int use_yuvscaler_pipe;
      extern int use_yuvplay_pipe;
      extern int preview_or_render;

      /* program finished */
      if (verbose) printf("%s finished\n", app);

      /* close pipes/app */
      close(pipe_in[number]);
      if (number != MP2ENC && number != MPEG2ENC && number != YUVSCALER && number != YUVPLAY &&
         number != YUVPLAY_E && number != YUV2LAV && number != YUVDENOISE && number != YUV2DIVX &&
         number != YUV2LAV_E) {
         close(pipe_out[number]);
      }
      close_pipe(number);

      if (number == LAV2WAV) {
         close(pipe_out[MP2ENC]);
      }
      if (number == LAV2YUV) {
         if (use_yuvdenoise_pipe)
            close(pipe_out[YUVDENOISE]);
         else if (use_yuvscaler_pipe)
            close(pipe_out[YUVSCALER]);
         else
         {
            extern int studio_enc_format;
            switch (studio_enc_format)
            {
               case STUDIO_ENC_FORMAT_MPEG:
                  close(pipe_out[MPEG2ENC]);
                  break;
               case STUDIO_ENC_FORMAT_DIVX:
                  close(pipe_out[YUV2DIVX]);
                  break;
               case STUDIO_ENC_FORMAT_MJPEG:
                  close(pipe_out[YUV2LAV_E]);
                  break;
            }
         }

         if (use_yuvplay_pipe)
            close(pipe_out[YUVPLAY]);
      }
      if (number == YUVDENOISE) {
         if (use_yuvscaler_pipe)
            close(pipe_out[YUVSCALER]);
         else
         {
            extern int studio_enc_format;
            switch (studio_enc_format)
            {
               case STUDIO_ENC_FORMAT_MPEG:
                  close(pipe_out[MPEG2ENC]);
                  break;
               case STUDIO_ENC_FORMAT_DIVX:
                  close(pipe_out[YUV2DIVX]);
                  break;
               case STUDIO_ENC_FORMAT_MJPEG:
                  close(pipe_out[YUV2LAV_E]);
                  break;
            }
         }
      }
      if (number == YUVSCALER) {
         extern int studio_enc_format;
         switch (studio_enc_format)
         {
            case STUDIO_ENC_FORMAT_MPEG:
               close(pipe_out[MPEG2ENC]);
               break;
            case STUDIO_ENC_FORMAT_DIVX:
               close(pipe_out[YUV2DIVX]);
               break;
            case STUDIO_ENC_FORMAT_MJPEG:
               close(pipe_out[YUV2LAV_E]);
               break;
         }
      }
      if (number == LAVPIPE) {
         if (preview_or_render)
            close(pipe_out[YUV2LAV]);
         else
            close(pipe_out[YUVPLAY_E]);
      }

      /* trigger callback function for each specific app */
      if (number == MPEG2ENC || number == MP2ENC || number == MPLEX || number == YUV2DIVX ||
          number == YUV2LAV_E) {
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
            if (number == MP2ENC && strncmp(temp, "--DEBUG: ", 9)==0)
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
               case YUV2DIVX:
               case YUV2LAV_E:
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
   if (!app_location(number) || app_location(number)[0]!='/')
   {
      gtk_show_text_window(STUDIO_ERROR,
         "No valid application location found for %s. "
         "Please install and specify the correct path "
         "in the options screen in order to use it",
         app_name(number));
      return;
   }

   if (!active[number])
   {
      int ipipe[2], opipe[2], spipe[2];
      int n;

      if(pipe(ipipe) || pipe(opipe))
      {
         gtk_show_text_window(STUDIO_ERROR, "pipe() failed");
         return;
      }

      if (number == LAV2YUV)
      {
         if (pipe(spipe))
         {
            gtk_show_text_window(STUDIO_ERROR, "pipe() failed");
            return;
         }
      }

      pid[number] = fork();
      if(pid[number]<0)
      {
         gtk_show_text_window(STUDIO_ERROR, "fork() failed");
         return;
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
            active[LAV2YUV_DATA] = 0;
            pipe_in[LAV2YUV_DATA] = spipe[0];
            close(spipe[1]);
            pthread_create(&lav2yuv_reading_thread, NULL,
               lav2yuv_read_thread, (void *) pipe_in[LAV2YUV_DATA]);
         }

         pipe_out[number] = ipipe[1]; /* don't O_NONBLOCK it! */
         close(ipipe[0]);
      }
      else /* This is the child process (i.e. lav2wav/mp2enc) */
      {
         extern int use_yuvscaler_pipe;
         /*extern int use_yuvdenoise_pipe;*/
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
         else if (number == LAV2YUV) {
            close(spipe[0]);
            n = dup2(spipe[1],1);
            close(spipe[1]);
         }
         else if (number == YUVSCALER) {
            extern int studio_enc_format;
            switch (studio_enc_format)
            {
               case STUDIO_ENC_FORMAT_MPEG:
                  n = dup2(pipe_out[MPEG2ENC],1); /* writes yuvscaler directly to mpeg2enc */
                  break;
               case STUDIO_ENC_FORMAT_DIVX:
                  n = dup2(pipe_out[YUV2DIVX],1); /* writes yuvscaler directly to yuv2divx */
                  break;
               case STUDIO_ENC_FORMAT_MJPEG:
                  n = dup2(pipe_out[YUV2LAV_E],1); /* writes yuvscaler directly to yuv2lav */
                  break;
            }
         }
         else if (number == YUVDENOISE) {
           if (use_yuvscaler_pipe)
              n = dup2(pipe_out[YUVSCALER],1); /* writes yuvdenoise directly to yuvscaler */
           else
           {
              extern int studio_enc_format;
              switch (studio_enc_format)
              {
                 case STUDIO_ENC_FORMAT_MPEG:
                    n = dup2(pipe_out[MPEG2ENC],1); /* writes yuvdenoise directly to mpeg2enc */
                    break;
                 case STUDIO_ENC_FORMAT_DIVX:
                    n = dup2(pipe_out[YUV2DIVX],1); /* writes yuvdenoise directly to yuv2divx */
                    break;
                 case STUDIO_ENC_FORMAT_MJPEG:
                    n = dup2(pipe_out[YUV2LAV_E],1); /* writes yuvdenoise directly to yuv2lav */
                    break;
              }
           }
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

         execvp(app_location(number), command);

         /* uh oh, app does not exist */
         fprintf(stderr, "**ERROR: %s (\'%s\') could not be started: %s\n",
            app_name(number), app_location(number), strerror(errno));

         _exit(1);
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

   /* default app locations */
   app_locations[LAV2WAV_APP] = LAV2WAV_LOCATION;
   app_locations[MP2ENC_APP] = MP2ENC_LOCATION;
   app_locations[LAV2YUV_APP] = LAV2YUV_LOCATION;
   app_locations[YUVSCALER_APP] = YUVSCALER_LOCATION;
   app_locations[MPEG2ENC_APP] = MPEG2ENC_LOCATION;
   app_locations[MPLEX_APP] = MPLEX_LOCATION;
   app_locations[LAVPLAY_APP] = LAVPLAY_LOCATION;
   app_locations[LAVREC_APP] = LAVREC_LOCATION;
   app_locations[YUVPLAY_APP] = YUVPLAY_LOCATION;
   app_locations[LAVPIPE_APP] = LAVPIPE_LOCATION;
   app_locations[YUV2LAV_APP] = YUV2LAV_LOCATION;
   app_locations[YUVDENOISE_APP] = YUVDENOISE_LOCATION;
   app_locations[YUV2DIVX_APP] = YUV2DIVX_LOCATION;
   app_locations[LAVADDWAV_APP] = LAVADDWAV_LOCATION;
   app_locations[LAVTRANS_APP] = LAVTRANS_LOCATION;

   l2y_y4m12 = y4m12_malloc();
}

void save_app_locations(FILE *fp)
{
  int i;

  fprintf(fp, "\n[ApplicationLocations]\n");
  for (i=0;i<NUM_APPS;i++)
  {
    fprintf(fp, "default_application_location_%s = %s\n",
      app_names[i], app_locations[i]);
  }
}

void load_app_locations()
{
  char buff[256];
  char *val;
  int i;
  
  for (i=0;i<NUM_APPS;i++)
  {
    sprintf(buff, "default_application_location_%s", app_names[i]);
    if (NULL != (val = cfg_get_str("ApplicationLocations",buff)))
    {
      if (val[0] == '/' || !strcmp(val, "not available"))
      {
        app_locations[i] = val;
        if (verbose)
           printf("%s - \'%s\' loaded\n", buff, val);
      }
      else
      {
        printf("Config file Error: %s == \'%s\' not allowed\n", buff, val);
      }
    }
    else
      if (verbose)
        printf("%s - default \'%s\' loaded\n", buff, app_locations[i]);
  }
}
