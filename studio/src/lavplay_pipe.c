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
#include <gdk/gdkx.h>

#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#include "studio.h"
#include "gtktvplug.h"
#include "pipes.h"
#include "gtkfunctions.h"

#include "file_widget_open.xpm"
#include "editor_play.xpm"
#include "editor_play_i.xpm"
#include "editor_pause.xpm"
#include "editor_step.xpm"
#include "editor_step_i.xpm"
#include "editor_fast.xpm"
#include "editor_fast_i.xpm"

/* Variables */
GtkWidget *textfield2, *lavplay_tv;
GtkWidget *label_lavplay_format, *label_lavplay_fps, *label_lavplay_vsize, *label_lavplay_stereo;
GtkWidget *label_lavplay_absize, *label_lavplay_abrate, *label_lavplay_time, *label_lavplay_frames;
//int lavplay_inp_pipe, lavplay_out_pipe, lavplay_pid;
//int studio_lavplay_int, 
int adjustment_a_changed;
int total_frames, lavplay_abrate, lavplay_absize, lavplay_sizew, lavplay_sizeh;
int lavplay_fps, lavplay_chans;
char lavplay_norm;
//gint studio_lavplay_gint;
GtkObject *lavplay_slider_adj;

/* Forward declarations */
void lavplay_stopped(void);
void table_lavplay_set_text(int frame);
void set_lavplay_log(int norm, int cur_pos, int cur_speed);
void lavplay_slider_value_changed(GtkAdjustment *adj);
void quit_lavplay_with_error(char *msg);
void process_lavplay_input(char *msg);
/*void lavplay_input(gpointer data, gint source, GdkInputCondition condition);*/
void file_ok_sel2( GtkWidget *w, GtkFileSelection *fs );
void create_lavplay_child(void);
void command_to_lavplay(GtkWidget *widget, gpointer data);
void play_file(GtkWidget *widget, gpointer data);
void create_filesel2(GtkWidget *widget, gpointer data);
void create_lavplay_logtable(GtkWidget *table);
GtkWidget *create_lavplay_buttons(void);

/* ================================================================= */

void table_lavplay_set_text(int frame)
{
	int f, h, m, s;
	char temp[100];

	frame++;

	if (lavplay_fps==0)
		lavplay_fps = lavplay_norm=='n'?30:25;

	s = frame/lavplay_fps;
	f = frame%lavplay_fps;
	m = s/60;
	s = s%60;
	h = m/60;
	m = m%60;

	if(lavplay_norm=='n')
		sprintf(temp, "NTSC");
	else
		sprintf(temp, "PAL/SECAM");
	gtk_label_set_text(GTK_LABEL(label_lavplay_format), temp);

	sprintf(temp, "%d fps", lavplay_fps);
	gtk_label_set_text(GTK_LABEL(label_lavplay_fps), temp);

	sprintf(temp, "%d x %d", lavplay_sizew, lavplay_sizeh);
	gtk_label_set_text(GTK_LABEL(label_lavplay_vsize), temp);

	sprintf(temp, "%d bit", lavplay_absize);
	gtk_label_set_text(GTK_LABEL(label_lavplay_absize), temp);

	sprintf(temp, "%d Hz", lavplay_abrate);
	gtk_label_set_text(GTK_LABEL(label_lavplay_abrate), temp);

	if (lavplay_chans == 1)
		sprintf(temp, "Mono");
	else
		sprintf(temp, "Stereo");
	gtk_label_set_text(GTK_LABEL(label_lavplay_stereo), temp);

	sprintf(temp, "%2d:%2.2d:%2.2d:%2.2d",h,m,s,f);
	gtk_label_set_text(GTK_LABEL(label_lavplay_time), temp);

	sprintf(temp, "%d/%d", frame, total_frames);
	gtk_label_set_text(GTK_LABEL(label_lavplay_frames), temp);
}

void set_lavplay_log(int norm, int cur_pos, int cur_speed)
{
	adjustment_a_changed = TRUE;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(lavplay_slider_adj), ((2500000 / total_frames)*(cur_pos+1)));
	table_lavplay_set_text(cur_pos);
}

void lavplay_slider_value_changed(GtkAdjustment *adj)
{
	double val;
	char out[256];

	if (pipe_is_active(LAVPLAY) && !adjustment_a_changed)
	{
		val = adj->value / 2500000;
		sprintf(out,"s%ld\n",(long)(val*total_frames));
		/*write(lavplay_out_pipe,out,strlen(out));*/
		write_pipe(LAVPLAY, out);
	}
	else
		adjustment_a_changed = FALSE;
}

void quit_lavplay_with_error(char *msg)
{
	quit_lavplay();
	if (verbose) g_print("Lavplay error: %s\n", msg);
	gtk_show_text_window(STUDIO_ERROR,
		"Lavplay stopped and gave the following error:",
		msg);
}

void process_lavplay_input(char *msg)
{
	int cur_pos, cur_speed;

	if(msg[0]=='@')
	{
		int n, tot=0;
		for (n=0;msg[n];n++)
			if (msg[n]=='/')
				tot++;
		if (tot==2)
			sscanf(msg+1,"%c%d/%d/%d",&lavplay_norm,&cur_pos,&total_frames,&cur_speed);
		else
		{
			int norm_num;
			sscanf(msg+1,"%d/%d/%d/%d",&norm_num,&cur_pos,&total_frames,&cur_speed);
			lavplay_norm = (norm_num==25)?'p':'n';
		}
		set_lavplay_log(lavplay_norm, cur_pos, cur_speed);
		return;
	}

	else if (strncmp(msg, "**ERROR:", 8) == 0)
	{
		// Error handling
		quit_lavplay_with_error(msg+9);
	}

	if(strncmp(msg, "--DEBUG:    ", 12) == 0)
	{
		if (strncmp(msg+12, "frames: ", 8)==0)
		{
			sscanf(msg+12,"frames: %d",&total_frames);
		}
		else if (strncmp(msg+12, "width: ", 7)==0)
		{
			sscanf(msg+12,"width: %d",&lavplay_sizew);
		}
		else if (strncmp(msg+12, "height: ", 8)==0)
		{
			sscanf(msg+12,"height: %d",&lavplay_sizeh);
		}
		else if (strncmp(msg+12, "frames/sec: ", 12)==0)
		{
			sscanf(msg+12,"frames/sec: %d",&lavplay_fps);
		}
		else if (strncmp(msg+12, "audio chans: ", 13)==0)
		{
			sscanf(msg+12,"audio chans: %d",&lavplay_chans);
		}
		else if (strncmp(msg+12, "audio bits: ", 12)==0)
		{
			sscanf(msg+12,"audio bits: %d",&lavplay_absize);
		}
		else if (strncmp(msg+12, "audio rate: ", 12)==0)
		{
			sscanf(msg+12,"audio rate: %d",&lavplay_abrate);
		}
		return;
	}

	if (strncmp(msg, "   INFO: Output norm: ",22) == 0)
	{
		if (msg[23] == 'N')
			lavplay_norm = 'n';
		else
			lavplay_norm = 'p';
		table_lavplay_set_text(0);
	}
}

void quit_lavplay()
{
	close_pipe(LAVPLAY);
}

void lavplay_stopped()
{
	if (verbose) g_print("\nLavplay Stopped\n");
	gtk_adjustment_set_value(GTK_ADJUSTMENT(lavplay_slider_adj), 0);
}

void create_lavplay_child()
{
	char *lavplay_command[256];
	int n;

	n=0;
	lavplay_command[n] = LAVPLAY_LOCATION; n++;
	lavplay_command[n] = "-q"; n++;
	lavplay_command[n] = "-g"; n++;
	lavplay_command[n] = "-v"; n++; lavplay_command[n] = "2"; n++;
	lavplay_command[n] = encoding_syntax_style==140?"-C":"-pC"; n++;
	lavplay_command[n] = gtk_entry_get_text(GTK_ENTRY(textfield2)); n++;
	lavplay_command[n] = NULL;

	start_pipe_command(lavplay_command, LAVPLAY);
}

void play_file(GtkWidget *widget, gpointer data)
{
	if (pipe_is_active(LAVPLAY))
	{
		printf("Error: lavplay is already active!\n");
		return;
	}
	create_lavplay_child();
}

void reset_lavplay_tv()
{
	gtk_widget_set_usize(GTK_WIDGET(lavplay_tv), tv_width_process, tv_height_process);
	gtk_tvplug_set(GTK_WIDGET(lavplay_tv), "port", port);
}

void file_ok_sel2( GtkWidget *w, GtkFileSelection *fs )
{
	gtk_entry_set_text(GTK_ENTRY(textfield2), gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

void create_filesel2(GtkWidget *widget, gpointer data)
{
	GtkWidget *filew;
	
	filew = gtk_file_selection_new ("Linux Video Studio - Select Location");
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		"clicked", (GtkSignalFunc) file_ok_sel2, filew);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), gtk_entry_get_text(GTK_ENTRY(textfield2)));
	gtk_widget_show(filew);

}

void create_lavplay_logtable(GtkWidget *table)
{
	GtkWidget *label;

	label = gtk_label_new("Output Norm: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	gtk_widget_show (label);
	label_lavplay_format = gtk_label_new("unknown");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_format), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_format, 1, 2, 0, 1);
	gtk_widget_show (label_lavplay_format);

	label = gtk_label_new("Frames/sec: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	gtk_widget_show (label);
	label_lavplay_fps = gtk_label_new("0 fps");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_fps), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_fps, 1, 2, 1, 2);
	gtk_widget_show (label_lavplay_fps);

	label = gtk_label_new("Video Size: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	gtk_widget_show (label);
	label_lavplay_vsize = gtk_label_new("0 x 0");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_vsize), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_vsize, 1, 2, 2, 3);
	gtk_widget_show (label_lavplay_vsize);

	label = gtk_label_new("Audio bitsize: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);
	gtk_widget_show (label);
	label_lavplay_absize = gtk_label_new("0 bit");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_absize), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_absize, 1, 2, 3, 4);
	gtk_widget_show (label_lavplay_absize);

	label = gtk_label_new("Audio bitrate: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 4, 5);
	gtk_widget_show (label);
	label_lavplay_abrate = gtk_label_new("0 Hz");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_abrate), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_abrate, 1, 2, 4, 5);
	gtk_widget_show (label_lavplay_abrate);

	label = gtk_label_new("Mono/stereo: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 5, 6);
	gtk_widget_show (label);
	label_lavplay_stereo = gtk_label_new("unknown");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_stereo), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_stereo, 1, 2, 5, 6);
	gtk_widget_show (label_lavplay_stereo);

	label = gtk_label_new("Time: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 6, 7);
	gtk_widget_show (label);
	label_lavplay_time = gtk_label_new("0.00.00:00");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_time), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_time, 1, 2, 6, 7);
	gtk_widget_show (label_lavplay_time);

	label = gtk_label_new("Frames: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 7, 8);
	gtk_widget_show (label);
	label_lavplay_frames = gtk_label_new("0/0");
	gtk_misc_set_alignment(GTK_MISC(label_lavplay_frames), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label_lavplay_frames, 1, 2, 7, 8);
	gtk_widget_show (label_lavplay_frames);
}

void command_to_lavplay(GtkWidget *widget, gpointer data)
{
	char command[10];
/*	int l;*/

	sprintf(command, "%s\n", (char *)data);
/*	l = strlen((char *)data)+1;

	write(lavplay_out_pipe,command,l);*/

	write_pipe(LAVPLAY, command);
}

GtkWidget *create_lavplay_buttons()
{
	GtkWidget *pixmap_widget, *hbox3, *button;
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();

	hbox3 = gtk_hbox_new(TRUE, 5);

	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(editor_fast_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Fast Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay),"p-3");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(editor_play_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay),"p-1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(editor_step_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "One Frame Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay),"-");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(editor_pause_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Pause", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay),"p0");
	gtk_box_pack_start (GTK_BOX (hbox3), button,FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(editor_step_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "One Frame Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay),"+");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(editor_play_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay),"p1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(editor_fast_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Fast Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay),"p3");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	return hbox3;
}

GtkWidget *create_lavplay_layout()
{
	GtkWidget *hbox2, *hbox, *vbox, *vbox2, *label, *button;
	GtkWidget *scrollbar, *table, *pixmap_widget, *hbox3;
	char tempfile[100];
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();

	vbox = gtk_vbox_new(FALSE,0);
	vbox2 = gtk_vbox_new(FALSE,0);
	hbox = gtk_hbox_new(FALSE,20);
	hbox2 = gtk_hbox_new(FALSE,20);
	lavplay_tv = gtk_tvplug_new(port);
	if (lavplay_tv == NULL)
	{
		lavplay_tv = gtk_event_box_new();
		set_background_color(lavplay_tv, 0,0,0);
	}
	gtk_widget_set_usize(GTK_WIDGET(lavplay_tv), tv_width_process, tv_height_process);
	if (port == -1)
		exit(1);
	gtk_box_pack_start (GTK_BOX (vbox2), lavplay_tv,TRUE, FALSE, 0);
	gtk_widget_show(lavplay_tv);

	lavplay_slider_adj = gtk_adjustment_new(0, 0, 2500000, 10000, 100000, 0);
	gtk_signal_connect (GTK_OBJECT (lavplay_slider_adj), "value_changed",
		GTK_SIGNAL_FUNC (lavplay_slider_value_changed), NULL);
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (lavplay_slider_adj));
	gtk_range_set_update_policy (GTK_RANGE (scrollbar), GTK_UPDATE_CONTINUOUS);
	gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 0);
	gtk_box_pack_start(GTK_BOX (vbox2), scrollbar, FALSE, FALSE, 10);
	gtk_widget_show(scrollbar);

	gtk_box_pack_start(GTK_BOX (hbox2), vbox2, TRUE, FALSE, 20);
	gtk_widget_show(vbox2);
	gtk_box_pack_start(GTK_BOX (vbox), hbox2, TRUE, TRUE, 20);
	gtk_widget_show(hbox2);

	hbox3 = gtk_hbox_new(FALSE, 10);
	vbox2 = gtk_vbox_new(FALSE, 10);

	hbox2 = gtk_hbox_new(FALSE,10);
	label = gtk_label_new("Play from: ");
	gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	textfield2 = gtk_entry_new();
	sprintf(tempfile, "%s/%s", getenv("HOME"), "video001.avi");
	gtk_entry_set_text(GTK_ENTRY(textfield2), tempfile);
	gtk_box_pack_start (GTK_BOX (hbox2), textfield2, TRUE, TRUE, 0);
	gtk_widget_show(textfield2);

	button = gtk_button_new(); //_with_label("Select");
	pixmap_widget = gtk_widget_from_xpm_data(file_widget_open_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Open movie or editlist", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_filesel2), NULL);
	gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, TRUE, FALSE, 10);
	gtk_widget_show(hbox2);

	hbox2 = gtk_hbox_new(TRUE, 20);

	button = gtk_button_new_with_label("[Start]");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(play_file), NULL);
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("[Stop]");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(quit_lavplay), NULL);
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, TRUE, FALSE, 10);
	gtk_widget_show(hbox2);

	hbox2 = create_lavplay_buttons();
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 10);
	gtk_widget_show(hbox2);

	gtk_box_pack_start (GTK_BOX (hbox3), vbox2, TRUE, TRUE, 10);
	gtk_widget_show(vbox2);

	table = gtk_table_new (2,8, FALSE);
	create_lavplay_logtable(table);
	gtk_widget_show(table);
	gtk_box_pack_start (GTK_BOX (hbox3), table, TRUE, TRUE, 20);

	gtk_box_pack_start (GTK_BOX (vbox), hbox3, TRUE, TRUE, 10);
	gtk_widget_show(hbox3);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 20);
	gtk_widget_show(vbox);

	return hbox;
}
