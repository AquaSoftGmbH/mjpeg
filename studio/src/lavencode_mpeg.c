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
 * also mpeg1, mpeg2, vcd, svcd, in the future maybe: DVD 
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
#include <sys/stat.h>

#include "studio.h"
#include "gtkfunctions.h"

/* Forward declarations */
void open_mpeg_window(GtkWidget *widget, gpointer data);
void accept_mpegoptions(GtkWidget *widget, gpointer data);
void create_audio_mplex_layout(GtkWidget *hbox);
void create_yuv2lav_options(GtkWidget *hbox);
void create_yuv2mpeg_layout(GtkWidget *hbox);
void create_sound_encoding (GtkWidget *table, int *tx,int *ty);
void create_mplex_encoding (GtkWidget *table, int *tx, int *ty);
void create_denoise_layout (GtkWidget *table, int *tx, int *ty);
void create_video_options_layout (GtkWidget *table, int *tx, int *ty);
void create_yuvscaler_layout (GtkWidget *table, int *tx, int *ty);
void create_video_layout (GtkWidget *table, int *tx, int *ty);
void create_yuv2lav_layout (GtkWidget *table, int *tx, int *ty);
void set_mplex_muxfmt (GtkWidget *widget, gpointer data);
void set_audiobitrate (GtkWidget *widget, gpointer data);
void set_samplebitrate (GtkWidget *widget, gpointer data);
void force_options (GtkWidget *widget, gpointer data);
void set_drop_samples (GtkWidget *widget, gpointer data);
void set_activewindow (GtkWidget *widget, gpointer data);
void set_scalerstrings (GtkWidget *widget, gpointer data);
void set_videobit (GtkWidget *widget, gpointer data);
void set_searchrad (GtkWidget *widget, gpointer data);
void set_format (GtkWidget *widget, gpointer data);
void set_quality (GtkWidget *widget, gpointer data);
void set_maxfilesize (GtkWidget *widget, gpointer data);
void init_tempenco(gpointer task);
void show_data_audiomp2(gpointer task);
void show_data_lav2yuv(gpointer task);
void show_data_yuvtools(gpointer task);
void show_data_mpeg2enc(gpointer task);
void show_data_mplex(gpointer task);
void show_data_yuv2lav(gpointer task);
void set_decoderbuffer(GtkWidget *widget, gpointer data);
void set_datarate (GtkWidget *widget, gpointer data);
void set_vbr (GtkWidget *widget, gpointer data);
void set_qualityfa (GtkWidget *widget, gpointer data);
void set_minGop (GtkWidget *widget, gpointer data);
void set_maxGop (GtkWidget *widget, gpointer data);
void set_sequencesize (GtkWidget *widget, gpointer data);
void set_nonvideorate (GtkWidget *widget, gpointer data);
void update_vbr(void);
void set_interlacing (GtkWidget *widget, gpointer data);
void set_use_yuvdenoise(GtkWidget *widget, gpointer data);
void set_use_deinterlace(GtkWidget *widget, gpointer data);
void set_2lav_interlacing (GtkWidget *widget, gpointer data);
void set_sharpen (GtkWidget *widget, gpointer data);
void set_thhold (GtkWidget *widget, gpointer data);
void set_average (GtkWidget *widget, gpointer data);
void input_select (GtkWidget *widget, gpointer data);

/* Some variables */
GList *samples = NULL;            /**< holds the possible audio sample rates */
GList *muxformat = NULL;
GList *streamdata = NULL;
GList *interlace_correct = NULL;
GList *yuv2lav_interlace = NULL;
GList *yuv2lav_format = NULL;
struct encodingoptions tempenco;
struct encodingoptions *point;    /* points to the encoding struct to change */
int changed_streamdatarate;  /* shows if the rate was updated into the Glist */

GtkWidget *combo_entry_scaleroutput, *combo_entry_scalermode;
GtkWidget *combo_entry_samples, *combo_entry_audiobitrate;
GtkWidget *combo_entry_samplebitrate, *button_force_stereo, *button_force_mono;
GtkWidget *button_force_vcd, *combo_entry_searchradius, *combo_entry_muxfmt;
GtkWidget *combo_entry_videobitrate, *combo_entry_decoderbuffer, *switch_vbr; 
GtkWidget *combo_entry_streamrate, *combo_entry_qualityfa, *combo_entry_minGop;
GtkWidget *combo_entry_maxGop, *combo_entry_sequencemb, *combo_entry_nonvideo;
GtkWidget *combo_streamrate, *combo_entry_interlacecorr, *switch_yuvdenoise;
GtkWidget *combo_entry_quality; 
GtkWidget *combo_entry_format,*combo_entry_maxfilesize;
GtkWidget *combo_entry_interlace, *switch_deinterlace, *combo_entry_sharp;
GtkWidget *combo_entry_thhold, *combo_entry_average;

GtkWidget *combo_samplerate;

/* some constant values */
#define thhold_def "5, default"
#define average_def "3, default"

/* =============================================================== */
/* Start of the code */

/** Set the tempenco struct to defined values, the struct
    holds the temporal values */
void init_tempenco(gpointer task)
{
	sprintf(tempenco.notblacksize,"%s",(*point).notblacksize);
	sprintf(tempenco.input_use,"%s",(*point).input_use);
	sprintf(tempenco.output_size,"%s",(*point).output_size);
	sprintf(tempenco.mode_keyword,"%s",(*point).mode_keyword);
	tempenco.addoutputnorm=(*point).addoutputnorm;
	sprintf(tempenco.interlacecorr,"%s",(*point).interlacecorr);
	tempenco.audiobitrate=(*point).audiobitrate;
	tempenco.outputbitrate=(*point).outputbitrate;
	sprintf(tempenco.forcestereo,"%s",(*point).forcestereo);
	sprintf(tempenco.forcemono,"%s",(*point).forcemono);
	sprintf(tempenco.forcevcd,"%s",(*point).forcevcd);
	tempenco.use_yuvdenoise=(*point).use_yuvdenoise;
	tempenco.deinterlace=(*point).deinterlace;
	tempenco.sharpness=(*point).sharpness;
	tempenco.denois_thhold=(*point).denois_thhold;
	tempenco.average_frames=(*point).average_frames;
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
	sprintf(tempenco.codec,"%s",(*point).codec);
}

/* setting the values of the GTK_ENTRY's for the audio options */
void show_data_audiomp2(gpointer task)
{
	char val[LONGOPT];

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
}

/* setting the values of the GTK_ENTRY's for the lav2yuv options */
void show_data_lav2yuv(gpointer task)
{
  printf(" notback \"%s\"\n",tempenco.notblacksize);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_active), tempenco.notblacksize);
  gtk_signal_connect(GTK_OBJECT(combo_entry_active), "changed",
    GTK_SIGNAL_FUNC (set_activewindow), NULL);
}

/* setting the values of the GTK_ENTRY's for the yuvtools options */
void show_data_yuvtools(gpointer task)
{
char val[LONGOPT];

  /* yuvscaler options */
  gtk_entry_set_text(GTK_ENTRY(combo_entry_scalerinput),tempenco.input_use);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_scaleroutput), tempenco.output_size);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_scalermode), tempenco.mode_keyword);

  /* interlacing correction */
  gtk_entry_set_text(GTK_ENTRY(combo_entry_interlacecorr) ,tempenco.interlacecorr);
  /* denoise options */
  if (tempenco.use_yuvdenoise == 1 )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(switch_yuvdenoise),TRUE);

  if (tempenco.deinterlace == 1 )
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(switch_deinterlace),TRUE);

  if (tempenco.sharpness == 125)
    gtk_entry_set_text(GTK_ENTRY(combo_entry_sharp), "125, default");
  else if (tempenco.sharpness == 0)
    gtk_entry_set_text(GTK_ENTRY(combo_entry_sharp), "0, disabled");
  else
    {
      sprintf(val,"%i",tempenco.sharpness);
      gtk_entry_set_text(GTK_ENTRY(combo_entry_sharp), val);
    }
 
 if (tempenco.denois_thhold == 5)
   gtk_entry_set_text(GTK_ENTRY(combo_entry_thhold), thhold_def);
 else 
   {
     sprintf(val,"%i", tempenco.denois_thhold);
     gtk_entry_set_text(GTK_ENTRY(combo_entry_thhold), val);
   } 

 if (tempenco.average_frames == 3)
   gtk_entry_set_text(GTK_ENTRY(combo_entry_average), average_def);
 else
   {
     sprintf(val,"%i", tempenco.average_frames);
     gtk_entry_set_text(GTK_ENTRY(combo_entry_average), val);
   }

}

/* setting the values of the GTK_ENTRY's for the mpeg2enc options */
void show_data_mpeg2enc(gpointer task)
{
char val[LONGOPT];

  if (tempenco.bitrate == 0)
    sprintf(val,"default");
  else 
    sprintf(val,"%i",tempenco.bitrate);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_videobitrate),val);

  if (tempenco.qualityfactor != 0)
    sprintf(val,"%i",tempenco.qualityfactor);
  else
    sprintf(val,"disabled");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_qualityfa),val);

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
}

/* setting the values of the GTK_ENTRY's for the mplex options */
void show_data_mplex(gpointer task)
{
int i;
char val[LONGOPT];

  muxformat = g_list_first (muxformat);
  for (i = 0; i < tempenco.muxformat ;i++)
    muxformat = g_list_next (muxformat);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_muxfmt), muxformat->data);

  if (tempenco.muxvbr[0] == '-')
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(switch_vbr),TRUE);

  sprintf(val,"%i",tempenco.decoderbuffer);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_decoderbuffer), val);

if (tempenco.sequencesize != 0)
  {
    sprintf(val,"%i",tempenco.sequencesize);
    gtk_entry_set_text(GTK_ENTRY(combo_entry_sequencemb), val);
  }

if (tempenco.nonvideorate != 0)
  {
    sprintf(val,"%i",tempenco.nonvideorate);
    gtk_entry_set_text(GTK_ENTRY(combo_entry_nonvideo), val);
  }

  if ( tempenco.streamdatarate == 0)
    sprintf(val,"from streams");
  else
    sprintf(val,"%i",tempenco.streamdatarate);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_streamrate), val);
}

/* setting the values of the GTK_ENTRY's for the lav2yuv options */
void show_data_yuv2lav(gpointer task)
{
char val[LONGOPT];
int i;

i=0;

  gtk_entry_set_text(GTK_ENTRY(combo_entry_format),tempenco.codec);

  if (tempenco.qualityfactor != 80)
    sprintf(val,"%i",tempenco.qualityfactor);
  else
    sprintf(val,"default");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_quality),val);

  if (tempenco.sequencesize != 0)
    sprintf(val,"%i",tempenco.sequencesize);
  else
    sprintf(val,"default");
  gtk_entry_set_text(GTK_ENTRY(combo_entry_maxfilesize),val);

  if (tempenco.minGop == 3) /* strange value calcualtion because I'd want */
    tempenco.minGop=0;           /* that the value is the same as for the */
  else                                     /* programm, and the ignore is 3. */
    tempenco.minGop++;                           /* Didn't want to use -1 */
  
  yuv2lav_interlace = g_list_first (yuv2lav_interlace);
  for (i = 0; i < tempenco.minGop ;i++)
    yuv2lav_interlace = g_list_next (yuv2lav_interlace);
  gtk_entry_set_text(GTK_ENTRY(combo_entry_interlace), yuv2lav_interlace->data);

}

/* Set the string for the lav2yuv active window */
void set_activewindow (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;

  i=0;

  test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

  strncpy(tempenco.notblacksize,test,LONGOPT);

  if (verbose)
    printf (" Set activ window to : %s\n", tempenco.notblacksize);
}

/** Set the string fo the correction of the interlacing type */
void set_interlacing (GtkWidget *widget, gpointer data)
{
  char *test;
  int i;

  i=0;

  test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

  interlace_correct = g_list_last (interlace_correct);

  for (i = (g_list_length (g_list_first(interlace_correct))-1) ; i > 0 ; i--)
    {
      if (strcmp (test,interlace_correct->data) == 0)
        {
           strcpy(tempenco.interlacecorr,interlace_correct->data);
           break;
        }
 
      interlace_correct = g_list_previous (interlace_correct);
    }

  interlace_correct = g_list_first (interlace_correct);

  if (verbose)
    printf (" Set Interlacing Correction to : %s \n", tempenco.interlacecorr);
}

/* set the decoder buffer size for mplex and mpeg2enc */
void set_decoderbuffer(GtkWidget *widget, gpointer data)
{
char *test;
int i;

  i = 0;

  test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

  i = atoi ( test );

  if ( (i >= 20) && (i <= 1000) )
    tempenco.decoderbuffer = i;

  if (verbose)
    printf(" Set decoder buffer to : %i kB\n", tempenco.decoderbuffer);
}

/* And now we check vor a usefull value in Datarate of streams */
/* if non checked we calcualte one out of video and audio and  */
/* insert it into the list */
void update_vbr()
{
int audio_rate, video_rate, mplex_rate, temprate;
char val[LONGOPT];
audio_rate=0;
video_rate=0;
mplex_rate=0;

temprate = tempenco.streamdatarate; /* here we save the old .streamdatarate */
     /* because the value gets lost when updating the combo_popdown_strings */

if (strcmp(tempenco.forcevcd,"-V") == 0 )
  audio_rate=224;
else
  audio_rate=tempenco.audiobitrate;

if (tempenco.bitrate == 0)
  video_rate=2500;
else
  video_rate=tempenco.bitrate;

mplex_rate=(video_rate+audio_rate)*1.017;
sprintf(val,"%i",mplex_rate);

if (strcmp(tempenco.muxvbr,"-V") == 0 )
  {
    if (changed_streamdatarate)
      streamdata = g_list_remove(streamdata,(g_list_last(streamdata))->data);
    else
      changed_streamdatarate=1;
    
    streamdata = g_list_append (streamdata, val);
    streamdata = g_list_first (streamdata);
    gtk_combo_set_popdown_strings( GTK_COMBO(combo_streamrate), streamdata);

    streamdata = g_list_first (streamdata);

    if ( temprate == 0)
      sprintf(val,"from streams");
    else
      sprintf(val,"%i",temprate);
    gtk_entry_set_text(GTK_ENTRY(combo_entry_streamrate), val);
  }
}

/* set the vbr flag for the multiplexer */
void set_vbr(GtkWidget *widget, gpointer data)
{
int i;

  if (GTK_TOGGLE_BUTTON (widget)->active)
    {
       sprintf(tempenco.muxvbr,"-V");
       update_vbr();
    }
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

  test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

  if(strcmp(test,"from streams") != 0)
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

  test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

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

  test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

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
  test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));


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

/** Callback Set the string for the yuvscaler options */
void set_scalerstrings (GtkWidget *widget, gpointer data)
{
char *test;
int i;

i=0;

test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));


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
  if (strlen(test) > 0)
    sprintf(tempenco.output_size,"%s", test);

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

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	if ( strcmp(test, "default") == 0 )
		tempenco.bitrate = 0;
	else
		tempenco.bitrate = atoi ( test );

	update_vbr();

	if (verbose)
		printf(" selected video bitrate: %i \n", tempenco.bitrate);
}

/* Set the quality factor for the stream VBR */
void set_qualityfa (GtkWidget *widget, gpointer data)
{
	char *test;
	int i;
	i=0;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

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

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

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

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

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

/* set the search radius for the mpegencoder */
void set_searchrad (GtkWidget *widget, gpointer data)
{
	char *test;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	tempenco.searchradius = atoi ( test );

	if (verbose)
		printf(" selected searchradius : %i \n", tempenco.searchradius);
}

/* creating the options for mplex */
void create_mplex_encoding (GtkWidget *table, int *tx, int *ty)
{
	GtkWidget *label1, *combo_muxfmt, *combo_decoderbuffer; 
	/* , *combo_streamrate; */
	GtkWidget *combo_sequencemb, *combo_nonvideo;
	GList *decoderbuffer = NULL;
	GList *sequence_mb = NULL;
	GList *nonvideorate = NULL;

	decoderbuffer = g_list_append (decoderbuffer, "20");
	decoderbuffer = g_list_append (decoderbuffer, "46");
	decoderbuffer = g_list_append (decoderbuffer, "100");
	decoderbuffer = g_list_append (decoderbuffer, "200");
	decoderbuffer = g_list_append (decoderbuffer, "1000");

	sequence_mb = g_list_append (sequence_mb, "disabled");
	sequence_mb = g_list_append (sequence_mb, "640");
	sequence_mb = g_list_append (sequence_mb, "700");

	nonvideorate = g_list_append (nonvideorate, "disabled");
	nonvideorate = g_list_append (nonvideorate, "138");
	nonvideorate = g_list_append (nonvideorate, "170");
	nonvideorate = g_list_append (nonvideorate, "238");

	label1 = gtk_label_new ("  Video / Mux format: ");
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
	gtk_table_attach_defaults (GTK_TABLE(table),switch_vbr,*tx+1,*tx+2,*ty,*ty+1);
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
	gtk_signal_connect(GTK_OBJECT(combo_entry_nonvideo), "changed",
			GTK_SIGNAL_FUNC (set_nonvideorate), NULL);
	gtk_widget_set_usize (combo_nonvideo, 60, -2);
	gtk_table_attach_defaults (GTK_TABLE(table), 
			combo_nonvideo, *tx+1,*tx+2,*ty,*ty+1);
	gtk_widget_show (combo_nonvideo);
}

/* set output bitrate for the audio */
void set_audiobitrate (GtkWidget *widget, gpointer data)
{
	char *test;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));
	tempenco.audiobitrate = atoi ( test );

	if ( (tempenco.audiobitrate < 32) || (tempenco.audiobitrate > 384))
		tempenco.audiobitrate = 224;

	update_vbr();

	if (verbose)
		printf(" selected audio bitrate: %i \n", tempenco.audiobitrate);
}

/* set samplerate for the audio */
void set_samplebitrate (GtkWidget *widget, gpointer data)
{
gchar *test;
int i;

i = 0;

test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

samples = g_list_last (samples);

for (i = (g_list_length (g_list_first(samples)) -1 ) ; i > 0 ; i--)
  {
    if (strcmp (test,samples->data) == 0)
      {
        tempenco.outputbitrate = ( (atoi (samples->data)) / 100);
        break;
      }
    else 
        tempenco.outputbitrate = 441;

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
	{
		sprintf(tempenco.forcevcd, "%s", (char*)data);
		update_vbr();         /* here the streamdarate can also be changed */
	}
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

/* Set the chosen output format for yuv2lav */
void set_format (GtkWidget *widget, gpointer data)
{ 
	gchar *test;
	int i;

	i = 0;
	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	yuv2lav_format = g_list_last (yuv2lav_format);

	for (i = (g_list_length (g_list_first(yuv2lav_format))-1) ; i > 0 ; i--)
	{ 
		if (strcmp (test,yuv2lav_format->data) == 0)
			break;

		yuv2lav_format = g_list_previous (yuv2lav_format);
	}

	strncpy(tempenco.codec,yuv2lav_format->data,LONGOPT);   

	yuv2lav_format = g_list_first (yuv2lav_format);

	if (verbose)
		printf(" selected output format: %s \n", tempenco.codec);
}   

/* Set the quality of the picutres */
void set_quality (GtkWidget *widget, gpointer data)
{
	char *test;
	int i;
	i=0;

	test = (char*)gtk_entry_get_text(GTK_ENTRY(widget));

	if ( strncmp(test, "default", 7) == 0)
		tempenco.qualityfactor = 80;
	else
	{
		i = atoi (test);
		if ( (i <= 100) && (i >= 1) )
			tempenco.qualityfactor = i;
	}

	if (verbose)
		printf(" selected quality factor : %i \n", tempenco.qualityfactor);
}

/* Set the maximal filesize of output mjpeg file */
void set_maxfilesize (GtkWidget *widget, gpointer data)
{
	char *test;
	int i;
	i=0;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	if ( strncmp(test, "default", 7) == 0)
		tempenco.sequencesize = 0;
	else
	{
		i = atoi (test);
		if ( (i <= 60000) && (i >= 1) )
			tempenco.sequencesize = i;
	}

	if (verbose)
		printf(" selected output filesize : %i \n", tempenco.sequencesize);
}

/* Set the interlacing for the output mjpeg file */
void set_2lav_interlacing (GtkWidget *widget, gpointer data)
{
	char *test;
	int i;
	i=0;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	yuv2lav_interlace = g_list_last (yuv2lav_interlace);

	for (i = (g_list_length (g_list_first(yuv2lav_interlace))-1) ; i > 0 ; i--)
	{
		if (strcmp (test,yuv2lav_interlace->data) == 0)
			break;

		yuv2lav_interlace = g_list_previous (yuv2lav_interlace);
	}

	if (i == 0)
		tempenco.minGop = 3;
	else 
		tempenco.minGop = --i;

	yuv2lav_interlace = g_list_first (yuv2lav_interlace);

	if (verbose)
		printf(" selected multiplexing format: %i \n", tempenco.minGop);
} 

/* Create Layout for the sound conversation */
void create_sound_encoding (GtkWidget *table,int *tx,int *ty)
{
	GtkWidget *label1, *combo_audiobit, *button_force_no; 
	GSList *group_force;
	GList *abitrate = NULL;

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
//	g_signal_connect (G_OBJECT( GTK_COMBO(combo_samplerate)->entry), "activate",
//	  G_CALLBACK (set_samplebitrate), NULL);
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

/** set the yuvdenoise flag for the Encoding */
void set_use_yuvdenoise(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		tempenco.use_yuvdenoise=1;
	else
		tempenco.use_yuvdenoise=0;

	if (verbose)
		printf(" set the use of yuvdenoise to : %i \n",tempenco.use_yuvdenoise);
}

/** set the deinterlace flag for the encoding 
  @param widget there we get the data
  @param data   unused                         */
void set_use_deinterlace(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		tempenco.deinterlace=1;
	else
		tempenco.deinterlace=0;

	if (verbose)
		printf(" set the use of the deinterlacer to : %i \n",tempenco.deinterlace);
}

/** Set the value for the yuvdenoise sharpen option
  @param widget there we get the data
  @param data   unused                         */
void set_sharpen (GtkWidget *widget, gpointer data)
{
	char *test;
	int i;

	i=0;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	if (strcmp(test,"125, default") == 0)
		tempenco.sharpness = 125;
	else if (strcmp(test,"0, disabled") == 0)
		tempenco.sharpness = 0;
	else
	{
		i = atoi (test);
		if ( (i <= 255) && (i >= 1) )
			tempenco.sharpness = i;
	}

	if (verbose)
		printf (" Setting sharpness to : %i\n", tempenco.sharpness);
}

/** Set the value for the yuvdenoise Denoiser threshold 
  @param widget there we get the data
  @param data   unused                         */
void set_thhold (GtkWidget *widget, gpointer data)
{
	char *test;
	int i;

	i=0;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	if (strcmp(test,thhold_def) == 0)
		tempenco.denois_thhold = 5;
	else
	{
		i = atoi (test);
		if ( (i <= 255) && (i >= 0) )
			tempenco.denois_thhold = i;
	}

	if (verbose)
		printf (" Setting Denoiser threshold to : %i\n", tempenco.denois_thhold);
}

/** Set number of frames used for time-lowpassed pixel
  @param widget there we get the data
  @param data   unused                         */
void set_average (GtkWidget *widget, gpointer data)
{
	char *test;
	int i;

	i=0;

	test = (char*) gtk_entry_get_text(GTK_ENTRY(widget));

	if (strcmp(test,average_def) == 0)
		tempenco.average_frames = 3;
	else
	{
		i = atoi (test);
		if ( (i <= 255) && (i >= 0) )
			tempenco.average_frames = i;
	}

	if (verbose)
		printf (" Setting Denoiser threshold to : %i\n", tempenco.average_frames);
}

/** here we preprare the size selection
  @param widget the calling widget 
  @param the data with the contence of the data we select what we do */
void input_select (GtkWidget *widget, gpointer data)
{
	char filename[FILELEN];
	char command[200];
	FILE *fp;
	int i;
	struct stat stbuf;

	for (i= 1; i < 200; i++)
		command[i]='\0'; 

	if (strcmp((char*)data,"activewindow") == 0)
	{
		if ((fp = fopen(enc_inputfile, "r")) == NULL)
		{
			gtk_show_text_window(STUDIO_ERROR,"The editlistfile you specified does not exist. Choose a valid file in the Input file selection");
			if (verbose)
				printf("\n File does not exist \n");
		}
		else 
		{
			fclose(fp);

			sprintf(command, "lav2yuv -f 1 %s | y4mtoppm -L >%s/.studio/frame.ppm",
					enc_inputfile, getenv("HOME"));
			if (verbose)
				printf("Command creating input window pic : %s \n",command);

			fp = popen(command, "w");
			fflush(fp);
			pclose(fp);

			sprintf(filename,"%s/.studio/frame.ppm",getenv("HOME"));

			/* Here we check if the file created has a size larger than 0 bytes
			   If it has 0 bytes something went wrong with the generation of the
			   pic we show. And we show a nice message box. Else we go on. */
			stat(filename, &stbuf);
			if (stbuf.st_size == 0)
				gtk_show_text_window(STUDIO_ERROR,"Something went wrong with the creation of the image for the preview. Check the input file !!");
			else
				create_window_select(filename);
		}
	}

}

/** create the denoise encoding options now only with yuvdenoise,           
  but later maybe with: yuvkineco, yuvycsnoise, yuvmedianfilter
  @param table the table in which we insert the widgets
  @param tx    the x coodinate where start to insert the widgets
  @param ty    the y coodinate where start to insert the widgets */
void create_denoise_layout (GtkWidget *table, int *tx, int *ty)
{
	GtkWidget *label1, *combo_sharp, *combo_thhold, *combo_average;
	GList *sharp_val = NULL;
	GList *thhold = NULL;
	GList *faverag = NULL;

	sharp_val = g_list_append (sharp_val, "125, default"); 
	sharp_val = g_list_append (sharp_val, "0, disabled"); 
	sharp_val = g_list_append (sharp_val, "100"); 
	sharp_val = g_list_append (sharp_val, "110"); 
	sharp_val = g_list_append (sharp_val, "135"); 

	thhold = g_list_append (thhold, "3");
	thhold = g_list_append (thhold, thhold_def);
	thhold = g_list_append (thhold, "7");
	thhold = g_list_append (thhold, "9");

	faverag = g_list_append (faverag, "2");
	faverag = g_list_append (faverag, average_def);
	faverag = g_list_append (faverag, "5");
	faverag = g_list_append (faverag, "7");


	label1 = gtk_label_new ("  Noise reduction : ");
	gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
	gtk_widget_show (label1);

	switch_yuvdenoise = gtk_check_button_new_with_label (" use yuvdenoise");
	gtk_widget_ref (switch_yuvdenoise);
	gtk_signal_connect (GTK_OBJECT (switch_yuvdenoise), "toggled",
			GTK_SIGNAL_FUNC (set_use_yuvdenoise), NULL);
	gtk_table_attach_defaults (GTK_TABLE(table),
			switch_yuvdenoise,*tx+1,*tx+2,*ty,*ty+1);
	gtk_widget_show (switch_yuvdenoise);
	(*ty)++;

	label1 = gtk_label_new ("  Deinterlace  : ");
	gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
	gtk_widget_show (label1);

	switch_deinterlace = gtk_check_button_new_with_label ("  when denoising");
	gtk_widget_ref (switch_deinterlace);
	gtk_signal_connect (GTK_OBJECT (switch_deinterlace), "toggled",
			GTK_SIGNAL_FUNC (set_use_deinterlace), NULL);
	gtk_table_attach_defaults (GTK_TABLE(table),
			switch_deinterlace,*tx+1,*tx+2,*ty,*ty+1);
	gtk_widget_show (switch_deinterlace);
	(*ty)++;

	label1 = gtk_label_new ("  Sharpen  : ");
	gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
	gtk_widget_show (label1);

	combo_sharp = gtk_combo_new();
	gtk_combo_set_popdown_strings (GTK_COMBO (combo_sharp), sharp_val);
	combo_entry_sharp = GTK_COMBO (combo_sharp)->entry;
	gtk_widget_set_usize (combo_sharp, 200, -2 );
	gtk_signal_connect(GTK_OBJECT(combo_entry_sharp), "changed",
			GTK_SIGNAL_FUNC (set_sharpen), NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), combo_sharp,
			*tx+1, *tx+2, *ty, *ty+1);
	gtk_widget_show (combo_sharp);
	(*ty)++;

	label1 = gtk_label_new ("  Denoiser threshold : ");
	gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
	gtk_widget_show (label1);

	combo_thhold = gtk_combo_new();
	gtk_combo_set_popdown_strings (GTK_COMBO (combo_thhold), thhold);
	combo_entry_thhold = GTK_COMBO (combo_thhold)->entry;
	gtk_widget_set_usize (combo_thhold, 200, -2 );
	gtk_signal_connect(GTK_OBJECT(combo_entry_thhold), "changed",
			GTK_SIGNAL_FUNC (set_thhold), NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), combo_thhold,
			*tx+1, *tx+2, *ty, *ty+1);
	gtk_widget_show (combo_thhold);
	(*ty)++;

	label1 = gtk_label_new ("  Average frames : ");
	gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
	gtk_widget_show (label1);

	combo_average = gtk_combo_new();
	gtk_combo_set_popdown_strings (GTK_COMBO (combo_average), faverag);
	combo_entry_average = GTK_COMBO (combo_average)->entry;
	gtk_widget_set_usize (combo_average, 200, -2 );
	gtk_signal_connect(GTK_OBJECT(combo_entry_average), "changed",
			GTK_SIGNAL_FUNC (set_average), NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), combo_average,
			*tx+1, *tx+2, *ty, *ty+1);
	gtk_widget_show (combo_average);
	(*ty)++;

}

/* create the lav2yuv encoding options */
void create_video_options_layout (GtkWidget *table, int *tx, int *ty)
{
GtkWidget *label1, *combo_active, *combo_interlacecorr;
GList *input_active_size = NULL;
GtkWidget *button_input;

input_active_size = g_list_append (input_active_size, "as is");
input_active_size = g_list_append (input_active_size, "348x278+2+2");
input_active_size = g_list_append (input_active_size, "352x210+0+39");
input_active_size = g_list_append (input_active_size, "352x168+0+60");
input_active_size = g_list_append (input_active_size, "700x500+10+30");

if (!interlace_correct)
  {
  interlace_correct =g_list_append (interlace_correct, "not needed");
  interlace_correct =g_list_append (interlace_correct, "exchange fields");
  interlace_correct =g_list_append(interlace_correct,"shift bottom field forw");
  interlace_correct =g_list_append(interlace_correct,"shift top field forward");
  interlace_correct =g_list_append (interlace_correct, "interlace top first");
  interlace_correct =g_list_append (interlace_correct,"interlace bottom first");
  interlace_correct =g_list_append (interlace_correct, "not interlaced");
  }

	button_input = gtk_button_new_with_label(" Set active window: ");
	gtk_signal_connect(GTK_OBJECT(button_input), "clicked",
			GTK_SIGNAL_FUNC(input_select), "activewindow");
	gtk_table_attach_defaults (GTK_TABLE(table),button_input,*tx,*tx+1,*ty,*ty+1);
	gtk_widget_show (button_input);

	combo_active = gtk_combo_new();
	gtk_combo_set_popdown_strings (GTK_COMBO (combo_active), input_active_size);
	combo_entry_active = GTK_COMBO (combo_active)->entry;
	/* The signal connect is done after the have written the default value
	   into the combobox */
	gtk_widget_set_usize (combo_active, 200, -2 );
	gtk_table_attach_defaults (GTK_TABLE (table), combo_active,
			*tx+1, *tx+2, *ty, *ty+1);
	gtk_widget_show (combo_active);
	(*ty)++;

	label1 = gtk_label_new (NULL);
	gtk_label_set_markup(GTK_LABEL(label1), "  <i>Interlacing correction:</i> ");
	gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
	gtk_widget_show (label1);

	combo_interlacecorr = gtk_combo_new();
	gtk_combo_set_popdown_strings (GTK_COMBO (combo_interlacecorr), 
			interlace_correct);
	combo_entry_interlacecorr = GTK_COMBO (combo_interlacecorr)->entry;
	gtk_widget_set_usize (combo_interlacecorr, 200, -2 );
	gtk_signal_connect(GTK_OBJECT(combo_entry_interlacecorr), "changed",
			GTK_SIGNAL_FUNC (set_interlacing), NULL);
	gtk_table_attach_defaults (GTK_TABLE (table),combo_interlacecorr,
			*tx+1, *tx+2, *ty, *ty+1);
	gtk_widget_show (combo_interlacecorr);
	(*ty)++;
}

/** create the layout for yuvscaler programm 
  @param points to the table widget
  @param the current x position in the widget 
  @param the current y position in the widget */
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
	yuvscalermode = g_list_append (yuvscalermode, "FAST_WIDE2VCD");
	yuvscalermode = g_list_append (yuvscalermode, "RATIO_388_352_1_1");
	yuvscalermode = g_list_append (yuvscalermode, "RATIO_1_1_288_200");
	yuvscalermode = g_list_append (yuvscalermode, "RATIO_2_1_2_1");
	yuvscalermode = g_list_append (yuvscalermode, "RATIO_352_320_288_200");

	output_window = g_list_append (output_window, "as is");
	output_window = g_list_append (output_window, "VCD");
	output_window = g_list_append (output_window, "SVCD");
	output_window = g_list_append (output_window, "DVD");
	output_window = g_list_append (output_window, "352x240");

	label1 = gtk_label_new("  Input window: ");
	gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table),
			label1, *tx,*tx+1,*ty,*ty+1);
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

  label1 = gtk_label_new("  Output size: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_scaler_output = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_scaler_output),
    output_window);
  combo_entry_scaleroutput = GTK_COMBO (combo_scaler_output)->entry;
  gtk_signal_connect(GTK_OBJECT(combo_entry_scaleroutput), "changed",
    GTK_SIGNAL_FUNC (set_scalerstrings), "SCALEROUTPUT");
  gtk_widget_set_usize (combo_scaler_output, 130, -2 );
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

   vbitrate = g_list_append (vbitrate, "default");
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

/* create the video options for the yuv2lav specific options */
void create_yuv2lav_layout (GtkWidget *table, int *tx, int *ty)
{
GtkWidget *label1, *combo_format, *combo_interlace, *combo_quality;
GtkWidget *combo_maxfilesize;
GList *yuv2lav_quality = NULL;
GList *yuv2lav_filesize = NULL;

  yuv2lav_quality = g_list_append (yuv2lav_quality, "default");
  yuv2lav_quality = g_list_append (yuv2lav_quality, "50");
  yuv2lav_quality = g_list_append (yuv2lav_quality, "70");
  yuv2lav_quality = g_list_append (yuv2lav_quality, "100");

  yuv2lav_filesize = g_list_append (yuv2lav_filesize, "default");
  yuv2lav_filesize = g_list_append (yuv2lav_filesize, "640");
  yuv2lav_filesize = g_list_append (yuv2lav_filesize, "700");
  yuv2lav_filesize = g_list_append (yuv2lav_filesize, "1990");

  label1 = gtk_label_new ("  Output Format: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_format = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_format), yuv2lav_format);
  combo_entry_format = GTK_COMBO (combo_format)->entry;
  gtk_widget_set_usize (combo_format, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_format), "changed",
                      GTK_SIGNAL_FUNC (set_format), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_format,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_format);
  (*ty)++;

  label1 = gtk_label_new ("  Output Quality: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_quality = gtk_combo_new();
  gtk_combo_set_popdown_strings (GTK_COMBO (combo_quality), yuv2lav_quality);
  combo_entry_quality = GTK_COMBO (combo_quality)->entry;
  gtk_widget_set_usize (combo_quality, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_quality), "changed",
                      GTK_SIGNAL_FUNC (set_quality), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_quality,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_quality);
  (*ty)++;

  label1 = gtk_label_new ("  Output File Size: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);
  
  combo_maxfilesize = gtk_combo_new();
  gtk_combo_set_popdown_strings(GTK_COMBO(combo_maxfilesize),yuv2lav_filesize);
  combo_entry_maxfilesize = GTK_COMBO (combo_maxfilesize)->entry;
  gtk_widget_set_usize (combo_maxfilesize, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_maxfilesize), "changed",
                      GTK_SIGNAL_FUNC (set_maxfilesize), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_maxfilesize,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_maxfilesize);
  (*ty)++;

  label1 = gtk_label_new ("  Force Interlacing to: ");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), label1,*tx,*tx+1,*ty,*ty+1);
  gtk_widget_show (label1);

  combo_interlace = gtk_combo_new();
  gtk_combo_set_popdown_strings(GTK_COMBO(combo_interlace),yuv2lav_interlace);
  combo_entry_interlace = GTK_COMBO (combo_interlace)->entry;
  gtk_widget_set_usize (combo_interlace, 50, -2 );
  gtk_signal_connect(GTK_OBJECT(combo_entry_interlace), "changed",
                      GTK_SIGNAL_FUNC (set_2lav_interlacing), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), combo_interlace,
                                             *tx+1,*tx+2,*ty,*ty+1);
  gtk_widget_show (combo_interlace);
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

  label = gtk_label_new (" Mplex / Video option: ");
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
int tx,ty; /* table size x, y */

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

  create_denoise_layout(table, &tx, &ty);

  create_video_layout (table, &tx, &ty);

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show(table);
}

void create_yuv2lav_options(GtkWidget *hbox)
{GtkWidget *label, *table;
int tx, ty;  /* table size x, y */

tx = 2;
ty = 9;

  table = gtk_table_new (tx, ty, FALSE);
  tx = 0;
  ty = 0;

  label = gtk_label_new (" MJPEG options: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;
 
  create_video_options_layout(table, &tx, &ty);

  create_yuvscaler_layout (table, &tx, &ty);

  create_denoise_layout(table, &tx, &ty);

  create_yuv2lav_layout(table, &tx, &ty);
   
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
  (*point).addoutputnorm=tempenco.addoutputnorm;
  sprintf((*point).interlacecorr,"%s",tempenco.interlacecorr);
  (*point).audiobitrate=tempenco.audiobitrate;
  (*point).outputbitrate=tempenco.outputbitrate;
  sprintf((*point).forcestereo,"%s",tempenco.forcestereo);
  sprintf((*point).forcemono,"%s",tempenco.forcemono);
  sprintf((*point).forcevcd,"%s",tempenco.forcevcd);
  (*point).use_yuvdenoise=tempenco.use_yuvdenoise;
  (*point).deinterlace=tempenco.deinterlace;
  (*point).sharpness=tempenco.sharpness;
  (*point).denois_thhold=tempenco.denois_thhold;
  (*point).average_frames=tempenco.average_frames;
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
  sprintf((*point).codec,"%s",tempenco.codec);
}

/* open a new window with all the options in it */
void open_mpeg_window(GtkWidget *widget, gpointer data)
{
GtkWidget *options_window, *button;
GtkWidget *hbox,*vbox, *label1;

if (g_list_length (muxformat) == 0)
  {
    muxformat = g_list_append (muxformat, "Auto MPEG 1");
    muxformat = g_list_append (muxformat, "standard VCD");
    muxformat = g_list_append (muxformat, "user-rate VCD");
    muxformat = g_list_append (muxformat, "Auto MPEG 2");
    muxformat = g_list_append (muxformat, "standard SVCD");
    muxformat = g_list_append (muxformat, "user-rate SVCD");
    muxformat = g_list_append (muxformat, "VCD Stills");
    muxformat = g_list_append (muxformat, "SVCD Stills");
    muxformat = g_list_append (muxformat, "DVD");
  } 

if (g_list_length (streamdata) == 0)
  {
    streamdata = g_list_append (streamdata, "from streams");
    streamdata = g_list_append (streamdata, "1412");
    streamdata = g_list_append (streamdata, "1770");
    streamdata = g_list_append (streamdata, "2280");
    streamdata = g_list_append (streamdata, "2780");
  }

if (g_list_length(streamdata) == 5)
  changed_streamdatarate=0;
else
  changed_streamdatarate=1;

if (g_list_length (yuv2lav_format) == 0)
  {
    yuv2lav_format = g_list_append (yuv2lav_format, "AVI");
    yuv2lav_format = g_list_append (yuv2lav_format, "AVI fields reversed");
    yuv2lav_format = g_list_append (yuv2lav_format, "Quicktime");
  }

if (g_list_length (yuv2lav_interlace) == 0)
  {
    yuv2lav_interlace = g_list_append (yuv2lav_interlace, "stream default");
    yuv2lav_interlace = g_list_append (yuv2lav_interlace, "not interlaced");
    yuv2lav_interlace = g_list_append (yuv2lav_interlace, "top field first");
    yuv2lav_interlace = g_list_append (yuv2lav_interlace, "bottom field first");
  }

  /* Here we set the struct we have to use */
  if (verbose)
    printf("Which task to do :%s \n",(char*)data);

  if      (strcmp ((char*)data,"MPEG1") == 0)
    point = &encoding;
  else if (strcmp ((char*)data,"MPEG2") == 0)
    point = &encoding2;
  else if (strcmp ((char*)data,"GENERIC") == 0)
    point = &encoding_gmpeg;
  else if (strcmp ((char*)data,"VCD") == 0)
    point = &encoding_vcd;
  else if (strcmp ((char*)data,"SVCD") == 0)
    point = &encoding_svcd;
  else if (strcmp ((char*)data,"DVD") == 0)
    point = &encoding_dvd;
  else if (strcmp ((char*)data,"MJPEG") == 0)
    point = &encoding_yuv2lav;
  else 
    point = &encoding; /* fallback should never be used ;) */

 /* here the pointers are set to point to the correct set of mpeg options */
 init_tempenco(data);

  options_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  hbox = gtk_hbox_new (FALSE, 10);
  vbox = gtk_vbox_new (FALSE, 10);

  if ( strcmp(data,"MJPEG") == 0)
    {
      create_yuv2lav_options(hbox);

      show_data_lav2yuv  (data);
      show_data_yuvtools (data);
      show_data_yuv2lav  (data);
    }
  else
    { 
      /* Left table for the audio and mplex options */
      create_audio_mplex_layout(hbox);

      /* Right table for the video options */
      create_yuv2mpeg_layout(hbox);

      show_data_audiomp2 (data);
      show_data_lav2yuv  (data);
      show_data_yuvtools (data);
      show_data_mpeg2enc (data);
      show_data_mplex    (data);
    }

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox);

  /* Show the OK and cancel Button */

  hbox = gtk_hbox_new(TRUE, 20);

  button = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC (accept_mpegoptions), (gpointer) "test");
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
           G_CALLBACK(gtk_widget_destroy), G_OBJECT(options_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  g_signal_connect_swapped(G_OBJECT(button), "clicked",
          G_CALLBACK(gtk_widget_destroy), G_OBJECT(options_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* Show the last note */
  hbox = gtk_hbox_new(TRUE, 20);
  label1 = gtk_label_new (NULL);
  gtk_label_set_markup(GTK_LABEL(label1), "  <i>Note: Options in italic are only aviable in scripts</i>");
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, GTK_MISC(label1)->yalign);
  gtk_box_pack_start (GTK_BOX (hbox), label1, TRUE, TRUE, 20);
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* pack the last widget into the windows and show everything */
  gtk_container_add (GTK_CONTAINER (options_window), vbox);
  gtk_widget_show(vbox);

  gtk_widget_show(options_window);

  
}
