/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * lavencode_mpeg done by Bernhard Praschinger
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
 * Here is the layout for the mpeg encoding options is created.
 * It is-will be reused tor all mpeg versions. 
 * also mpeg1, mpeg2, vcd, svcd, DVD 
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


/* Forward declarations */
void open_mpeg_window(GtkWidget *widget, gpointer data);
void accept_mpegoptions(GtkWidget *widget, gpointer data);
void create_audio_mplex_layout(GtkWidget *hbox);
void create_yuv2mpeg_layout(GtkWidget *hbox);
void create_sound_encoding (GtkWidget *table,int *tx,int *ty);
void create_mplex_encoding (GtkWidget *table, int *tx, int *ty);
void create_video_options_layout (GtkWidget *table, int *tx, int *ty);
void create_yuvscaler_layout (GtkWidget *table, int *tx, int *ty);
void create_video_layout (GtkWidget *table, int *tx, int *ty);
void set_mplex_muxfmt (GtkWidget *widget, gpointer data);
void set_audiobitrate (GtkWidget *widget, gpointer data);
void set_samplebitrate (GtkWidget *widget, gpointer data);
void force_options (GtkWidget *widget, gpointer data);
void set_noisefilter (GtkWidget *widget, gpointer data);
void set_drop_samples (GtkWidget *widget, gpointer data);
void set_activewindow (GtkWidget *widget, gpointer data);
void set_outputformat (GtkWidget *widget, gpointer data);
void set_scalerstrings (GtkWidget *widget, gpointer data);
void set_videobit (GtkWidget *widget, gpointer data);
void set_searchrad (GtkWidget *widget, gpointer data);
void init_tempenco(gpointer task);
void show_data(gpointer task);
void set_decoderbuffer(GtkWidget *widget, gpointer data);
void set_datarate (GtkWidget *widget, gpointer data);
void set_vbr (GtkWidget *widget, gpointer data);
void set_qualityfa (GtkWidget *widget, gpointer data);
void set_minGop (GtkWidget *widget, gpointer data);
void set_maxGop (GtkWidget *widget, gpointer data);
void set_sequencesize (GtkWidget *widget, gpointer data);
void set_nonvideorate (GtkWidget *widget, gpointer data);

/* Some variables */
GList *samples = NULL; 
GList *muxformat = NULL;
GList *outputformat = NULL;
struct encodingoptions tempenco;
struct encodingoptions *point;    /* points to the encoding struct to change */

GtkWidget *combo_entry_active, *combo_entry_scalerinput, *combo_entry_scaleroutput;
GtkWidget *combo_entry_scalermode, *combo_entry_outputformat;
GtkWidget *combo_entry_samples, *combo_entry_noisefilter, *combo_entry_audiobitrate;
GtkWidget *combo_entry_samplebitrate, *button_force_stereo, *button_force_mono;
GtkWidget *button_force_vcd, *combo_entry_searchradius, *combo_entry_muxfmt;
GtkWidget *combo_entry_videobitrate, *combo_entry_decoderbuffer, *switch_vbr; 
GtkWidget *combo_entry_streamrate, *combo_entry_qualityfa, *combo_entry_minGop;
GtkWidget *combo_entry_maxGop, *combo_entry_sequencemb, *combo_entry_nonvideo;
/* =============================================================== */
/* Start of the code */

/* Set the tempenco struct to defined values */
void init_tempenco(gpointer task)
{

  if (verbose)
    printf("Which task to do :%s \n",(char*)task);

  if (strcmp ((char*)task,"MPEG1") == 0)
    point = &encoding;
  else if (strcmp ((char*)task,"MPEG2") == 0)
    point = &encoding2;
  else if (strcmp ((char*)task,"VCD") == 0)
    point = &encoding_vcd;
  else if (strcmp ((char*)task,"SVCD") == 0)
    point = &encoding_svcd;
  else 
    point = &encoding; /* fallback should never be used ;) */


  sprintf(tempenco.notblacksize,"%s",(*point).notblacksize);
  sprintf(tempenco.input_use,"%s",(*point).input_use);
  sprintf(tempenco.output_size,"%s",(*point).output_size);
  sprintf(tempenco.mode_keyword,"%s",(*point).mode_keyword);
  sprintf(tempenco.ininterlace_type,"%s",(*point).ininterlace_type);
  tempenco.addoutputnorm=(*point).addoutputnorm;
  tempenco.outputformat=(*point).outputformat;
  tempenco.droplsb=(*point).droplsb;
  tempenco.noisefilter=(*point).noisefilter;
  tempenco.audiobitrate=(*point).audiobitrate;
  tempenco.outputbitrate=(*point).outputbitrate;
  sprintf(tempenco.forcestereo,"%s",(*point).forcestereo);
  sprintf(tempenco.forcemono,"%s",(*point).forcemono);
  sprintf(tempenco.forcevcd,"%s",(*point).forcevcd);
  tempenco.bitrate=(*point).bitrate;
  tempenco.qualityfactor=(*point).qualityfactor;
  tempenco.minGop=(*point).minGop;
  tempenco.maxGop=(*point).maxGop;
  tempenco.sequencesize=(*point).sequencesize;
  tempenco.nonvideorate=(*point).nonvideorate;
  tempenco.searchradius=(*point).searchradius;
  tempenco.muxformat=(*point).muxformat;
  sprintf(tempenco.muxvbr,"%s",(*point).muxvbr);
  tempenco.streamdatarate=(*point).streamdatarate;
  tempenco.decoderbuffer=(*point).decoderbuffer;
}

/* setting the values of the GTK_ENTRY's */
void show_data(gpointer task)
{
int i;
char val[LONGOPT];

  /* lav2yuv option */
  gtk_entry_set_text(GTK_ENTRY(combo_entry_active), tempenco.notblacksize);

  /* yuvscaler options */
  gtk_entry_set_text(GTK_ENTRY(combo_entry_scalerinput),tempenco.input_use);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_scaleroutput), tempenco.output_size);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_scalermode), tempenco.mode_keyword);

  /* lav2yuv options */
  outputformat = g_list_first (outputformat);
  for (i = 0; i < tempenco.outputformat ;i++)
    outputformat = g_list_next (outputformat);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_outputformat), outputformat->data);

  sprintf(val,"%i",tempenco.droplsb);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_samples), val);

  sprintf(val,"%i",tempenco.noisefilter);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_noisefilter), val);

  /* Audio Options  */
  sprintf(val,"%i",tempenco.audiobitrate);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_audiobitrate), val);
  sprintf(val,"%i00",tempenco.outputbitrate);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_samplebitrate), val);

  if (tempenco.forcestereo[0] == '-')
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_force_stereo),TRUE);
  if (tempenco.forcemono[0] == '-')
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_force_mono),TRUE);
  if (tempenco.forcevcd[0] == '-')
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_force_vcd),TRUE);

  sprintf(val,"%i",tempenco.bitrate);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_videobitrate),val);

  if (tempenco.minGop == 12)
    gtk_entry_set_text(GTK_ENTRY(combo_entry_minGop),"default 12");
  else
    {
       sprintf(val,"%i",tempenco.minGop);
       gtk_entry_set_text(GTK_ENTRY(combo_entry_minGop),val);
    }

  if (tempenco.maxGop == 12)
    gtk_entry_set_text(GTK_ENTRY(combo_entry_maxGop),"default 12");
  else
    {
       sprintf(val,"%i",tempenco.maxGop);
       gtk_entry_set_text(GTK_ENTRY(combo_entry_maxGop),val);
    }

  sprintf(val,"%i",tempenco.searchradius);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_searchradius),val);

  /* Mplex Options how do I change this ? */
  muxformat = g_list_first (muxformat);
  for (i = 0; i < tempenco.muxformat ;i++)
    muxformat = g_list_next (muxformat);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_muxfmt), muxformat->data);

  if (tempenco.muxvbr[0] == '-')
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(switch_vbr),TRUE);

  sprintf(val,"%i",tempenco.decoderbuffer);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_decoderbuffer), val);

  if ( tempenco.streamdatarate == 0)
    sprintf(val,"auto");
  else
    sprintf(val,"%i",tempenco.streamdatarate);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_streamrate), val);
}

/* set the noisfilter for lav2yuv */
void set_noisefilter (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;

  i = 0;
  test = gtk_entry_get_text(GTK_ENTRY(widget));
  i = atoi ( test );

  if ( (i >= 0) && (i <= 2) )
    tempenco.noisefilter = i;

  if (verbose)
    printf(" selected noise filer grade : %i \n", tempenco.noisefilter);
}

/* set the number of LSBs dropped */
void set_drop_samples (GtkWidget *widget, gpointer data)
{
char *test;
int i;

  i = 0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  i = atoi ( test );

  if ( (i >= 0) && (i <= 4) )
    tempenco.droplsb = i;

  if (verbose)
    printf(" dropping as many LSB of each sample : %i \n", tempenco.droplsb);
}

/* Set the string for the lav2yuv active window */
void set_activewindow (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;

  i=0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

/* Old version */
//  if (strcmp(test,"as is") == 0)
//    for (i=0; i < LONGOPT; i++)
//       tempenco.notblacksize[i]='\0';
//  else
    sprintf(tempenco.notblacksize,"%s", test);

  if (verbose)
    printf (" Set activ window to : %s\n", test);
}

/* set the output format of lav2yuv */
void set_outputformat (GtkWidget *widget, gpointer data)
{
char *test;
int i;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  outputformat = g_list_last (outputformat);

  for (i = (g_list_length (g_list_first(outputformat))-1) ; i > 0 ; i--)
    {
      if (strcmp (test,outputformat->data) == 0)
                break;

      outputformat = g_list_previous (outputformat);
    }
    tempenco.outputformat = i;

  outputformat = g_list_first (outputformat);

  if (verbose)
    printf(" selected output format: %i \n", tempenco.outputformat);
}

/* set the decoder buffer size for mplex and mpeg2enc */
void set_decoderbuffer(GtkWidget *widget, gpointer data)
{
char *test;
int i;

  i = 0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  i = atoi ( test );

  if ( (i >= 20) && (i <= 1000) )
    tempenco.decoderbuffer = i;

  if (verbose)
    printf(" Set decoder buffer to : %i kB\n", tempenco.decoderbuffer);
}

/* set the vbr flag for the multiplexer */
void set_vbr(GtkWidget *widget, gpointer data)
{
int i;

  if (GTK_TOGGLE_BUTTON (widget)->active)
    sprintf(tempenco.muxvbr,"-V");
  else
    for (i = 0; i < SHORTOPT; i++)
      tempenco.muxvbr[i]='\0'; 
 
  if (verbose)
    printf(" Set the mux vbr flag to : %s \n",tempenco.muxvbr);
}

/* set the data rate of the multiplexed stream */
void set_datarate(GtkWidget *widget, gpointer data)
{
char *test;
int i;

  i = 0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if(strcmp(test,"auto") != 0)
  {
    i = atoi ( test );
    tempenco.streamdatarate = i;
  }
  else 
    tempenco.streamdatarate = 0;
  
  if (verbose)
    printf(" Set Stream data rate to : %i kB\n", tempenco.streamdatarate);
}

/* Set the sequencesize of the final multiplexed stream */
void set_sequencesize(GtkWidget *widget, gpointer data)
{
char *test;
int i;

  i = 0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if(strcmp(test,"disabled") != 0)
  {
    i = atoi ( test );
    tempenco.sequencesize = i;
  }
  else
    tempenco.sequencesize = 0;

  if (verbose)
    printf(" Set the sequence size to %i MB\n",tempenco.sequencesize); 
}

/* Set the non video rate of the final multiplexed stream */
void set_nonvideorate(GtkWidget *widget, gpointer data)
{
char *test;
int i;

  i = 0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if(strcmp(test,"auto") != 0)
  {
    i = atoi ( test );
    tempenco.nonvideorate = i;
  }
  else
    tempenco.nonvideorate = 0;

  if (verbose)
    printf(" Set the non video rate in the final stream to: %i kBit/s\n", 
                                               tempenco.nonvideorate);
}

/* set the mplex format */
void set_mplex_muxfmt (GtkWidget *widget, gpointer data)
{
  gchar *test;
  int i;

  i = 0;
  test = gtk_entry_get_text(GTK_ENTRY(widget));


  muxformat = g_list_last (muxformat);

  for (i = (g_list_length (g_list_first(muxformat))-1) ; i > 0 ; i--)
   {
     if (strcmp (test,muxformat->data) == 0)
               break;

     muxformat = g_list_previous (muxformat);
   }

  tempenco.muxformat = i;

  muxformat = g_list_first (muxformat);

  if (verbose)
    printf(" selected multiplexing format: %i \n", tempenco.muxformat);
}

/* Set the string for the yuvscaler options */
void set_scalerstrings (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;

  i=0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));


  if (data == "SCALERINPUT")
  {
    if (strlen(test) > 0 )
        sprintf(tempenco.input_use,"%s",test);

    if (verbose)
      printf(" selected yuvscaler input window: %s \n", tempenco.input_use);
  }

  if (data == "SCALERMODE")
  {
    if (strlen(test) > 0 )
      sprintf(tempenco.mode_keyword,"%s",test);

    if (verbose)
      printf(" selected yuvscaler scaling mode: %s \n", tempenco.mode_keyword);
  }

  if (data == "SCALEROUTPUT")
  {
    if (strcmp(test,"as is") != 0 )
      {
      if ( strcmp(test,"VCD") == 0)
      sprintf(tempenco.output_size,"%s", test);
    else if ( strcmp(test,"SVCD") == 0)
      sprintf(tempenco.output_size,"%s", test);
    else if ( strncmp(test,"INTERLACED_",11) == 0)
      sprintf(tempenco.output_size,"%s", test);
    else
      sprintf(tempenco.output_size,"SIZE_%s", test);
      }

    if (verbose)
      printf(" selected yuvscaler output size: %s \n", tempenco.output_size);
  }
}

/* set output bitrate for the video */
void set_videobit (GtkWidget *widget, gpointer data)
{
char *test;
int i;
i=0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if ( strcmp(test, "disabled") == 0 )
    tempenco.bitrate = 0;
  else
    tempenco.bitrate = atoi ( test );

  if (verbose)
    printf(" selected video bitrate: %i \n", tempenco.bitrate);
}

/* Set the quality factor for the stream VBR */
void set_qualityfa (GtkWidget *widget, gpointer data)
{
char *test;
int i;
i=0;
 
  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if ( strcmp(test, "disabled") == 0 )
    tempenco.qualityfactor = 0;
  else
    {
      i = atoi (test);
      if ( (i <= 31) && (i >= 1) )
        tempenco.qualityfactor = i;
    }

  if (verbose)
    printf(" selected quality factor: %i \n", tempenco.qualityfactor);
}

/* Set the min GOP */
void set_minGop (GtkWidget *widget, gpointer data)
{
char *test;
int i;
i=0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  i = atoi (test);

  if ( strncmp(test, "default", 7) == 0)
    tempenco.minGop = 12;
  else 
    {
      i = atoi (test);
      if ( (i <= 28) && (i >= 6) )
        tempenco.minGop = i;
    }

  if (verbose)
    printf(" selected Minimum Gop size: %i \n", tempenco.minGop);
}

/* Set the max GOP */
void set_maxGop (GtkWidget *widget, gpointer data)
{
char *test;
int i;
i=0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  if ( strncmp(test, "default", 7) == 0)
    tempenco.maxGop = 12;
  else 
    {
      i = atoi (test);
      if ( (i <= 28) && (i >= 6) )
        tempenco.maxGop = i;
    }

  if (verbose)
    printf(" selected Maximum Gop size: %i \n", tempenco.maxGop);
}

/* set output bitrate for the video */
void set_searchrad (GtkWidget *widget, gpointer data)
{
char *test;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  tempenco.searchradius = atoi ( test );

  if (verbose)
    printf(" selected searchradius : %i \n", tempenco.searchradius);
}

/* creating the options for mplex */
void create_mplex_encoding (GtkWidget *table, int *tx, int *ty)
{
GtkWidget *label1, *combo_muxfmt, *combo_decoderbuffer, *combo_streamrate;
GtkWidget *combo_sequencemb, *combo_nonvideo;
GList *streamdata = NULL;
GList *decoderbuffer = NULL;
GList *sequence_mb = NULL;
GList *nonvideorate = NULL;

  decoderbuffer = g_list_append (decoderbuffer, "20");
  decoderbuffer = g_list_append (decoderbuffer, "46");
  decoderbuffer = g_list_append (decoderbuffer, "100");
  decoderbuffer = g_list_append (decoderbuffer, "200");
  decoderbuffer = g_list_append (decoderbuffer, "1000");

  streamdata = g_list_append (streamdata, "auto");
  streamdata = g_list_append (streamdata, "1412");
  streamdata = g_list_append (streamdata, "1770");
  streamdata = g_list_append (streamdata, "2280");
  streamdata = g_list_append (streamdata, "2780");

  sequence_mb = g_list_append (sequence_mb, "disabled");
  sequence_mb = g_list_append (sequence_mb, "640");
  sequence_mb = g_list_append (sequence_mb, "700");

  nonvideorate = g_list_append (nonvideorate, "disabled");
  nonvideorate = g_list_append (nonvideorate, "138");
  nonvideorate = g_list_append (nonvideorate, "170");
  nonvideorate = g_list_append (nonvideorate, "238");

  label1 = gtk_label_new ("  Mux format: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);

  combo_muxfmt = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_muxfmt), muxformat);
  combo_entry_muxfmt = GTK_COMBO (combo_muxfmt)->entry;
  gtk_widget_set_usize (combo_muxfmt, 120, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_muxfmt), "changed",
                      GTK_SIGNAL_FUNC (set_mplex_muxfmt), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_muxfmt, *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_muxfmt);
  (*ty)++;

  label1 = gtk_label_new ("  Decoder buffer size: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);

  combo_decoderbuffer = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_decoderbuffer), decoderbuffer);
  combo_entry_decoderbuffer = GTK_COMBO (combo_decoderbuffer)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_decoderbuffer), "changed",
                      GTK_SIGNAL_FUNC (set_decoderbuffer), NULL);
  gtk_widget_set_usize (combo_decoderbuffer, 60, -2);
  gtk_table_attach_defaults (GTK_TABLE(table), 
                             combo_decoderbuffer, *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_decoderbuffer);
  (*ty)++;

  label1 = gtk_label_new ("  Datarate of streams: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);

  combo_streamrate = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_streamrate), streamdata);
  combo_entry_streamrate = GTK_COMBO (combo_streamrate)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_streamrate), "changed",
                      GTK_SIGNAL_FUNC (set_datarate), NULL);
  gtk_widget_set_usize (combo_streamrate, 60, -2);
  gtk_table_attach_defaults (GTK_TABLE(table), 
                             combo_streamrate, *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_streamrate);
  (*ty)++;

  switch_vbr = gtk_check_button_new_with_label (" Mux VBR ");
  gtk_widget_ref (switch_vbr);
  gtk_signal_connect (GTK_OBJECT (switch_vbr), "toggled",
                      GTK_SIGNAL_FUNC (set_vbr), NULL);
  gtk_table_attach_defaults (GTK_TABLE(table), switch_vbr, *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (switch_vbr);
  (*ty)++;

  label1 = gtk_label_new ("  Sequence every MB: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);

  combo_sequencemb = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_sequencemb), sequence_mb);
  combo_entry_sequencemb = GTK_COMBO (combo_sequencemb)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_sequencemb), "changed",
                      GTK_SIGNAL_FUNC (set_sequencesize), NULL);
  gtk_widget_set_usize (combo_sequencemb, 60, -2);
  gtk_table_attach_defaults (GTK_TABLE(table), 
                             combo_sequencemb, *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_sequencemb);
  (*ty)++;

  label1 = gtk_label_new ("  Non video data bitrate: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);

  combo_nonvideo = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_nonvideo), nonvideorate);
  combo_entry_nonvideo = GTK_COMBO (combo_nonvideo)->entry;
//  gtk_signal_connect(GTK_OBJECT(combo_entry_nonvideo), "changed",
//                      GTK_SIGNAL_FUNC (set_datarate), NULL);
  gtk_widget_set_usize (combo_nonvideo, 60, -2);
  gtk_table_attach_defaults (GTK_TABLE(table), 
                             combo_nonvideo, *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_nonvideo);
}

/* set output bitrate for the audio */
void set_audiobitrate (GtkWidget *widget, gpointer data)
{
char *test;

  test = gtk_entry_get_text(GTK_ENTRY(widget));
  tempenco.audiobitrate = atoi ( test );

  if ( (tempenco.audiobitrate < 32) || (tempenco.audiobitrate > 320))
    tempenco.audiobitrate = 224;

  if (verbose)
    printf(" selected audio bitrate: %i \n", tempenco.audiobitrate);
}

/* set smaplerate for the audio */
void set_samplebitrate (GtkWidget *widget, gpointer data)
{
gchar *test;
int i;

  i = 0;

  test = gtk_entry_get_text(GTK_ENTRY(widget));

  samples = g_list_last (samples);

  printf("Anzahl :%i, test : %s \n",g_list_length (g_list_first(samples)),test);

  for (i = (g_list_length (g_list_first(samples))) ; i > 0 ; i--)
    {
      if (strcmp (test,samples->data) == 0)
        {
          tempenco.outputbitrate = ( (atoi (samples->data)) / 100);
          break;
        }
      samples = g_list_previous (samples);
    }

  samples = g_list_first (samples);

  if (verbose)
    printf(" selected audio samplerate: %i00\n", tempenco.outputbitrate);
}

/* Set the option that should be forced */
void force_options (GtkWidget *widget, gpointer data)
{
  if ( (strcmp((char*)data,"-V") ==0) && tempenco.forcevcd[0] == ' ' )
    sprintf(tempenco.forcevcd, "%s", (char*)data);
  else
    sprintf(tempenco.forcevcd, " ");

  if ( (strcmp((char*)data,"-s") ==0) && tempenco.forcestereo[0] == ' ' )
    sprintf(tempenco.forcestereo, "%s", (char*)data);
  else
    sprintf(tempenco.forcestereo, " ");

  if ( (strcmp((char*)data,"-m") ==0) && tempenco.forcemono[0] == ' ' )
    sprintf(tempenco.forcemono, "%s", (char*)data);
  else
    sprintf(tempenco.forcemono, " ");

  if (verbose)
    printf(" in mono select: %s, vcd: %s, stereo %s, mono %s \n",
     (char*)data, tempenco.forcevcd, tempenco.forcestereo, tempenco.forcemono);
}

/* Create Layout for the sound conversation */
void create_sound_encoding (GtkWidget *table,int *tx,int *ty)
{
GtkWidget *label1, *combo_audiobit, *combo_samplerate, *button_force_no; 
GSList *group_force;
GList *abitrate = NULL;
//GList *samples = NULL;

   abitrate = g_list_append (abitrate, "224");
   abitrate = g_list_append (abitrate, "160");
   abitrate = g_list_append (abitrate, "128");
   abitrate = g_list_append (abitrate, "96");

if (!samples)
  {
    samples = g_list_append (samples, "44100");
    samples = g_list_append (samples, "48000");
    samples = g_list_append (samples, "32000");
  }

  /* Creating 1st line iwth the Bitrate selection */
  label1 = gtk_label_new ("  Bitrate: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);

  combo_audiobit = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_audiobit), abitrate);
  combo_entry_audiobitrate = GTK_COMBO (combo_audiobit)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_audiobitrate), "changed",
                      GTK_SIGNAL_FUNC (set_audiobitrate), NULL);
  gtk_widget_set_usize (combo_audiobit, 60, -2);
  gtk_table_attach_defaults (GTK_TABLE(table), combo_audiobit, *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_audiobit);
  (*ty)++;

  /* Creating 2nd line with the Samplerate selection */
  label1 = gtk_label_new ("  Sampelrate: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);

  combo_samplerate = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_samplerate), samples);
  combo_entry_samplebitrate = GTK_COMBO (combo_samplerate)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_samplebitrate), "changed",
                      GTK_SIGNAL_FUNC (set_samplebitrate), NULL);
  gtk_widget_set_usize (combo_samplerate, 80, -2);
  gtk_table_attach_defaults (GTK_TABLE(table),combo_samplerate,*tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_samplerate);
  (*ty)++;

  /* Creating the 3rd line with force sound options and the radio button
   * selection. 3 additional lines */
  label1 = gtk_label_new ("  Force Sound: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1, *tx, *tx+1, *ty, *ty+1);
  gtk_widget_show (label1);
  (*tx)++;

  button_force_no = gtk_radio_button_new_with_label (NULL, "no change");
  gtk_signal_connect (GTK_OBJECT (button_force_no), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "cl");
  gtk_table_attach_defaults (GTK_TABLE (table), button_force_no,*tx,*tx+1,*ty,*ty+1);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_no));
  gtk_widget_show (button_force_no);
  (*ty)++;

  button_force_vcd = gtk_radio_button_new_with_label(group_force, "VCD");
  gtk_signal_connect (GTK_OBJECT (button_force_vcd), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "-V");
  gtk_table_attach_defaults (GTK_TABLE (table), button_force_vcd,*tx,*tx+1,*ty,*ty+1);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_vcd));
  gtk_widget_show (button_force_vcd);
  (*ty)++;

  button_force_stereo = gtk_radio_button_new_with_label(group_force, "Stereo");
  gtk_signal_connect (GTK_OBJECT (button_force_stereo), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "-s");
  gtk_table_attach_defaults (GTK_TABLE (table), button_force_stereo, 
                                                *tx,*tx+1,*ty,*ty+1);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_stereo));  
  gtk_widget_show (button_force_stereo);
  (*ty)++;

  button_force_mono = gtk_radio_button_new_with_label(group_force, "Mono");
  gtk_signal_connect (GTK_OBJECT (button_force_mono), "toggled",
                      GTK_SIGNAL_FUNC (force_options), (gpointer) "-m");
  gtk_table_attach_defaults (GTK_TABLE (table),button_force_mono,*tx,*tx+1,*ty,*ty+1);
  group_force = gtk_radio_button_group (GTK_RADIO_BUTTON (button_force_mono));
  gtk_widget_show (button_force_mono);
  (*ty)++;
  (*tx)--;
}

/* create the lav2yuv encoding options */
void create_video_options_layout (GtkWidget *table, int *tx, int *ty)
{
GtkWidget *label1, *combo_active, *combo_noisefilter; 
GtkWidget *combo_samples, *combo_outputformat;
GList *noisefilter = NULL;
GList *droplsb = NULL;
GList *input_active_size = NULL;

   noisefilter = g_list_append (noisefilter, "0");
   noisefilter = g_list_append (noisefilter, "1");
   noisefilter = g_list_append (noisefilter, "2");

   droplsb = g_list_append (droplsb, "0");
   droplsb = g_list_append (droplsb, "1");
   droplsb = g_list_append (droplsb, "2");
   droplsb = g_list_append (droplsb, "3");

if (!outputformat)
  {
   outputformat = g_list_append (outputformat, "as is");
   outputformat = g_list_append (outputformat, "half height/width from input");
   outputformat = g_list_append (outputformat, "wide of 720 to 480 for SVCD");
   outputformat = g_list_append (outputformat, "wide of 720 to 352 for VCD");
   outputformat = g_list_append (outputformat, "720x240 to 480x480 for SVCD");
  }

   input_active_size = g_list_append (input_active_size, "as is");
   input_active_size = g_list_append (input_active_size, "348x278+2+2");
   input_active_size = g_list_append (input_active_size, "352x210+0+39");
   input_active_size = g_list_append (input_active_size, "352x168+0+60");
   input_active_size = g_list_append (input_active_size, "700x500+10+30");

  label1 = gtk_label_new ("  Noise filter: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_noisefilter = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_noisefilter), noisefilter);
  combo_entry_noisefilter = GTK_COMBO (combo_noisefilter)->entry;
  gtk_widget_set_usize (combo_noisefilter, 35, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_noisefilter), "changed",
                      GTK_SIGNAL_FUNC (set_noisefilter), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_noisefilter,
                                        *tx+1, *tx+2, *ty, *ty+1);
  gtk_widget_show (combo_noisefilter);
  (*ty)++;

  label1 = gtk_label_new ("  Drop LSB of samples: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_samples = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_samples), droplsb);
  combo_entry_samples = GTK_COMBO (combo_samples)->entry;
  gtk_widget_set_usize (combo_samples, 35, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_samples), "changed",
                      GTK_SIGNAL_FUNC (set_drop_samples), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_samples,
                                        *tx+1, *tx+2, *ty, *ty+1);
  gtk_widget_show (combo_samples);
  (*ty)++;

  label1 = gtk_label_new ("  Set active window: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_active = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_active), input_active_size);
  combo_entry_active = GTK_COMBO (combo_active)->entry;
  gtk_widget_set_usize (combo_active, 200, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_active), "changed",
    GTK_SIGNAL_FUNC (set_activewindow), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_active,
                                        *tx+1, *tx+2, *ty, *ty+1);
  gtk_widget_show (combo_active);
  (*ty)++;

  label1 = gtk_label_new ("  Lav2yuv output format: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_outputformat = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_outputformat), outputformat);
  combo_entry_outputformat = GTK_COMBO (combo_outputformat)->entry;
  gtk_widget_set_usize (combo_outputformat, 200, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_outputformat), "changed",
                      GTK_SIGNAL_FUNC (set_outputformat), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),combo_outputformat,
                                        *tx+1, *tx+2, *ty, *ty+1);
  gtk_widget_show (combo_outputformat);
  (*ty)++;
}

/* create the layout for yuvscaler programm */
void create_yuvscaler_layout (GtkWidget *table, int *tx, int *ty)
{
GtkWidget *label1, *combo_scaler_input,*combo_scaler_output, *combo_scaler_mode;
GList *input_window = NULL;
GList *yuvscalermode = NULL;
GList *output_window = NULL;

  input_window = g_list_append (input_window, "as is");
  input_window = g_list_append (input_window, "352x288+208+144");
  input_window = g_list_append (input_window, "352x240+144+120");

  yuvscalermode = g_list_append (yuvscalermode, "as is");
  yuvscalermode = g_list_append (yuvscalermode, "WIDE2STD");
  yuvscalermode = g_list_append (yuvscalermode, "WIDE2VCD");
  yuvscalermode = g_list_append (yuvscalermode, "LINE_SWITCH");
  yuvscalermode = g_list_append (yuvscalermode, "FAST_WIDE2VCD");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_388_352_1_1");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_1_1_288_200");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_2_1_2_1");
  yuvscalermode = g_list_append (yuvscalermode, "RATIO_352_320_288_200");

  output_window = g_list_append (output_window, "as is");
  output_window = g_list_append (output_window, "VCD");
  output_window = g_list_append (output_window, "SVCD");
  output_window = g_list_append (output_window, "352x240");
//  Removed because not needed any more. Wait for an answer from Xavier 
//  output_window = g_list_append (output_window, "INTERLACED_ODD_FIRST");
//  output_window = g_list_append (output_window, "INTERLACED_EVEN_FIRST");

  label1 = gtk_label_new("  Input window: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_scaler_input = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_scaler_input), input_window);
  combo_entry_scalerinput = GTK_COMBO (combo_scaler_input)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_scalerinput), "changed",
    GTK_SIGNAL_FUNC (set_scalerstrings), "SCALERINPUT" );
  gtk_widget_set_usize (combo_scaler_input, 130, -2 );
  gtk_table_attach_defaults (GTK_TABLE (table), combo_scaler_input,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_scaler_input);
  (*ty)++;

  label1 = gtk_label_new("  Scaling mode: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_scaler_mode = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_scaler_mode), yuvscalermode);
  combo_entry_scalermode = GTK_COMBO (combo_scaler_mode)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_scalermode), "changed",
    GTK_SIGNAL_FUNC (set_scalerstrings), "SCALERMODE" );
  gtk_widget_set_usize (combo_scaler_mode, 130, -2);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_scaler_mode,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show(combo_scaler_mode);
  (*ty)++;

  label1 = gtk_label_new("  Output window: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_scaler_output = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_scaler_output),
    output_window);
  combo_entry_scaleroutput = GTK_COMBO (combo_scaler_output)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_scaleroutput), "changed",
    GTK_SIGNAL_FUNC (set_scalerstrings), "SCALEROUTPUT");
  gtk_widget_set_usize (combo_scaler_output, 80, -2 );
  gtk_table_attach_defaults (GTK_TABLE (table), combo_scaler_output,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_scaler_output);
  (*ty)++;
}

/* create the video conversation  the mpeg2enc options */
void create_video_layout (GtkWidget *table, int *tx, int *ty)
{
GtkWidget *label1, *combo_videobit, *combo_qualityfa, *combo_searchradius; 
GtkWidget *combo_minGop, *combo_maxGop;
GList *vbitrate = NULL;
GList *searchrad = NULL;
GList *qualityfactors = NULL;
GList *minGop = NULL;
GList *maxGop = NULL;

   vbitrate = g_list_append (vbitrate, "disabled");
   vbitrate = g_list_append (vbitrate, "1152");
   vbitrate = g_list_append (vbitrate, "1300");
   vbitrate = g_list_append (vbitrate, "1500");
   vbitrate = g_list_append (vbitrate, "1800");
   vbitrate = g_list_append (vbitrate, "2000");
   vbitrate = g_list_append (vbitrate, "2500");

   searchrad = g_list_append (searchrad, "0");
   searchrad = g_list_append (searchrad, "8");
   searchrad = g_list_append (searchrad, "16");
   searchrad = g_list_append (searchrad, "24");
   searchrad = g_list_append (searchrad, "32");

   qualityfactors = g_list_append (qualityfactors, "disabled");
   qualityfactors = g_list_append (qualityfactors, "1");
   qualityfactors = g_list_append (qualityfactors, "7");
   qualityfactors = g_list_append (qualityfactors, "9");
   qualityfactors = g_list_append (qualityfactors, "15");
   qualityfactors = g_list_append (qualityfactors, "31");

   minGop = g_list_append (minGop, "6");
   minGop = g_list_append (minGop, "default 12");
   minGop = g_list_append (minGop, "18");
   minGop = g_list_append (minGop, "28");

   maxGop = g_list_append (maxGop, "6");
   maxGop = g_list_append (maxGop, "default 12");
   maxGop = g_list_append (maxGop, "18");
   maxGop = g_list_append (maxGop, "28");

  label1 = gtk_label_new ("  Bitrate: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_videobit = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_videobit), vbitrate);
  combo_entry_videobitrate = GTK_COMBO (combo_videobit)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_videobitrate), "changed",
                      GTK_SIGNAL_FUNC (set_videobit), NULL);
  gtk_widget_set_usize (combo_videobit, 80, -2 );
  gtk_table_attach_defaults (GTK_TABLE (table), combo_videobit,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_videobit);
  (*ty)++;

  label1 = gtk_label_new ("  Quality factor: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1); 
 
  combo_qualityfa = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_qualityfa), qualityfactors);
  combo_entry_qualityfa = GTK_COMBO (combo_qualityfa)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_qualityfa), "changed",
                      GTK_SIGNAL_FUNC (set_qualityfa), NULL);
  gtk_widget_set_usize (combo_qualityfa, 80,-2);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_qualityfa,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_qualityfa);
  (*ty)++;

  label1 = gtk_label_new ("  Searchradius: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_searchradius = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_searchradius), searchrad);
  combo_entry_searchradius = GTK_COMBO (combo_searchradius)->entry;
  gtk_widget_set_usize (combo_searchradius, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_searchradius), "changed",
                      GTK_SIGNAL_FUNC (set_searchrad), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_searchradius,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_searchradius);
  (*ty)++;

  label1 = gtk_label_new ("  Minimal GOP: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_minGop = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_minGop), minGop);
  combo_entry_minGop = GTK_COMBO (combo_minGop)->entry;
  gtk_widget_set_usize (combo_minGop, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_minGop), "changed",
                      GTK_SIGNAL_FUNC (set_minGop), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_minGop,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_minGop);
  (*ty)++;

  label1 = gtk_label_new ("  Maximal GOP: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_maxGop = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_maxGop), maxGop);
  combo_entry_maxGop = GTK_COMBO (combo_maxGop)->entry;
  gtk_widget_set_usize (combo_maxGop, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_maxGop), "changed",
                      GTK_SIGNAL_FUNC (set_maxGop), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_maxGop,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_maxGop);
  (*ty)++;

}


/* Create the audio and mplex layout */
void create_audio_mplex_layout(GtkWidget *hbox)
{
GtkWidget *label, *table;
int tx,ty; /* tabele size x, y */

tx = 2;
ty = 10;

  table = gtk_table_new (tx,ty,FALSE);
  tx = 0;
  ty = 0;

  label = gtk_label_new (" Sound option: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;
  
  create_sound_encoding (table, &tx, &ty);

  label = gtk_label_new (" Mplex option: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  create_mplex_encoding (table, &tx, &ty);

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show(table);
}

/* Create the video encoding layout */
void create_yuv2mpeg_layout(GtkWidget *hbox)
{
GtkWidget *label, *table;
int tx,ty; /* tabele size x, y */

tx = 2;
ty = 9;

  table = gtk_table_new (tx,ty,FALSE);
  tx = 0;
  ty = 0;

  label = gtk_label_new (" Video option: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  create_video_options_layout(table, &tx, &ty);

  create_yuvscaler_layout (table, &tx, &ty);

  create_video_layout (table, &tx, &ty);

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show(table);
}

/* This is done when the OK Button was pressed */
void accept_mpegoptions(GtkWidget *widget, gpointer data)
{
  sprintf((*point).notblacksize,"%s",tempenco.notblacksize);
  sprintf((*point).input_use,"%s",tempenco.input_use);
  sprintf((*point).output_size,"%s",tempenco.output_size);
  sprintf((*point).mode_keyword,"%s",tempenco.mode_keyword);
  sprintf((*point).ininterlace_type,"%s",tempenco.ininterlace_type);
  (*point).addoutputnorm=tempenco.addoutputnorm;
  (*point).outputformat=tempenco.outputformat;
  (*point).droplsb=tempenco.droplsb;
  (*point).noisefilter=tempenco.noisefilter;
  (*point).audiobitrate=tempenco.audiobitrate;
  (*point).outputbitrate=tempenco.outputbitrate;
  sprintf((*point).forcestereo,"%s",tempenco.forcestereo);
  sprintf((*point).forcemono,"%s",tempenco.forcemono);
  sprintf((*point).forcevcd,"%s",tempenco.forcevcd);
  (*point).bitrate=tempenco.bitrate;
  (*point).qualityfactor=tempenco.qualityfactor;
  (*point).minGop=tempenco.minGop;
  (*point).maxGop=tempenco.maxGop;
  (*point).sequencesize=tempenco.sequencesize;
  (*point).nonvideorate=tempenco.nonvideorate;
  (*point).searchradius=tempenco.searchradius;
  (*point).muxformat=tempenco.muxformat;
  sprintf((*point).muxvbr,"%s",tempenco.muxvbr);
  (*point).streamdatarate=tempenco.streamdatarate;
  (*point).decoderbuffer=tempenco.decoderbuffer;
}

/* open a new window with all the options in it */
void open_mpeg_window(GtkWidget *widget, gpointer data)
{
GtkWidget *options_window, *button;
GtkWidget *hbox,*vbox;

if (g_list_length (muxformat) == 0)
  {
    muxformat = g_list_append (muxformat, "Auto MPEG 1");
    muxformat = g_list_append (muxformat, "standard VCD");
    muxformat = g_list_append (muxformat, "user-rate VCD");
    muxformat = g_list_append (muxformat, "Auto MPEG 2");
    muxformat = g_list_append (muxformat, "standard SVCD");
    muxformat = g_list_append (muxformat, "user-rate SVCD");
    muxformat = g_list_append (muxformat, "DVD");
  } 

  printf("\n laenge %i \n", g_list_length (g_list_first(muxformat)));
  printf("\n laenge samples %i \n", g_list_length (g_list_first(samples)));
//  printf("\n laenge outputformat %i \n", g_list_length (g_list_first(outputformat)));
 

 init_tempenco(data);

  options_window = gtk_window_new(GTK_WINDOW_DIALOG);
  hbox = gtk_hbox_new (FALSE, 10);
  vbox = gtk_vbox_new (FALSE, 10);

  /* Left table for the audio and mplex options */
  create_audio_mplex_layout(hbox);

 /* Right table for the video options */
  create_yuv2mpeg_layout(hbox);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  /* Show the OK and cancel Button */

  hbox = gtk_hbox_new(TRUE, 20);

  button = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC (accept_mpegoptions), (gpointer) "test");
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

  show_data(data);
  
}
