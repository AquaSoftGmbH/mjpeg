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
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "gtkfunctions.h"
#include "studio.h"
#include "gtktvplug.h"
#include "mixer.h"
#include "pipes.h"

#include "slider_hue.xpm"
#include "slider_contrast.xpm"
#include "slider_sat_colour.xpm"
#include "slider_brightness.xpm"
#include "slider_volume.xpm"
#include "file_widget_open.xpm"
#include "editor_record.xpm"
#include "editor_stop.xpm"

/* Variables */
//int inp_pipe, out_pipe, pid_lr, capturing;
GtkWidget *start_button, *stop_button, *init_button, *lavrec_label, *textfield, *tv;
//gint studio_lavreclog_gint;
GtkWidget *table_label_time, *table_label_format, *table_label_vinput, *table_label_ainput;
GtkWidget *table_label_aerrors, *table_label_inserted, *table_label_deleted, *table_label_lost;
int mixer_id;
int current_file;
char files_recorded[256][256];
GtkWidget *scene_detection_window;
char eli_file[256];

GtkWidget *scene_detection_status_label, *scene_detection_button_label, *scene_detection_bar;

/* Forward declarations */
void lavrec_quit(void);
void studio_set_lavrec_label(char *text);
void quit_lavrec_with_error(char *msg);
void dispatch_input(char *buff);
/*void lavreclog_input_cb (gpointer data, gint source, GdkInputCondition condition);*/
void create_child(void);
void emulate_enter(void);
void text_changed(GtkWidget *widget, gpointer data);
void do_scene_detection(GtkWidget *w, GtkFileSelection *fs);
void scene_detection(void);
void do_no_scene_detection(GtkWidget *widget, gpointer data);
void scene_detection_file(GtkWidget *widget, gpointer data);
void file_ok_sel1( GtkWidget *w, GtkFileSelection *fs );
void create_filesel1(GtkWidget *widget, gpointer data);
void write_result(GtkWidget *widget, gpointer data);
GtkWidget *create_buttons(GtkWidget *vbox, GtkWidget *window);
void create_lavrec_logtable(GtkWidget *table);
GtkWidget *create_audio_sliders(void);
void audio_slider_changed(GtkAdjustment *adj, gpointer data);
GtkWidget *create_video_sliders(void);
void video_slider_changed(GtkAdjustment *adj, char *what);
void create_scene_detection_window(void);
void scene_detection_input_cb(char *input);
void stop_scene_detection_process(GtkWidget *widget, gpointer data);
void scene_detection_finished(void);
void scene_detection_open_movie(GtkWidget *widget, gpointer data);

/* ================================================================= */

void scene_detection_open_movie(GtkWidget *widget, gpointer data)
{
	global_open_location(eli_file);
}

void scene_detection_finished()
{
	GtkWidget *dialog_window, *button, *vbox, *hbox, *label;

	if (scene_detection_button_label)
		gtk_label_set_text(GTK_LABEL(scene_detection_button_label),
		" Close ");
	if (scene_detection_bar)
		gtk_progress_bar_update(GTK_PROGRESS_BAR(scene_detection_bar),
		1.0);
	if (scene_detection_status_label)
		gtk_label_set_text(GTK_LABEL(scene_detection_status_label),
		"Scene detection finished!");

	scene_detection_button_label = NULL;
	scene_detection_status_label = NULL;
	scene_detection_bar = NULL;
	gtk_widget_destroy(scene_detection_window);

	/* ask to open eli_file */
	dialog_window = gtk_window_new(GTK_WINDOW_DIALOG);
	vbox = gtk_vbox_new(FALSE, 5);

	gtk_window_set_title (GTK_WINDOW(dialog_window),
		"Linux Video Studio - Question");
	gtk_container_set_border_width (GTK_CONTAINER (dialog_window), 20);

	label = gtk_label_new("Would you like to open this movie\n" \
		"in all the editor-subparts?");
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(TRUE, 5);
	button = gtk_button_new_with_label("Yes");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		(GtkSignalFunc)scene_detection_open_movie, NULL);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(dialog_window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show(button);
	button = gtk_button_new_with_label("No");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(dialog_window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show(button);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_container_add (GTK_CONTAINER (dialog_window), vbox);
	gtk_widget_show(vbox);

	gtk_grab_add(dialog_window);
	gtk_widget_show(dialog_window);
}

void stop_scene_detection_process(GtkWidget *widget, gpointer data)
{
	close_pipe(LAV2YUV_S);
	scene_detection_button_label = NULL;
	scene_detection_status_label = NULL;
	scene_detection_bar = NULL;
	gtk_widget_destroy(scene_detection_window);
}

void scene_detection_input_cb(char *input)
{
	if (strncmp(input, "--DEBUG: frame", 14)==0)
	{
		int n1,n2,n3,n4;
		sscanf(input, "--DEBUG: frame %d/%d, lum_mean %d, delta_lum %d",
			&n1, &n2, &n3, &n4);
		if (scene_detection_status_label)
			gtk_label_set_text(GTK_LABEL(scene_detection_status_label),
			input+9);
		if (scene_detection_bar)
			gtk_progress_bar_update(GTK_PROGRESS_BAR(scene_detection_bar),
			((double)n1)/n2);
	}
	else if (strncmp(input, "**ERROR:", 8) == 0)
	{
		/* error */
		stop_scene_detection_process(NULL, NULL);
		gtk_show_text_window(STUDIO_ERROR,
			"Scene detection failed, lav2yuv gave the following error: %s",
			input+9);
	}
}

void create_scene_detection_window()
{
	GtkWidget *vbox, *button;

	scene_detection_window = gtk_window_new(GTK_WINDOW_DIALOG);
	vbox = gtk_vbox_new(FALSE, 5);

	gtk_window_set_title (GTK_WINDOW(scene_detection_window),
		"Linux Video Studio - Scene detection");
	gtk_container_set_border_width (GTK_CONTAINER (scene_detection_window), 20);

	scene_detection_status_label = gtk_label_new(" ");
	gtk_box_pack_start (GTK_BOX (vbox), scene_detection_status_label,
		FALSE,FALSE, 10);
	gtk_widget_show (scene_detection_status_label);

	scene_detection_bar = gtk_progress_bar_new();
	gtk_box_pack_start (GTK_BOX (vbox), scene_detection_bar,
		FALSE, FALSE, 0);
	gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(scene_detection_bar),
		GTK_PROGRESS_LEFT_TO_RIGHT);
	gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(scene_detection_bar),
		GTK_PROGRESS_CONTINUOUS);
	gtk_progress_set_show_text(GTK_PROGRESS(scene_detection_bar), 1);
	gtk_progress_set_format_string(GTK_PROGRESS(scene_detection_bar), "%p\%");
	gtk_widget_show(scene_detection_bar);

	button = gtk_button_new();
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		(GtkSignalFunc)stop_scene_detection_process, NULL);
	scene_detection_button_label = gtk_label_new(" Cancel ");
	gtk_container_add (GTK_CONTAINER (button), scene_detection_button_label);
	gtk_widget_show(scene_detection_button_label);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_container_add (GTK_CONTAINER (scene_detection_window), vbox);
	gtk_widget_show(vbox);

	gtk_grab_add(scene_detection_window);
	gtk_widget_show(scene_detection_window);
}

void do_scene_detection(GtkWidget *w, GtkFileSelection *fs)
{
	char *lav2yuv_command[256];
	char temp1[256], temp2[256];
	int n=0,i;

	lav2yuv_command[n] = app_name(LAV2YUV_S); n++;
	lav2yuv_command[n] = "-v2"; n++;
	lav2yuv_command[n] = "-D"; n++;
	sprintf(temp1, "%d", scene_detection_width_decimation);
	lav2yuv_command[n] = temp1; n++;
	lav2yuv_command[n] = "-T"; n++;
	sprintf(temp2, "%d", scene_detection_treshold);
	lav2yuv_command[n] = temp2; n++;
	lav2yuv_command[n] = "-S"; n++;
	sprintf(eli_file,
		gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
	lav2yuv_command[n] = eli_file; n++;

	for(i=0;i<current_file;i++)
	{
		lav2yuv_command[n] = files_recorded[i]; n++;
	}

	lav2yuv_command[n] = NULL;

	create_scene_detection_window();
	start_pipe_command(lav2yuv_command, LAV2YUV_S); /* lav2yuv */
}

void scene_detection_file(GtkWidget *widget, gpointer data)
{
	GtkWidget *filew;
	
	filew = gtk_file_selection_new ("Linux Video Studio - Select Location");
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		"clicked", (GtkSignalFunc) do_scene_detection, filew);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), "project.eli");

	gtk_grab_add(filew);
	gtk_widget_show(filew);
}

void scene_detection()
{
	GtkWidget *popup_window, *button, *label, *hbox, *vbox;

	popup_window = gtk_window_new(GTK_WINDOW_DIALOG);

	vbox = gtk_vbox_new(FALSE, 5);

	gtk_window_set_title (GTK_WINDOW(popup_window), "Linux Video Studio - Scene Detection");
	gtk_container_set_border_width (GTK_CONTAINER (popup_window), 20);

	label = gtk_label_new("Would you like to do\nscene detection (through lav2yuv)?");
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(TRUE, 5);
	button = gtk_button_new_with_label("Yes");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		(GtkSignalFunc)scene_detection_file, NULL);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(popup_window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show(button);
	button = gtk_button_new_with_label("No");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(popup_window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show(button);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_container_add (GTK_CONTAINER (popup_window), vbox);
	gtk_widget_show(vbox);

	gtk_grab_add(popup_window);
	gtk_widget_show(popup_window);
}

void studio_set_lavrec_label(char *text)
{
	gtk_label_set_text(GTK_LABEL(lavrec_label), text);
}

void table_set_text(int h, int m, int s, int f, int l, int ins, int del, int aerr)
{
	char temp[100];

	if (input_source == 'P') sprintf(temp, "S-VHS/PAL");
	else if (input_source == 'p') sprintf(temp, "Composite/PAL");
	else if (input_source == 'S') sprintf(temp, "S-VHS/SECAM");
	else if (input_source == 's') sprintf(temp, "Composite/SECAM");
	else if (input_source == 'N') sprintf(temp, "S-VHS/NTSC");
	else if (input_source == 'n') sprintf(temp, "Composite/NTSC");
	else if (input_source == 't') sprintf(temp, "TV-Input");
	else if (input_source == 'a') sprintf(temp, "Automatic");
	gtk_label_set_text(GTK_LABEL(table_label_vinput), temp);

	if (audio_recsrc == 'l') sprintf(temp, "Line-in");
	else if (audio_recsrc == 'm') sprintf(temp, "Microphone");
	else if (audio_recsrc == 'c') sprintf(temp, "CD-ROM");
	gtk_label_set_text(GTK_LABEL(table_label_ainput), temp);

	if (video_format == 'a') sprintf(temp, "MJPEG/AVI");
	else if (video_format == 'A') sprintf(temp, "MJPEG/AVI (f.e.)");
	else if (video_format == 'q') sprintf(temp, "Quicktime");
	else if (video_format == 'm') sprintf(temp, "Movtar");
	gtk_label_set_text(GTK_LABEL(table_label_format), temp);

	sprintf(temp, "%d.%2.2d.%2.2d:%2.2d", h, m, s, f);
	gtk_label_set_text(GTK_LABEL(table_label_time), temp);

	sprintf(temp, "%d", l);
	gtk_label_set_text(GTK_LABEL(table_label_lost), temp);

	sprintf(temp, "%d", ins);
	gtk_label_set_text(GTK_LABEL(table_label_inserted), temp);

	sprintf(temp, "%d", del);
	gtk_label_set_text(GTK_LABEL(table_label_deleted), temp);

	sprintf(temp, "%d", aerr);
	gtk_label_set_text(GTK_LABEL(table_label_aerrors), temp);
}

void quit_lavrec_with_error(char *msg)
{
	emulate_ctrl_c();
	if (verbose) g_print("Lavrec error: %s\n", msg);
	gtk_show_text_window(STUDIO_ERROR,
		"Lavrec failed with the following error: %s",
		msg);
}

void dispatch_input(char *buff)
{
	char buff2[4096], buff3[4096];
	int h, m, s, f, l, i, d, a, bla;

	if (strcmp(buff, "Press enter to start recording>") == 0)
	{
		studio_set_lavrec_label("Press Start to start");
	}
	else if (sscanf(buff, "%d.%d.%d:%d lst: %d ins: %d del: %d ae: %d",
		&h, &m, &s, &f, &l, &i, &d, &a)==8) /* 1.4.x syntax */
	{
		table_set_text(h, m, s, f, l, i, d, a);
	}
	else if (sscanf(buff, "%d.%d.%d:%d int: %d lst: %d ins: %d del: %d ae: %d",
		&h, &m, &s, &f, &bla, &l, &i, &d, &a) == 9) /* 1.5.x syntax */
	{
		table_set_text(h, m, s, f, l, i, d, a);
	}
	else if (strncmp(buff, "**ERROR:", 8) == 0)
	{
		/* Error handling */
		quit_lavrec_with_error(buff+9);
	}
	else if (strncmp(buff, "   INFO: Opening output", 23) == 0)
	{
		sscanf(buff, "   INFO: Opening output file %s", buff2);
		sprintf(buff3, "Recording to %s...\nPress \"Stop\" to stop...", buff2);
		strcpy(files_recorded[current_file],buff2); current_file++;
		studio_set_lavrec_label(buff3);
	}
}

void create_child()
{
	char lavrec_i[4], lavrec_f[4], lavrec_d[4], lavrec_q[6], lavrec_a[5], lavrec_r[8];
	char lavrec_R[4], lavrec_c[4], lavrec_t[11], lavrec_T[4], lavrec_n[6], lavrec_b[8];
	char lavrec_g[20], lavrec_ff[6], lavrec_mfs[10];
	int n;
	char *lavrec_command[256];

	/* create command in lavrec_command */
	n=0;
	lavrec_command[n] = app_name(LAVREC); n++;
	lavrec_command[n] = "-i"; n++; sprintf(lavrec_i, "%c", input_source); lavrec_command[n] = lavrec_i; n++;
	lavrec_command[n] = single_frame ? "-S" : "-w"; n++;
	lavrec_command[n] = "-f"; n++; sprintf(lavrec_f, "%c", video_format); lavrec_command[n] = lavrec_f; n++;
	if (!software_encoding)
	{
		lavrec_command[n] = "-d"; n++;
		sprintf(lavrec_d, "%i", hordcm);
		lavrec_command[n] = lavrec_d; n++;
	}
	lavrec_command[n] = "-q"; n++; sprintf(lavrec_q, "%i", quality); lavrec_command[n] = lavrec_q; n++;
	lavrec_command[n] = "-a"; n++; sprintf(lavrec_a, "%i", audio_size); lavrec_command[n] = lavrec_a; n++;
	lavrec_command[n] = "-r"; n++; sprintf(lavrec_r, "%i", audio_rate); lavrec_command[n] = lavrec_r; n++;
	lavrec_command[n] = "-l"; n++; lavrec_command[n] = "-1"; n++;
	lavrec_command[n] = "-R"; n++; sprintf(lavrec_R, "%c", (char)audio_recsrc); lavrec_command[n]=lavrec_R;n++;
	lavrec_command[n] = "-c"; n++; sprintf(lavrec_c, "%i", sync_corr); lavrec_command[n] = lavrec_c; n++;
	lavrec_command[n] = "-t"; n++; sprintf(lavrec_t, "%i", record_time); lavrec_command[n] = lavrec_t; n++;
	lavrec_command[n] = "-T"; n++; sprintf(lavrec_T, "%i", time_lapse); lavrec_command[n] = lavrec_T; n++;
	lavrec_command[n] = "-n"; n++; sprintf(lavrec_n, "%i", MJPG_nbufs); lavrec_command[n] = lavrec_n; n++;
	lavrec_command[n] = "-b"; n++; sprintf(lavrec_b, "%i", MJPG_bufsize); lavrec_command[n] = lavrec_b; n++;
	if (audio_mute) { lavrec_command[n] = "-m"; n++; }
	if (stereo) { lavrec_command[n] = "-s"; n++; }
	if (software_encoding && encoding_syntax_style==150)
	{ lavrec_command[n] = "--software-encoding"; n++; }
	if (use_read && encoding_syntax_style == 150)
	{ lavrec_command[n] = "--use-read"; n++; }
	if (file_flush > 0 && encoding_syntax_style == 150)
	{
		lavrec_command[n] = "--file_flush"; n++;
		sprintf(lavrec_ff, "%d", file_flush);
		lavrec_command[n] = lavrec_ff; n++;
	}
	if (max_file_size > 0 && encoding_syntax_style == 150)
	{
		lavrec_command[n] = "--max-file-size"; n++;
		sprintf(lavrec_mfs, "%d", max_file_size);
		lavrec_command[n] = lavrec_mfs; n++;
	}
	if ((geom_width!=0 && geom_height!=0) && !software_encoding)
	{
		lavrec_command[n] = "-g"; n++;
		sprintf(lavrec_g, "%ix%i+%i+%i", geom_width, geom_height, geom_x, geom_y);
		lavrec_command[n] = lavrec_g; n++;
	}
	else if (software_encoding && encoding_syntax_style == 150)
	{
		lavrec_command[n] = "-g"; n++;
		sprintf(lavrec_g, "%ix%i", software_recwidth, software_recheight);
		lavrec_command[n] = lavrec_g; n++;
	}
	lavrec_command[n] = gtk_entry_get_text(GTK_ENTRY(textfield)); n++;
	lavrec_command[n] = NULL;

	start_pipe_command(lavrec_command, LAVREC);
}

void emulate_enter()
{
	if (pipe_is_active(LAVREC))
	{
		/*write(out_pipe, "\n", 1);*/
		write_pipe(LAVREC, "\n");
		studio_set_lavrec_label("\nPress \"Stop\" to stop...");
	}
}

void emulate_ctrl_c()
{
	close_pipe(LAVREC);
}

void lavrec_quit()
{
	studio_set_lavrec_label("Lavrec was stopped\nPress \"Initialize Capture\" to start");
	scene_detection();
}

void text_changed(GtkWidget *widget, gpointer data)
{
	record_dir = gtk_entry_get_text(GTK_ENTRY(textfield));
	save_config();
}

void reset_lavrec_tv()
{
	gtk_widget_set_usize(GTK_WIDGET(tv), tv_width_capture, tv_height_capture);
	gtk_tvplug_set(GTK_WIDGET(tv), "port", port);
}

void file_ok_sel1( GtkWidget *w, GtkFileSelection *fs )
{
	gtk_entry_set_text(GTK_ENTRY(textfield), gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
	sprintf(record_dir, "%s", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
	save_config();
}

void create_filesel1(GtkWidget *widget, gpointer data)
{
	GtkWidget *filew;
	
	filew = gtk_file_selection_new ("Linux Video Studio - Select Location");
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		"clicked", (GtkSignalFunc) file_ok_sel1, filew);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), gtk_entry_get_text(GTK_ENTRY(textfield)));
	gtk_widget_show(filew);
}

void write_result(GtkWidget *widget, gpointer data)
{
	if (pipe_is_active(LAVREC))
	{
		printf("Error, lavrec is already active!\n");
		return;
	}
	table_set_text(0, 0, 0, 0, 0, 0, 0, 0);
	current_file = 0;
	studio_set_lavrec_label("Initializing capture\nPlease wait...");
	create_child();
}

GtkWidget *create_buttons(GtkWidget *vbox, GtkWidget *window)
{
	GtkWidget *hbox, *label, *button, *pixmap_widget, *hbox2;
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();
	lavrec_label = gtk_label_new("Welcome to Linux Video Studio\nPress \"Initialize Capture\" to start");
	gtk_box_pack_start (GTK_BOX (vbox), lavrec_label, TRUE, TRUE, 10);
	gtk_widget_show (lavrec_label);

	/*Text field where to save the file (used to be in table)*/
	hbox = gtk_hbox_new(FALSE,10);
	label = gtk_label_new("Capture to: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	textfield = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(textfield), record_dir);
	gtk_signal_connect(GTK_OBJECT(textfield), "changed", GTK_SIGNAL_FUNC(text_changed), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), textfield, TRUE, TRUE, 0);
	gtk_widget_show(textfield);

	//button = gtk_button_new_with_label("Select");
	button = gtk_button_new();
	pixmap_widget = gtk_widget_from_xpm_data(file_widget_open_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Select Recording Location", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_filesel1), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
	gtk_widget_show(hbox);

	hbox = gtk_hbox_new(FALSE,25);

	init_button = gtk_button_new_with_label("Initialize capture");
	gtk_signal_connect(GTK_OBJECT(init_button), "clicked",
		GTK_SIGNAL_FUNC(write_result), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), init_button, TRUE, FALSE, 0);
	gtk_widget_show(init_button);

	start_button = gtk_button_new(); //_with_label("[Start]");
	hbox2 = gtk_hbox_new(FALSE,10);
	pixmap_widget = gtk_widget_from_xpm_data(editor_record_xpm);
	gtk_box_pack_start (GTK_BOX (hbox2), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, start_button, "Start Recording", NULL);
	label = gtk_label_new("Record");
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_container_add(GTK_CONTAINER(start_button), hbox2);
	gtk_widget_show(hbox2);
	gtk_signal_connect(GTK_OBJECT(start_button), "clicked",
		GTK_SIGNAL_FUNC(emulate_enter), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), start_button, TRUE, FALSE, 0);
	gtk_widget_set_usize(start_button, -1, 32);
	gtk_widget_show(start_button);

	stop_button = gtk_button_new(); //_with_label("[Stop]");
	hbox2 = gtk_hbox_new(FALSE,10);
	pixmap_widget = gtk_widget_from_xpm_data(editor_stop_xpm);
	gtk_box_pack_start (GTK_BOX (hbox2), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, stop_button, "Stop Recording", NULL);
	label = gtk_label_new("Stop");
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_container_add(GTK_CONTAINER(stop_button), hbox2);
	gtk_widget_show(hbox2);
	gtk_signal_connect(GTK_OBJECT(stop_button), "clicked",
		GTK_SIGNAL_FUNC(emulate_ctrl_c), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), stop_button, TRUE, FALSE, 0);
	gtk_widget_set_usize(stop_button, -1, 32);
	gtk_widget_show(stop_button);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
	gtk_widget_show(hbox);

	return vbox;
}

void create_lavrec_logtable(GtkWidget *table)
{
	GtkWidget *label;

	label = gtk_label_new("Audio Input: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	gtk_widget_show (label);
	table_label_ainput = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_ainput), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_ainput, 1, 2, 0, 1);
	gtk_widget_show (table_label_ainput);

	label = gtk_label_new("Video Input: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	gtk_widget_show (label);
	table_label_vinput = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_vinput), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_vinput, 1, 2, 1, 2);
	gtk_widget_show (table_label_vinput);

	label = gtk_label_new("Output Format: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	gtk_widget_show (label);
	table_label_format = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_format), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_format, 1, 2, 2, 3);
	gtk_widget_show (table_label_format);

	label = gtk_label_new("Recording Time: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);
	gtk_widget_show (label);
	table_label_time = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_time), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_time, 1, 2, 3, 4);
	gtk_widget_show (table_label_time);

	label = gtk_label_new("Lost Frames: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 4, 5);
	gtk_widget_show (label);
	table_label_lost = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_lost), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_lost, 1, 2, 4, 5);
	gtk_widget_show (table_label_lost);

	label = gtk_label_new("Inserted Frames: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 5, 6);
	gtk_widget_show (label);
	table_label_inserted = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_inserted), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_inserted, 1, 2, 5, 6);
	gtk_widget_show (table_label_inserted);

	label = gtk_label_new("Deleted Frames: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 6, 7);
	gtk_widget_show (label);
	table_label_deleted = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_deleted), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_deleted, 1, 2, 6, 7);
	gtk_widget_show (table_label_deleted);

	label = gtk_label_new("Audio Errors: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 7, 8);
	gtk_widget_show (label);
	table_label_aerrors = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(table_label_aerrors), 1.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), table_label_aerrors, 1, 2, 7, 8);
	gtk_widget_show (table_label_aerrors);

	table_set_text(0, 0, 0, 0, 0, 0, 0, 0);
}

void reset_audio()
{
	/* we should reset the audio slider here - for now, do nothing */
}

void audio_slider_changed(GtkAdjustment *adj, gpointer data)
{
	//printf("Value: %d, i = %d\n", 100 - (int)GTK_ADJUSTMENT(adj)->value, i);
	int i = -1, no_of_devs;

	if (mixer_id > 0)
	{
		no_of_devs = mixer_num_of_devs (mixer_id);
		for (i = 0; i < no_of_devs; i++)
		{
			if (strcmp(mixer_get_label(mixer_id, i), "Line ") == 0 && audio_recsrc == 'l')
			{
				break;
			}
			else if (strcmp(mixer_get_label(mixer_id, i), "Mic  ") == 0 && audio_recsrc == 'm')
			{
				break;
			}
			else if (strcmp(mixer_get_label(mixer_id, i), "CD   ") == 0 && audio_recsrc == 'c')
			{
				break;
			}
		}
	}

	if (mixer_id > 0)
	{
		mixer_set_vol_left (mixer_id, i, 100 - (int)GTK_ADJUSTMENT(adj)->value);
		if (mixer_is_stereo (mixer_id, i))
			mixer_set_vol_right (mixer_id, i, 100 - (int)GTK_ADJUSTMENT(adj)->value);
	}
}

GtkWidget *create_audio_sliders()
{
	GtkWidget *vbox, *scrollbar, *label, *pixmap_w;
	GtkObject *adj;
	GtkTooltips *tooltip;
	int no_of_devs, i = -1, value;

	tooltip = gtk_tooltips_new();
	vbox = gtk_vbox_new (FALSE, 0);

	mixer_id = mixer_init (getenv("LAV_MIXER_DEV"));
	if (mixer_id > 0)
	{
		no_of_devs = mixer_num_of_devs (mixer_id);
		for (i = 0; i < no_of_devs; i++)
		{
			if (strcmp(mixer_get_label(mixer_id, i), "Line ") == 0 && audio_recsrc == 'l')
			{
				break;
			}
			else if (strcmp(mixer_get_label(mixer_id, i), "Mic  ") == 0 && audio_recsrc == 'm')
			{
				break;
			}
			else if (strcmp(mixer_get_label(mixer_id, i), "CD   ") == 0 && audio_recsrc == 'c')
			{
				break;
			}
		}
	}

	value = 100 - mixer_get_vol_left(mixer_id, i);

	label = gtk_label_new("\n");
	gtk_box_pack_start(GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	adj = gtk_adjustment_new(value, 0, 100, 1, 10, 0);
	gtk_signal_connect(adj, "value_changed", GTK_SIGNAL_FUNC(audio_slider_changed), NULL);
	scrollbar = gtk_vscale_new(GTK_ADJUSTMENT (adj));
	gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 0);
	gtk_box_pack_start(GTK_BOX (vbox), scrollbar, TRUE, TRUE, 0);
	gtk_widget_show(scrollbar);

	gtk_tooltips_set_tip(tooltip, scrollbar, "Volume", NULL);
	pixmap_w = gtk_widget_from_xpm_data(slider_volume_xpm);
	gtk_widget_show(pixmap_w);
	gtk_box_pack_start(GTK_BOX(vbox), pixmap_w, FALSE, FALSE, 0);

	label = gtk_label_new("\n");
	gtk_box_pack_start(GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	return vbox;
}

void video_slider_changed(GtkAdjustment *adj, char *what)
{
	gtk_tvplug_set(tv, what, adj->value);
}

GtkWidget *create_video_sliders()
{
   GtkWidget *hbox, *vbox, *pixmap, *scrollbar;
   GtkTooltips *tooltip;
   int i=0;
   char *titles[4] = {
      "Hue",
      "Contrast",
      "Brightness",
      "Colour Saturation"
   };
   char *names[4] = {
      "hue",
      "contrast",
      "brightness",
      "colour"
   };
   GtkAdjustment *adj[4] = {
      GTK_TVPLUG(tv)->hue_adj,
      GTK_TVPLUG(tv)->contrast_adj,
      GTK_TVPLUG(tv)->brightness_adj,
      GTK_TVPLUG(tv)->saturation_adj,
   };
   char **pixmaps[4] = {
      slider_hue_xpm,
      slider_contrast_xpm,
      slider_brightness_xpm,
      slider_sat_colour_xpm,
   };

   tooltip = gtk_tooltips_new();
   hbox = gtk_hbox_new(TRUE, 20);

   for (i=0;i<4;i++)
   {
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_object_ref(GTK_OBJECT(adj[i]));
      gtk_signal_connect(GTK_OBJECT(adj[i]), "value_changed",
         GTK_SIGNAL_FUNC(video_slider_changed), names[i]);
      scrollbar = gtk_vscale_new(adj[i]);
      gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 0);
      gtk_box_pack_start(GTK_BOX (vbox), scrollbar, TRUE, TRUE, 10);
      gtk_widget_show(scrollbar);
      gtk_widget_set_usize(scrollbar, -1, 150);
      gtk_tooltips_set_tip(tooltip, scrollbar, titles[i], NULL);
      pixmap = gtk_widget_from_xpm_data(pixmaps[i]);
      gtk_widget_show(pixmap);
      gtk_box_pack_start(GTK_BOX(vbox), pixmap, FALSE, FALSE, 10);
      gtk_box_pack_start(GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
      gtk_widget_show(vbox);
   }

   return hbox;
}


GtkWidget *create_lavrec_layout(GtkWidget *window)
{
	GtkWidget *vbox, *hbox, *hbox2, *hbox3, *vbox2, *table;

	vbox = gtk_vbox_new(FALSE,0);
	hbox = gtk_hbox_new(FALSE,20);
	hbox2 = gtk_hbox_new(FALSE,20);

	tv = gtk_tvplug_new(port);
	if (port == -1)
		exit(1);
	else if (port == 0 && tv != NULL)
	{
		port = GTK_TVPLUG (tv)->port;
		save_config();
	}

	if (tv)
 	{
		hbox3 = create_video_sliders();
		gtk_box_pack_start (GTK_BOX (hbox2), hbox3, TRUE, FALSE, 10);
		gtk_widget_show(hbox3);
	}

	if (tv == NULL)
	{
		//return NULL;
		tv = gtk_event_box_new();
		set_background_color(tv, 0,0,0);
	}
	gtk_widget_set_usize(GTK_WIDGET(tv), tv_width_capture, tv_height_capture);
	gtk_box_pack_start (GTK_BOX (hbox2), tv, TRUE, FALSE, 10);
	gtk_widget_show(tv);

	vbox2 = create_audio_sliders();
	gtk_box_pack_start (GTK_BOX (hbox2), vbox2, TRUE, FALSE, 10);
	gtk_widget_show(vbox2);

	gtk_box_pack_start (GTK_BOX (vbox), hbox2, TRUE, TRUE, 20);
	gtk_widget_show(hbox2);

	vbox2 = create_buttons(gtk_vbox_new(FALSE, 0), window);
	hbox2 = gtk_hbox_new(FALSE, 20);
	gtk_box_pack_start (GTK_BOX (hbox2), vbox2, TRUE, TRUE, 20);
	gtk_widget_show(vbox2);

	table = gtk_table_new (2,9, FALSE);
	create_lavrec_logtable(table);
	gtk_box_pack_start (GTK_BOX (hbox2), table, TRUE, TRUE, 20);
	gtk_widget_show(table);

	gtk_box_pack_start (GTK_BOX (vbox), hbox2, TRUE, TRUE, 20);
	gtk_widget_show(hbox2);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, FALSE, 20);
	gtk_widget_show(vbox);

	return hbox;
}
