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

#include "lavedit.h"
#include "pipes.h"
#include "gtkenhancedscale.h"
#include "gtkfunctions.h"

#include "editor_play.xpm"
#include "editor_play_i.xpm"
#include "editor_pause.xpm"
#include "editor_step.xpm"
#include "editor_step_i.xpm"
#include "editor_fast.xpm"
#include "editor_fast_i.xpm"

extern int verbose;
GtkWidget *tv_trimming, *pop_window;
char command_to_lavplay_trimming[256];
GtkObject *popup_adj[3];

void command_to_lavplay_trimming_set(GtkWidget *widget, char *data);
void lavplay_enhanced_slider_value_changed(GtkAdjustment *adj, gpointer data);
static GtkWidget *create_tv_stuff_for_trimming(GtkWidget *window);
void lavplay_enhanced_slider_value_changed(GtkAdjustment *adj, gpointer data);
void create_lavplay_trimming_child(void);
void quit_trimming(GtkWidget *widget, gpointer data);
int save_trimming_file(char *target);
void save_trimming_changes(GtkWidget *widget, gpointer data);
void lavplay_trimming_callback(char *msg);
void set_lavplay_trimming_log(char norm, int cur_pos, int total);
void quit_lavplay_trimming_with_error(char *msg);

/* for lavedit.c */
void create_lavplay_edit_child();

/* ============================================================= */

static GtkWidget *create_tv_stuff_for_trimming(GtkWidget *window)
{
	GtkWidget *vbox2, *hbox3, *pixmap_widget, *scrollbar, *button;
/*	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkStyle *style;*/
	GtkTooltips *tooltip;
	int i;
	extern int tv_width_edit, tv_height_edit;

	tooltip = gtk_tooltips_new();
/*	style = gtk_widget_get_style(window);*/

	vbox2 = gtk_vbox_new(FALSE,0);

	tv_trimming = gtk_event_box_new();
	gtk_widget_set_usize(GTK_WIDGET(tv_trimming), tv_width_edit, tv_height_edit);
	gtk_box_pack_start (GTK_BOX (vbox2), tv_trimming, FALSE, FALSE, 0);
	gtk_widget_show(tv_trimming);

	set_background_color(tv_trimming,0,0,0);

	popup_adj[0] = gtk_adjustment_new(GTK_IMAGEPLUG(image[current_image])->startscene -
		GTK_IMAGEPLUG(image[current_image])->startframe,
		0, GTK_IMAGEPLUG(image[current_image])->stopscene -
		GTK_IMAGEPLUG(image[current_image])->startscene,1, 10, 0);
	popup_adj[1] = gtk_adjustment_new(0,
		0, GTK_IMAGEPLUG(image[current_image])->stopscene -
		GTK_IMAGEPLUG(image[current_image])->startscene,1, 10, 0);
	popup_adj[2] = gtk_adjustment_new(GTK_IMAGEPLUG(image[current_image])->stopscene -
		GTK_IMAGEPLUG(image[current_image])->startscene,
		0, GTK_IMAGEPLUG(image[current_image])->stopscene -
		GTK_IMAGEPLUG(image[current_image])->startscene,1, 10, 0);
	for (i=0;i<3;i++)
		gtk_signal_connect (GTK_OBJECT (popup_adj[i]), "value_changed",
			GTK_SIGNAL_FUNC (lavplay_enhanced_slider_value_changed), (gpointer) i);
	scrollbar = gtk_enhanced_scale_new(popup_adj,3);

	gtk_box_pack_start(GTK_BOX (vbox2), scrollbar, FALSE, FALSE, 10);
	gtk_widget_show(scrollbar);

	hbox3 = gtk_hbox_new(TRUE, 20);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_fast_i_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_fast_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Fast Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(command_to_lavplay_trimming_set),"p-3");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_play_i_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_play_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(command_to_lavplay_trimming_set),"p-1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_step_i_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_step_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "One Frame Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(command_to_lavplay_trimming_set),"-");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_pause_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_pause_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Pause", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(command_to_lavplay_trimming_set),"p0");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_step_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_step_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "One Frame Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(command_to_lavplay_trimming_set),"+");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_play_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_play_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(command_to_lavplay_trimming_set),"p1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_fast_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_fast_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Fast Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(command_to_lavplay_trimming_set),"p3");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 10);
	gtk_widget_show(hbox3);

	return vbox2;
}

void set_background_color(GtkWidget *widget, int r, int g, int b)
{
	GtkRcStyle* rc_style;

	rc_style = gtk_rc_style_new();
	rc_style->bg[GTK_STATE_NORMAL].red = r;
	rc_style->bg[GTK_STATE_NORMAL].green = g;
	rc_style->bg[GTK_STATE_NORMAL].blue = b;
	rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_BG;
	gtk_widget_modify_style(widget,rc_style);
	gtk_rc_style_unref(rc_style);
}

void open_frame_edit_window()
{
	GtkWidget *button, *vbox, *hbox, *vbox2;
	char temp1[256];

	/* first, close the original lavplay */
	close_pipe(LAVPLAY_E);

	pop_window = gtk_window_new(GTK_WINDOW_DIALOG);

	vbox = gtk_vbox_new(FALSE, 5);

	gtk_window_set_title (GTK_WINDOW(pop_window), "Linux Video Studio - Scene Trimming");
	gtk_container_set_border_width (GTK_CONTAINER (pop_window), 20);
	gtk_widget_realize(pop_window);

	vbox2 = create_tv_stuff_for_trimming(pop_window);
	gtk_box_pack_start (GTK_BOX (vbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show (vbox2);

	hbox = gtk_hbox_new(TRUE, 20);

	button = gtk_button_new_with_label("OK");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(save_trimming_changes), NULL);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(quit_trimming), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE,TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("Cancel");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(quit_trimming), NULL);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE,TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_container_add (GTK_CONTAINER (pop_window), vbox);
	gtk_widget_show(vbox);

	gtk_grab_add(pop_window);
	gtk_widget_show(pop_window);

	/* make temp editlist and start lavplay */
	sprintf(temp1, "%s/.studio/%s", getenv("HOME"), "trimming.eli");
	if (save_trimming_file(temp1))
		create_lavplay_trimming_child();
	else
		return;
}

int save_trimming_file(char *target)
{
	FILE *fp;

	fp = fopen(target,"w");

	if (NULL == fp)
	{
		fprintf(stderr,"can't open editlist file %s\n",target);
		return 0;
	}

	fprintf(fp,"LAV Edit List\n");
	if (pal_or_ntsc == 'p')
		fprintf(fp,"PAL\n");
	else
		fprintf(fp,"NTSC\n");
	fprintf(fp, "1\n");
	fprintf(fp, "%s\n", GTK_IMAGEPLUG(image[current_image])->video_filename);
	fprintf(fp, "%d %d %d\n", 0,
		GTK_IMAGEPLUG(image[current_image])->startscene,
		GTK_IMAGEPLUG(image[current_image])->stopscene);

	fclose(fp);

	/* set the min/max frame params */
	gtk_adjustment_set_value(GTK_ADJUSTMENT(popup_adj[1]),
		GTK_IMAGEPLUG(image[current_image])->startframe -
		GTK_IMAGEPLUG(image[current_image])->startscene);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(popup_adj[2]),
		GTK_IMAGEPLUG(image[current_image])->stopframe -
		GTK_IMAGEPLUG(image[current_image])->startscene);

	return 1;
}

void lavplay_enhanced_slider_value_changed(GtkAdjustment *adj, gpointer data)
{
	if (!adjustment_b_changed)
	{
		if ((int)data == 0)
		{
			sprintf(command_to_lavplay_trimming,"s%d",(int)(adj->value));
		}
		else
		{
			if(GTK_ADJUSTMENT(popup_adj[1])->value > GTK_ADJUSTMENT(popup_adj[2])->value)
			{
				sprintf(command_to_lavplay_trimming,"es %d %d\n",
					(int)(GTK_ADJUSTMENT(popup_adj[2])->value),
					(int)(GTK_ADJUSTMENT(popup_adj[1])->value));
			}
			else
			{
				sprintf(command_to_lavplay_trimming,"es %d %d\n",
					(int)(GTK_ADJUSTMENT(popup_adj[1])->value),
					(int)(GTK_ADJUSTMENT(popup_adj[2])->value));
			}
		}
	}
	else
		adjustment_b_changed = FALSE;
}

void create_lavplay_trimming_child()
{
	char *lavplay_command[256];
	char temp1[256];
	int n;
	char SDL_windowhack[32];

	sprintf(SDL_windowhack,"%ld",GDK_WINDOW_XWINDOW(tv_trimming->window));
	setenv("SDL_WINDOWID", SDL_windowhack, 1);

	n=0;
	lavplay_command[n] = LAVPLAY_LOCATION; n++;
	lavplay_command[n] = "-q"; n++;
	lavplay_command[n] = "-g"; n++;
	lavplay_command[n] = "-v"; n++;
	lavplay_command[n] = "1"; n++;
	lavplay_command[n] = "-S"; n++;
	sprintf(temp1, "%s/.studio/%s", getenv("HOME"), "trimming.eli");
	lavplay_command[n] = temp1; n++;
	lavplay_command[n] = NULL;

	start_pipe_command(lavplay_command, LAVPLAY_T); /* lavplay */
}

void save_trimming_changes(GtkWidget *widget, gpointer data)
{
	int diff, i, new_start, new_stop;

	/* save trim-values */
	if(GTK_ADJUSTMENT(popup_adj[1])->value > GTK_ADJUSTMENT(popup_adj[2])->value)
	{


		new_start =
			GTK_IMAGEPLUG(image[current_image])->startscene +
			GTK_ADJUSTMENT(popup_adj[2])->value;
		new_stop =
			GTK_IMAGEPLUG(image[current_image])->startscene +
			GTK_ADJUSTMENT(popup_adj[1])->value;
	}
	else
	{
		new_start =
			GTK_IMAGEPLUG(image[current_image])->startscene +
			GTK_ADJUSTMENT(popup_adj[1])->value;
		new_stop =
			GTK_IMAGEPLUG(image[current_image])->startscene +
			GTK_ADJUSTMENT(popup_adj[2])->value;
	}

	diff = new_stop - new_start - 
		GTK_IMAGEPLUG(image[current_image])->stopframe +
		GTK_IMAGEPLUG(image[current_image])->startframe;

	GTK_IMAGEPLUG(image[current_image])->startframe = new_start;
	GTK_IMAGEPLUG(image[current_image])->stopframe = new_stop;

	if (diff != 0 && current_image < number_of_files - 1)
		for (i=current_image;i<number_of_files;i++)
			GTK_IMAGEPLUG(image[i])->start_total += diff;

	save_eli_temp_file();
}

void quit_trimming(GtkWidget *widget, gpointer data)
{
	close_pipe(LAVPLAY_T);

	gtk_widget_destroy(pop_window);

	create_lavplay_edit_child();
}

void lavplay_trimming_callback(char *msg)
{
	if(msg[0]=='@')
	{
		char norm;
		int cur_pos,total,speed;

		sscanf(msg+1,"%c%d/%d/%d",&norm,&cur_pos,&total,&speed);
		set_lavplay_trimming_log(norm, cur_pos,total);

		/* give command to lavplay if available */
		if (strcmp(command_to_lavplay_trimming, "") != 0)
		{
			write_pipe(LAVPLAY_T, command_to_lavplay_trimming);
			command_to_lavplay_trimming[0] = '\0';
		}

		return;
	}

	else if (strncmp(msg, "**Error:", 8) == 0)
	{
		/* Error handling */
		quit_lavplay_trimming_with_error(msg);
	}
}

void set_lavplay_trimming_log(char norm, int cur_pos, int total)
{
	/*int f, h, m, s;
	char temp[256];

	s = cur_pos/(norm=='p'?25:30);
	f = cur_pos%(norm=='p'?25:30);
	m = s/60;
	s = s%60;
	h = m/60;
	m = m%60;*/

	adjustment_b_changed = TRUE;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(popup_adj[0]), cur_pos);

	/*sprintf(temp, "%d/%d", cur_pos, total);
	gtk_label_set_text(GTK_LABEL(label_trim_frames), temp);*/

	/*sprintf(temp, "%2d:%2.2d:%2.2d:%2.2d",h,m,s,f); 
	gtk_label_set_text(GTK_LABEL(label_trim_time), temp);*/
}

void quit_lavplay_trimming_with_error(char *msg)
{
	quit_trimming(NULL,NULL);
	if (verbose) g_print("Lavplay error: %s\n", msg);
	gtk_show_text_window(STUDIO_ERROR, "Lavplay returned an error:", msg);
}

void command_to_lavplay_trimming_set(GtkWidget *widget, char *data)
{
	sprintf(command_to_lavplay_trimming, "%s\n", data);
}
