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

 * Michael Hanke, September 6, 2003
 *   - correction for still effects with interlaced material
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
#include <math.h>
#include <errno.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "studio.h"  /* for aspect ratio */
#include "lavedit.h"
#include "gtkfunctions.h"
#include "pipes.h"
#include "gtkenhancedscale.h"

#include "editor_play.xpm"
#include "editor_stop.xpm"
#include "file_widget_open.xpm"

#include "effect_text.xpm"
#include "effect_picture.xpm"
#include "effect_transition.xpm"
#include "scene_screenshot.xpm"

#include "gnome-color-browser.xpm"
#include "gnome-fontsel.xpm"

#define min(a,b) ((a) < (b) ? (a) : (b))

/* structs for the various effect utilities here */
struct scene_transition_options {
	GtkTransitionType type;	/* the name of the effect used */
	int length;		/* length of the scene transition */
	int value_start;	/* beginning value for transition (blend) */
	int value_end;		/* end value for transition (blend) */
	int num_rows;		/* number of wipe rows (wipe) */
	int reposition_first;	/* whether to reposition scene 1 (wipe) */
	int reposition_second;	/* whether to reposition scene 2 (wipe) */
	int orig_pos_x;		/* point of origin (X, overlay) */
	int orig_pos_y;		/* point of origin (Y, overlay) */
	int scaling;		/* whether to scale the stream (overlay) */
	char scene1_file[256];
	int scene1_start;	/* filename, start/end framenum for scene 1 */
	int scene1_stop;
	char scene2_file[256];
	int scene2_start;	/* filename, start/end framenum for scene 1 */
	int scene2_stop;

	GtkWidget *fs;
};

struct image_overlay_options {
	char scene_file[256];	/* Filename of the movie */
	int scene_start;	/* start of the movie scene */
	int scene_end;		/* end of the movie scene */
	GtkWidget *image_file_textbox;	/* Filename of the image */
	char text[256];
	int length;		/* time length that the image should be shown */
	int offset;		/* offset from beginning of scene where image should appear */
	int image_width;	/* height of the image */
	int image_height;	/* width of the image */
	int image_x;		/* x-position of the image over the movie */
	int image_y;		/* y-position of the image over the movie */
	int movie_width;	/* width of the movie */
	int movie_height;	/* height of the movie */
	GdkPixbuf *image;	/* the _original_ gdk-pixbuf of the image (unscaled!) */
	int opacity;		/* percentage that the image is visible (0-100%) */
	GtkEffectType type;	/* GTK_EFFECT_TEXT or GTK_EFFECT_IMAGE */
	char *font;		/* in case of GTK_EFFECT_TEXT, the font to be used */
        GdkFont *fontdesc;       /* Gdk Font Structure */
	gdouble colors[3];	/* in case of GTK_EFFECT_TEXT, the color to be used for the font */

	GtkWidget *fs;		/* stupid $%@$%@# needs this object */
	GtkObject *adj[2];	/* stupid $$ global var for length/offset slider */
	GtkWidget *image_position[4];	/* widgets for positioning the image (w/h/x-,y-offset) */
  int aspect_den;         /* aspect ratio: denominator */
  int aspect_enu;         /* aspect ratio: enumerator */
        char interlacing_info;        /* interlaced material */
};

/* variables */
extern int verbose;
GtkWidget *lavpipe_preview_tv;
int preview_or_render; /* 0=preview, 1=render */
char renderfile[256], soundfile[256];
static GtkWidget *render_button_label = NULL, *render_progress_label = NULL, *render_progress_status_label = NULL;
static GtkObject *render_progress_adj = NULL;
static GtkWidget *progress_label = NULL;
static int num_frames = 0;
static GtkWidget *transition_selection_box[3];

/* size of preview-tv-window */
int lavedit_effects_preview_width; /* = 240; set dynamically */
int lavedit_effects_preview_height = 180;

/* forward declarations */
void rgb_to_yuv(int width, int height, int rowstride, guchar *rgb, guchar **yuv, int has_alpha,
	int x_offset, int y_offset, int image_w, int image_h);
void alpha_to_yuv(int width, int height, int rowstride, guchar *rgba, guchar **yuv, int has_alpha,
	int x_offset, int y_offset, int image_w, int image_h, int opacity);

void effects_finished(void);
void effects_callback(int number, char *msg);
GtkWidget *effects_tv(gpointer options, GtkSignalFunc play);

void scene_transition_type_selected(GtkWidget *widget, gpointer data);
void scene_transition_button_toggled(GtkWidget *w, int *i);
void scene_transition_textbox_changed(GtkWidget *widget, int *i);
void stop_rendering_process(GtkWidget *w, gpointer data);
void create_progress_window(int length);
void start_lavpipe_render(char *lavpipe_file, char *result_file);
void start_lavpipe_preview(char *file);
void play_scene_transition(GtkWidget *widget, gpointer data);
void play_text_overlay(GtkWidget *widget, gpointer data);
void write_yuv_to_file(FILE *fd, guchar **yuv, int width, int height);
void play_image_overlay(GtkWidget *widget, gpointer data);
void stop_scene_transition(GtkWidget *widget, gpointer data);
void scene_transition_adj_changed(GtkAdjustment *adj, int *i);
void effects_scene_transition_accept(GtkWidget *widget, gpointer data);
void effects_scene_transition_show_window(struct scene_transition_options *options);

void overlay_render_file_selected(GtkWidget *w, gpointer data);
void scene_transition_render_file_selected(GtkWidget *w, gpointer data);
void image_overlay_file_load(GtkWidget *w, gpointer data);
void select_image_overlay_file(GtkWidget *widget, gpointer data);
void effects_image_overlay_accept(GtkWidget *widget, gpointer data);
void image_overlay_adj_changed(GtkAdjustment *adj, gpointer data);
void image_overlay_textbox_changed(GtkWidget *widget, int *i);
void effects_image_overlay_show_window(struct image_overlay_options *options);

void text_overlay_font_load(GtkWidget *w, gpointer data);
void select_text_overlay_font(GtkWidget *w, gpointer data);
void text_overlay_color_load(GtkWidget *w, gpointer data);
void select_text_overlay_color(GtkWidget *w, gpointer data);
void text_overlay_textbox_changed(GtkWidget *w, gpointer data);

void lavedit_effects_create_scene_transition(GtkWidget *widget, gpointer data);
void lavedit_effects_create_overlay(GtkWidget *widget, char *data);

/* ================================================================= */

void effects_finished()
{
  int stl;
	if (preview_or_render)
	{
		/* lavpipe | yuv2lav */
		if (soundfile[0] != '\0')
		{
			char command[256], file[256], new_renderfile[256];

			if (render_progress_label)
				gtk_label_set_text(GTK_LABEL(render_progress_label), "Creating sound");
			if (render_progress_status_label)
				gtk_label_set_text(GTK_LABEL(render_progress_status_label), "...");

			sprintf(file, "/tmp/.sound.wav");
			sprintf(command, "\"%s\" \"%s\" > \"%s\"%s",
				app_location(LAV2WAV), soundfile, file,
				verbose?"":" 2>/dev/null");
			system(command);

			if (render_progress_label)
				gtk_label_set_text(GTK_LABEL(render_progress_label), "Adding sound");
			if (render_progress_status_label)
				gtk_label_set_text(GTK_LABEL(render_progress_status_label), "......");

			strcpy(new_renderfile, renderfile);
			/* Need to add one character: avoid buffer overflow */
			stl = strlen(renderfile);
			if (stl < 255) {
			  new_renderfile[stl-4] = '~';
			  strcpy(new_renderfile+stl-3,renderfile+stl-4);
			}
			else new_renderfile[stl-5] = new_renderfile[stl-5] == '~'? '#' : '~';
			sprintf(command, "\"%s\" \"%s\" \"%s\" \"%s\"%s",
				app_location(LAVADDWAV), renderfile, file, new_renderfile,
				verbose?"":" > /dev/null 2>&1");
			system(command);

			unlink(file);
			unlink(renderfile);
			rename(new_renderfile,renderfile);

			soundfile[0] = '\0';
		}

		if (render_button_label)
			gtk_label_set_text(GTK_LABEL(render_button_label), " Close ");
		if (render_progress_label)
			gtk_label_set_text(GTK_LABEL(render_progress_label), "Finished!");
	}
	else
	{
		/* lavpipe | yuvplay */
		gtk_label_set_text(GTK_LABEL(progress_label), "Frame 0/0");
	}
}

void effects_callback(int number, char *msg)
{
	if (preview_or_render)
	{
		/* lavpipe | yuv2lav */
		if (number == YUV2LAV)
		{
			int framenum;
			if ((msg=strstr(msg, "frame"))!=NULL)
			{
				sscanf(msg, "frame %d", &framenum);
				if (render_progress_adj)
					gtk_adjustment_set_value(GTK_ADJUSTMENT(render_progress_adj), framenum);
				if (render_progress_status_label)
					gtk_label_set_text(GTK_LABEL(render_progress_status_label), msg);
			}
		}
	}
	else
	{
		/* lavpipe | yuvplay */
		if (number == YUVPLAY_E)
		{
			if (strncmp(msg, "Playing frame ", 14)==0)
			{
				int n;
				char temp[256];
				sscanf(msg+14, "%d", &n);
				sprintf(temp, "Frame %d/%d", n, num_frames);
				if (progress_label)
					gtk_label_set_text(GTK_LABEL(progress_label),
						temp);
			}
		}
	}
}

void stop_rendering_process(GtkWidget *w, gpointer data)
{
	/* cancel or close has been pressed, destroy the window */
	GtkWidget *window = (GtkWidget*)data;

	render_progress_adj = NULL;
	render_button_label = NULL;
	render_progress_label = NULL;
	render_progress_status_label = NULL;

	if (pipe_is_active(LAVPIPE))
	{
		/* close the pipes - TODO: addwav */
		close_pipe(LAVPIPE);
	}
	else
	{
		/* we were already done - so the movie is there, place it where it belongs */
		// TODO
	}

	gtk_widget_destroy(window);
}

void create_progress_window(int length)
{
	/* length of the effect (transition/overlay) in frames */

	GtkWidget *window, *button, *hbox, *vbox, *label, *render_progress_bar;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title (GTK_WINDOW(window), "Linux Video Studio - Video Rendering Status");
	gtk_container_set_border_width (GTK_CONTAINER (window), 20);

	vbox = gtk_vbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new ("Status: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE,FALSE, 0);
	gtk_widget_show (label);
	render_progress_label = gtk_label_new ("Starting up...");
	gtk_misc_set_alignment(GTK_MISC(render_progress_label), 0.0, GTK_MISC(render_progress_label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), render_progress_label, TRUE, TRUE, 0);
	gtk_widget_show (render_progress_label);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	render_progress_status_label = gtk_label_new(" ");
	gtk_box_pack_start (GTK_BOX (vbox), render_progress_status_label, FALSE,FALSE, 10);
	gtk_widget_show (render_progress_status_label);

	render_progress_bar = gtk_progress_bar_new();
	gtk_box_pack_start (GTK_BOX (vbox), render_progress_bar, FALSE, FALSE, 0);
	gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(render_progress_bar), GTK_PROGRESS_LEFT_TO_RIGHT);
	gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(render_progress_bar), GTK_PROGRESS_CONTINUOUS);
	gtk_progress_set_show_text(GTK_PROGRESS(render_progress_bar), 1);
	gtk_progress_set_format_string(GTK_PROGRESS(render_progress_bar), "%p\%");
	render_progress_adj = gtk_adjustment_new(0,0,length,1,1,0);
	gtk_progress_set_adjustment(GTK_PROGRESS(render_progress_bar), GTK_ADJUSTMENT(render_progress_adj));
	gtk_widget_show(render_progress_bar);

	button = gtk_button_new();
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		(GtkSignalFunc)stop_rendering_process, (gpointer)window);
	render_button_label = gtk_label_new(" Cancel ");
	gtk_container_add (GTK_CONTAINER (button), render_button_label);
	gtk_widget_show(render_button_label);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show(vbox);

	gtk_grab_add(window);
	gtk_widget_show(window);
}

GtkWidget *effects_tv(gpointer options, GtkSignalFunc play)
{
	GtkWidget *vbox2, *button, *hbox2;

	vbox2 = gtk_vbox_new(FALSE, 5);

	/* tv preview window */
	lavpipe_preview_tv = gtk_event_box_new();
	gtk_widget_set_usize(GTK_WIDGET(lavpipe_preview_tv),
		lavedit_effects_preview_width, lavedit_effects_preview_height);
	gtk_box_pack_start (GTK_BOX (vbox2), lavpipe_preview_tv, TRUE, FALSE, 0);
	gtk_widget_show (lavpipe_preview_tv);
	set_background_color(lavpipe_preview_tv,0,0,0);

	/* a play and a stop button in a new hbox */
	hbox2 = gtk_hbox_new(FALSE, 10);

	button = gtk_image_label_button(NULL,
				"Stop Preview Video Stream",
				editor_stop_xpm, 0, GTK_POS_BOTTOM);
	gtk_widget_set_usize(button, 32, 32);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(stop_scene_transition), NULL);
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE,FALSE, 0);
	gtk_widget_show (button);

	button = gtk_image_label_button(NULL,
				"Preview the Scene Transition",
				editor_play_xpm, 0, GTK_POS_BOTTOM);
	gtk_widget_set_usize(button, 32, 32);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		play, (gpointer)options);
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE,FALSE, 0);
	gtk_widget_show (button);

	progress_label = gtk_label_new("Frame 0/0");
	gtk_box_pack_start (GTK_BOX (hbox2), progress_label, TRUE,FALSE, 0);
	gtk_widget_show (progress_label);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, TRUE, FALSE, 0);
	gtk_widget_show(hbox2); 

	return vbox2;
}

void start_lavpipe_render(char *lavpipe_file, char *result_file)
{
	char *yuv2lav_command[256], *lavpipe_command[256];
	char temp1[10], temp2[10], temp3[10];
	int n;

	/* variables normally used for recording - let's use it for yuv2lav too */
	extern int quality, MJPG_bufsize;
	extern char video_format;

	n = 0;
	yuv2lav_command[n] = app_name(YUV2LAV); n++;
	sprintf(temp1, "%c", video_format);
	yuv2lav_command[n] = "-f"; n++;
	yuv2lav_command[n] = temp1; n++;
	sprintf(temp2, "%d", quality);
	yuv2lav_command[n] = "-q"; n++;
	yuv2lav_command[n] = temp2; n++;
	yuv2lav_command[n] = "-v"; n++;
	yuv2lav_command[n] = "2"; n++;
	sprintf(temp3, "%d", MJPG_bufsize);
	yuv2lav_command[n] = "-b"; n++;
	yuv2lav_command[n] = temp3; n++;
	yuv2lav_command[n] = "-o"; n++;
	yuv2lav_command[n] = result_file; n++;
	yuv2lav_command[n] = NULL;
	start_pipe_command(yuv2lav_command, YUV2LAV); /* yuv2lav */

	n = 0;
	lavpipe_command[n] = app_name(LAVPIPE); n++;
	lavpipe_command[n] = lavpipe_file; n++;
	lavpipe_command[n] = NULL;
	start_pipe_command(lavpipe_command, LAVPIPE); /* lavpipe */

	if (render_progress_label)
		gtk_label_set_text(GTK_LABEL(render_progress_label), "Rendering video: lavpipe | yuv2lav");
}

void start_lavpipe_preview(char *file)
{
	char *yuvplay_command[256], *lavpipe_command[256];
	char SDL_windowhack[32], temp[16];
	int n;

	sprintf(SDL_windowhack,"%ld",GDK_WINDOW_XWINDOW(lavpipe_preview_tv->window));
	setenv("SDL_WINDOWID", SDL_windowhack, 1);

	if (verbose) printf("Playing %s in lavpipe | yuvplay\n", file);

	n = 0;
	yuvplay_command[n] = app_name(YUVPLAY_E); n++;
	yuvplay_command[n] = "-s"; n++;
	sprintf(temp, "%dx%d", lavedit_effects_preview_width, lavedit_effects_preview_height);
	yuvplay_command[n] = temp; n++;
	yuvplay_command[n] = NULL;
	start_pipe_command(yuvplay_command, YUVPLAY_E); /* yuvplay */

	n = 0;
	lavpipe_command[n] = app_name(LAVPIPE); n++;
	lavpipe_command[n] = file; n++;
	lavpipe_command[n] = NULL;
	start_pipe_command(lavpipe_command, LAVPIPE); /* lavpipe */
}

void play_scene_transition(GtkWidget *widget, gpointer data)
{
	/* write a lavpipe .pli list and start lavpipe | yuvplay */
	FILE *fd;
	char file[256];

	struct scene_transition_options *options = (struct scene_transition_options*)data;

	/* let's start writing an editlist */
	sprintf(file, "%s/.studio/effect.pli", getenv("HOME"));
	fd = fopen(file, "w");
	if (fd == NULL)
	{
		gtk_show_text_window(STUDIO_ERROR, "Error opening \'%s\': %s",
			file, strerror(errno));
		return;
	}

	fprintf(fd, "LAV Pipe List\n");
	fprintf(fd, "%s\n", GTK_SCENELIST(scenelist)->norm=='p'?"PAL":"NTSC");
	fprintf(fd, "2\n");
	fprintf(fd, "lav2yuv -o $o -f $n %s\n", options->scene1_file);
	fprintf(fd, "lav2yuv -o $o -f $n %s\n", options->scene2_file);

	if (options->scene1_stop - options->scene1_start + 1 - options->length > 0 && preview_or_render == 0)
	{
		fprintf(fd, "%d\n", options->scene1_stop - options->scene1_start + 1 - options->length);
		fprintf(fd, "1\n");
		fprintf(fd, "0 %d\n", options->scene1_start);
		fprintf(fd, "-\n");
	}

	if(options->length > 0)
	{
		int temp_int = 1;

		fprintf(fd, "%d\n", options->length);
		fprintf(fd, "2\n");
		fprintf(fd, "0 %d\n", options->scene1_start + options->scene1_stop -
			options->scene1_start + 1 - options->length);
		fprintf(fd, "1 %d\n", options->scene2_start);
		/* here, enter the options for the transition!!  TODO */
		switch (options->type)
		{
			case GTK_TRANSITION_BLEND:
				fprintf(fd, "blend.flt -o %d -O %d",
					options->value_start, options->value_end);
				break;
			case GTK_TRANSITION_WIPE_RIGHT_TO_LEFT:
			case GTK_TRANSITION_WIPE_LEFT_TO_RIGHT:
			case GTK_TRANSITION_WIPE_TOP_TO_BOTTOM:
			case GTK_TRANSITION_WIPE_BOTTOM_TO_TOP:
				if (options->type == GTK_TRANSITION_WIPE_RIGHT_TO_LEFT)
					temp_int = 2;
				if (options->type == GTK_TRANSITION_WIPE_LEFT_TO_RIGHT)
					temp_int = 1;
				if (options->type == GTK_TRANSITION_WIPE_TOP_TO_BOTTOM)
					temp_int = 3;
				if (options->type == GTK_TRANSITION_WIPE_BOTTOM_TO_TOP)
					temp_int = 4;
				fprintf(fd, "wipe.flt -d %d -n %d -r %d",
					temp_int, options->num_rows,
					options->reposition_second*2+options->reposition_first);
				break;
			case GTK_TRANSITION_OVERLAY_ENLARGE:
			case GTK_TRANSITION_OVERLAY_ENSMALL:
				fprintf(fd, "overlay.flt %s%s-p %d,%d",
					options->type==GTK_TRANSITION_OVERLAY_ENSMALL?"-i ":"",
					options->scaling?"-s ":"",
					options->orig_pos_x, options->orig_pos_y);
				break;
		}
		/* length */
		fprintf(fd, " -l %d\n", options->length);
		//fprintf(fd, "transist.flt -s $o -n $n -o %d -O %d -d %d\n",
		//	options->value_start, options->value_end, options->length);
	}

	if (options->scene2_stop - options->scene2_start + 1 - options->length > 0 && preview_or_render == 0)
	{
		fprintf(fd, "%d\n", options->scene2_stop - options->scene2_start + 1 - options->length);
		fprintf(fd, "1\n");
		fprintf(fd, "1 %d\n", options->scene2_start + options->length);
		fprintf(fd, "-\n");
	}

	fclose(fd);

	/* done! now start lavpipe | yuvplay */
	if (preview_or_render == 0)
	{
		num_frames = options->scene2_stop + options->scene1_stop + 2 -
			options->scene1_start - options->scene2_start - options->length;
		start_lavpipe_preview(file);
	}
	else
		start_lavpipe_render(file, renderfile);
}

void stop_scene_transition(GtkWidget *widget, gpointer data)
{
	/* kill lavpipe */
	close_pipe(LAVPIPE);
}


void scene_transition_render_file_selected(GtkWidget *w, gpointer data)
{
	/* file has been selected, fetch it and let's render! */

	struct scene_transition_options *options = (struct scene_transition_options*)data;

	/* set mode to "render" */
	preview_or_render = 1;
	sprintf(renderfile, gtk_file_selection_get_filename (GTK_FILE_SELECTION (options->fs)));

	create_progress_window(options->length);

	play_scene_transition(NULL, (gpointer)options);
}

void effects_scene_transition_accept(GtkWidget *widget, gpointer data)
{
	/* OK was pressed, do lavpipe | yuv2lav and be happy */
	extern char video_format;
	char *temp;

	struct scene_transition_options *options = (struct scene_transition_options*)data;

	if (pipe_is_active(LAVPIPE) || pipe_is_active(YUVPLAY))
	{
		gtk_show_text_window(STUDIO_WARNING,
			"Lavpipe is already active");
		return;		
	}

	options->fs = gtk_file_selection_new ("Linux Video Studio - Select Location");
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (options->fs)->ok_button),
		"clicked", (GtkSignalFunc) scene_transition_render_file_selected, (gpointer)options);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(options->fs)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(options->fs)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	switch (video_format)
	{
		case 'a':
		case 'A':
			temp = "movie.avi";
			break;
		case 'q':
			temp = "movie.mov";
			break;
		case 'm':
			temp = "movie.movtar";
			break;
		default:
			temp = "movie.avi";
			break;
	}
	gtk_file_selection_set_filename (GTK_FILE_SELECTION(options->fs), temp);

	gtk_grab_add(options->fs);
	gtk_widget_show(options->fs);
}

void scene_transition_adj_changed(GtkAdjustment *adj, int *i)
{
	*i = adj->value;
}

void scene_transition_textbox_changed(GtkWidget *widget, int *i)
{
	*i = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
}

void scene_transition_button_toggled(GtkWidget *w, int *i)
{
	*i = GTK_TOGGLE_BUTTON (w)->active;
}

void scene_transition_type_selected(GtkWidget *widget, gpointer data)
{
	char *what;
	int type=0;
	struct scene_transition_options *options = (struct scene_transition_options*)data;

	what = gtk_entry_get_text(GTK_ENTRY(widget));
	if (strcmp(what,"Blend")==0)
		type = GTK_TRANSITION_BLEND;
	else if (strcmp(what,"Left-to-Right Wipe")==0)
		type = GTK_TRANSITION_WIPE_LEFT_TO_RIGHT;
	else if (strcmp(what,"Right-to-Left Wipe")==0)
		type = GTK_TRANSITION_WIPE_RIGHT_TO_LEFT;
	else if (strcmp(what,"Top-to-Bottom Wipe")==0)
		type = GTK_TRANSITION_WIPE_TOP_TO_BOTTOM;
	else if (strcmp(what,"Bottom-to-Top Wipe")==0)
		type = GTK_TRANSITION_WIPE_BOTTOM_TO_TOP;
	else if (strcmp(what,"Covering Overlay")==0)
		type = GTK_TRANSITION_OVERLAY_ENLARGE;
	else if (strcmp(what,"Disappearing Overlay")==0)
		type = GTK_TRANSITION_OVERLAY_ENSMALL;
	else return;

	options->type = type;
	if (type == GTK_TRANSITION_BLEND)
	{
		gtk_widget_hide(transition_selection_box[1]);
		gtk_widget_hide(transition_selection_box[2]);
		gtk_widget_show(transition_selection_box[0]);
	}
	if (type == GTK_TRANSITION_WIPE_RIGHT_TO_LEFT || 
		type == GTK_TRANSITION_WIPE_LEFT_TO_RIGHT ||
		type == GTK_TRANSITION_WIPE_TOP_TO_BOTTOM ||
		type == GTK_TRANSITION_WIPE_BOTTOM_TO_TOP)
	{
		gtk_widget_hide(transition_selection_box[0]);
		gtk_widget_hide(transition_selection_box[2]);
		gtk_widget_show(transition_selection_box[1]);
	}
	else if (type == GTK_TRANSITION_OVERLAY_ENLARGE ||
		type == GTK_TRANSITION_OVERLAY_ENSMALL)
	{
		gtk_widget_hide(transition_selection_box[1]);
		gtk_widget_hide(transition_selection_box[0]);
		gtk_widget_show(transition_selection_box[2]);
	}
}

void effects_scene_transition_show_window(struct scene_transition_options *options)
{
	/* Create a preview (lavpipe | yuvplay) for the transition in *options */

	GtkWidget *window, *vbox, *hbox, *vbox2, *hbox2, *hseparator, *button, *listw;
	GList *list = NULL;
	GtkScene *scene1, *scene2;
	GtkWidget *vbox3, *scrollbar, *label, *listbox, *textbox;
	GtkObject *adj;
	char temp[256];

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_window_set_title (GTK_WINDOW(window),
		"Linux Video Studio - Create Scene Transition");
	gtk_container_set_border_width (GTK_CONTAINER (window), 20);

	hbox = gtk_hbox_new(FALSE, 10);
	vbox2 = effects_tv((gpointer)options, GTK_SIGNAL_FUNC(play_scene_transition));
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, FALSE, 0);
	gtk_widget_show(vbox2);

	vbox2 = gtk_vbox_new(FALSE, 15);

	/* options for a scene transition */

	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Transition Type: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	list = g_list_append(list, "Blend");
	list = g_list_append(list, "Left-to-Right Wipe");
	list = g_list_append(list, "Right-to-Left Wipe");
	list = g_list_append(list, "Top-to-Bottom Wipe");
	list = g_list_append(list, "Bottom-to-Top Wipe");
	list = g_list_append(list, "Covering Overlay");
	list = g_list_append(list, "Disappearing Overlay");
	listw = gtk_combo_new();
	//gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(listw)->entry), FALSE);
	gtk_combo_set_popdown_strings(GTK_COMBO(listw), list);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(listw)->entry),
		"changed", GTK_SIGNAL_FUNC (scene_transition_type_selected),
		(gpointer)options);
	gtk_box_pack_start (GTK_BOX (vbox3), listw, TRUE, FALSE, 0);
	gtk_widget_show (listw);

	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Length: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	scene1 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene);
	scene2 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene + 1);
	adj = gtk_adjustment_new(options->length, 0,
		min(scene1->view_end - scene1->view_start + 1,
		scene2->view_end - scene2->view_start + 1), 1, 5, 0);
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
		GTK_SIGNAL_FUNC (scene_transition_adj_changed), &(options->length));
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (adj));
	gtk_range_set_update_policy (GTK_RANGE (scrollbar), GTK_UPDATE_CONTINUOUS);
	gtk_scale_set_digits(GTK_SCALE (scrollbar), 0);
	gtk_scale_set_value_pos(GTK_SCALE (scrollbar), GTK_POS_RIGHT);
	gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 1);
	gtk_box_pack_start (GTK_BOX (vbox3), scrollbar, TRUE, FALSE, 0);
	gtk_widget_show(scrollbar);

	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);

	/* options which are specific per transition */

	/* BLEND */
	transition_selection_box[0] = gtk_frame_new("Blend-Specific Options");
	listbox = gtk_vbox_new(FALSE, 15);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Start opacity: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	adj = gtk_adjustment_new(options->value_start, 0, 255, 1, 10, 0);
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
		GTK_SIGNAL_FUNC (scene_transition_adj_changed), &(options->value_start));
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (adj));
	gtk_range_set_update_policy (GTK_RANGE (scrollbar), GTK_UPDATE_CONTINUOUS);
	gtk_scale_set_digits(GTK_SCALE (scrollbar), 0);
	gtk_scale_set_value_pos(GTK_SCALE (scrollbar), GTK_POS_RIGHT);
	gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 1);
	gtk_box_pack_start (GTK_BOX (vbox3), scrollbar, TRUE, FALSE, 0);
	gtk_widget_show(scrollbar);

	gtk_box_pack_start (GTK_BOX (listbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("End opacity: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	adj = gtk_adjustment_new(options->value_end, 0, 255, 1, 10, 0);
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
		GTK_SIGNAL_FUNC (scene_transition_adj_changed), &(options->value_end));
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (adj));
	gtk_range_set_update_policy (GTK_RANGE (scrollbar), GTK_UPDATE_CONTINUOUS);
	gtk_scale_set_digits(GTK_SCALE (scrollbar), 0);
	gtk_scale_set_value_pos(GTK_SCALE (scrollbar), GTK_POS_RIGHT);
	gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 1);
	gtk_box_pack_start (GTK_BOX (vbox3), scrollbar, TRUE, FALSE, 0);
	gtk_widget_show(scrollbar);

	gtk_box_pack_start (GTK_BOX (listbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(transition_selection_box[0]), listbox);
	gtk_widget_show(listbox);
	gtk_box_pack_start (GTK_BOX (vbox2),
		transition_selection_box[0], TRUE, FALSE, 0);
	if (options->type == GTK_TRANSITION_BLEND)
		gtk_widget_show(transition_selection_box[0]);


	/* WIPE */
	transition_selection_box[1] = gtk_frame_new("Blend-Specific Options");
	listbox = gtk_vbox_new(FALSE, 15);
	vbox3 = gtk_vbox_new(FALSE, 5);

	hbox2 = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("Number of Rows: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	sprintf(temp, "%d", options->num_rows);
	textbox = gtk_entry_new();
	gtk_widget_set_usize(textbox, 50, 23);
	gtk_entry_set_text(GTK_ENTRY(textbox),temp);
	gtk_box_pack_start (GTK_BOX (hbox2),textbox, TRUE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(textbox), "changed",
		GTK_SIGNAL_FUNC(scene_transition_textbox_changed), &(options->num_rows));
	gtk_widget_show(textbox);
	gtk_box_pack_start (GTK_BOX (vbox3),hbox2, TRUE, FALSE, 0);
	gtk_widget_show(hbox2);

	gtk_box_pack_start (GTK_BOX (listbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Repositioning: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	button =  gtk_check_button_new_with_label ("Stream 1");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
		options->reposition_first);
	gtk_signal_connect(GTK_OBJECT(button), "toggled",
		GTK_SIGNAL_FUNC(scene_transition_button_toggled),
		&(options->reposition_first));
	gtk_box_pack_start (GTK_BOX (vbox3),button, TRUE, TRUE, 0);
	gtk_widget_show (button);
	button =  gtk_check_button_new_with_label ("Stream 2");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
		options->reposition_second);
	gtk_signal_connect(GTK_OBJECT(button), "toggled",
		GTK_SIGNAL_FUNC(scene_transition_button_toggled),
		&(options->reposition_second));
	gtk_box_pack_start (GTK_BOX (vbox3), button, TRUE, FALSE, 0);
	gtk_widget_show (button);

	gtk_box_pack_start (GTK_BOX (listbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(transition_selection_box[1]), listbox);
	gtk_widget_show(listbox);
	gtk_box_pack_start (GTK_BOX (vbox2),
		transition_selection_box[1], TRUE, FALSE, 0);
	if (options->type == GTK_TRANSITION_WIPE_RIGHT_TO_LEFT ||
		options->type == GTK_TRANSITION_WIPE_LEFT_TO_RIGHT ||
		options->type == GTK_TRANSITION_WIPE_TOP_TO_BOTTOM ||
		options->type == GTK_TRANSITION_WIPE_BOTTOM_TO_TOP)
		gtk_widget_show(transition_selection_box[1]);

	/* OVERLAY */
	transition_selection_box[2] = gtk_frame_new("Blend-Specific Options");
	listbox = gtk_vbox_new(FALSE, 15);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Point of Origin: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	hbox2 = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("X , Y: ");
	gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	textbox = gtk_entry_new();
	gtk_widget_set_usize(textbox, 50, 23);
	sprintf(temp, "%d", options->orig_pos_x);
	gtk_entry_set_text(GTK_ENTRY(textbox), temp);
	gtk_signal_connect(GTK_OBJECT(textbox), "changed",
		GTK_SIGNAL_FUNC(scene_transition_textbox_changed),
		&(options->orig_pos_x));
	gtk_box_pack_start (GTK_BOX (hbox2), textbox, TRUE, FALSE, 0);
	gtk_widget_show(textbox);
	label = gtk_label_new(" , ");
	gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	textbox = gtk_entry_new();
	gtk_widget_set_usize(textbox, 50, 23);
	sprintf(temp, "%d", options->orig_pos_y);
	gtk_entry_set_text(GTK_ENTRY(textbox), temp);
	gtk_signal_connect(GTK_OBJECT(textbox), "changed",
		GTK_SIGNAL_FUNC(scene_transition_textbox_changed),
		&(options->orig_pos_y));
	gtk_box_pack_start (GTK_BOX (hbox2), textbox, TRUE, FALSE, 0);
	gtk_widget_show(textbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox2, TRUE, FALSE, 0);
	gtk_widget_show(hbox2);

	gtk_box_pack_start (GTK_BOX (listbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Scaling: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	button =  gtk_check_button_new_with_label ("Enable scaling");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
		options->scaling);
	gtk_signal_connect(GTK_OBJECT(button), "toggled",
		GTK_SIGNAL_FUNC(scene_transition_button_toggled),
		&(options->scaling));
	gtk_box_pack_start (GTK_BOX (vbox3), button, TRUE, FALSE, 0);
	gtk_widget_show (button);

	gtk_box_pack_start (GTK_BOX (listbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(transition_selection_box[2]), listbox);
	gtk_widget_show(listbox);
	gtk_box_pack_start (GTK_BOX (vbox2),
		transition_selection_box[2], TRUE, FALSE, 0);
	if (options->type == GTK_TRANSITION_OVERLAY_ENLARGE ||
		options->type == GTK_TRANSITION_OVERLAY_ENSMALL)
		gtk_widget_show(transition_selection_box[2]);

	/***********************************/


	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, FALSE, 0);
	gtk_widget_show(vbox2);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
	gtk_widget_show(hbox);


	hseparator = gtk_hseparator_new();
	gtk_box_pack_start (GTK_BOX (vbox), hseparator, TRUE, TRUE, 0);
	gtk_widget_show (hseparator);


	hbox = gtk_hbox_new(TRUE, 20);

	button = gtk_button_new_with_label("  Accept  ");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(effects_scene_transition_accept), (gpointer)options);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, FALSE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("  Cancel  ");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, FALSE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show(vbox);

	/* prepare preview-mode */
	preview_or_render = 0;

	gtk_grab_add(window);
	gtk_widget_show(window);
}

void lavedit_effects_create_scene_transition(GtkWidget *widget, gpointer data)
{
	struct scene_transition_options *options;
	GtkScene *scene1, *scene2;

	/* Create a scene transition using lavpipe and transist.flt */

	if (GTK_SCENELIST(scenelist)->selected_scene < 0)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"Select an image for the scene transition");
		return;
	}

	if (GTK_SCENELIST(scenelist)->selected_scene+1 == (int)g_list_length(GTK_SCENELIST(scenelist)->scene))
	{
		gtk_show_text_window(STUDIO_WARNING,
			"Cannot do transition on the last scene - "
			"scene transitions are applied on the current and next scene");
		return;
	}

	/* okay, so we're allrighty, let's now start a wizzard-kinda thingy */
	options = (struct scene_transition_options*)malloc(sizeof(struct scene_transition_options));

	/* let's start by setting some defaults */
	options->type = GTK_TRANSITION_BLEND;
	options->value_start = 0;
	options->value_end = 255;
	options->num_rows = 1;
	options->reposition_first = 0;
	options->reposition_second = 0;
	options->orig_pos_x = 0;
	options->orig_pos_y = 0;
	options->scaling = 0;
	strcpy(options->scene1_file, gtk_scenelist_get_movie(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene));
	scene1 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene);
	options->scene1_start = scene1->view_start;
	options->scene1_stop = scene1->view_end;
	strcpy(options->scene2_file, gtk_scenelist_get_movie(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene + 1));
	scene2 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene + 1);
	options->scene2_start = scene2->view_start;
	options->scene2_stop = scene2->view_end;

	if (min(scene1->view_end - scene1->view_start + 1,
		scene2->view_end - scene2->view_start + 1) < 25)
	{
		options->length = min(scene1->view_end - scene1->view_start + 1,
			scene2->view_end - scene2->view_start + 1);
	}
	else
	{
		options->length = 25;
	}
	/* Set preview aspect ratio */
	if (strncmp(picture_aspect,"16:9",8))
	  lavedit_effects_preview_width = (lavedit_effects_preview_height*16)/9;
	else
	  lavedit_effects_preview_width = (lavedit_effects_preview_height*4)/3;

	effects_scene_transition_show_window(options);
}

/* rgb-to-yuv(), does 24/32bpp RGB to YUV422 colorspace conversion
 * width/height are sizes of the input RGB-data
 * x_offset, y_offset, image_w, image_h are sizes and x/y offsets for the RGB picture
 * guchar *rgb is the RGB-data in 24 or 32bpp format
 * guchar **yuv are allocated pointers of w*h (yuv[0]) or w*h/4 (yuv[12])
 * has_alpha means whether the image has alpha (probably it does)
 */
void rgb_to_yuv(int width, int height, int rowstride, guchar *rgb, guchar **yuv, int has_alpha,
	int x_offset, int y_offset, int image_w, int image_h)
{
	int x,y;
	guchar temp[4];

	int bpp = has_alpha?4:3;

	for (y=0;y<height;y++)
	{
		for (x=0;x<width;x++)
		{
			/* only do something if it's inside the offset<->width/height region */
			if (y >= y_offset && y < y_offset + image_h &&
				x >= x_offset && x < x_offset + image_w)
			{
				/* set Y = 0.29900 * R + 0.58700 * G + 0.11400 * B */
				yuv[0][y*width+x] = 0.257 * rgb[((y-y_offset)*rowstride)+(x-x_offset)*bpp] +
					0.504 * rgb[((y-y_offset)*rowstride)+(x-x_offset)*bpp+1] +
					0.098 * rgb[((y-y_offset)*rowstride)+(x-x_offset)*bpp+2] + 16;

				/* only set U/V if we're on an even row/column (YUV422!!) */
				if(x%2==0 && y%2==0)
				{
					int a;

					for(a=0;a<4;a++)
					{
						/* set U = -0.16874 * R - 0.33126 * G + 0.50000 * B */
						temp[a] = -0.148 * rgb[(((y-y_offset)+a/2)*rowstride)+((x-x_offset)+a%2)*bpp] -
							0.291 * rgb[(((y-y_offset)+a/2)*rowstride)+((x-x_offset)+a%2)*bpp+1] +
							0.439 * rgb[(((y-y_offset)+a/2)*rowstride)+((x-x_offset)+a%2)*bpp+2] + 128;
					}
					yuv[1][y*width/4+x/2] = (temp[0]+temp[1]+temp[2]+temp[3])/4;

					for(a=0;a<4;a++)
					{
						/* Set V = 0.50000 * R - 0.41869 * G - 0.08131 * B */
						temp[a] = 0.439 * rgb[(((y-y_offset)+a/2)*rowstride)+((x-x_offset)+a%2)*bpp] -
							0.368 * rgb[(((y-y_offset)+a/2)*rowstride)+((x-x_offset)+a%2)*bpp+1] -
							0.071 * rgb[(((y-y_offset)+a/2)*rowstride)+((x-x_offset)+a%2)*bpp+2] + 128;
					}
					yuv[2][y*width/4+x/2] = (temp[0]+temp[1]+temp[2]+temp[3])/4;
				}
			}
			else
			{
				/* let's make it white (that's the color GdkPixbuf uses for transparency) */
				yuv[0][y*width+x] = 255;
				if(x%2==0 && y%2==0)
					yuv[1][y*width/4+x/2] = yuv[2][y*width/4+x/2] = 128;
			}
		}
	}
}

/* bw_to_yuv(), turns black/white image data into YUV422
 * width/height are yuv image size
 * x_offset, y_offset, image_w, image_h are sizes and x/y offsets for the RGB picture
 * guchar *rgba is RGBA data, where each 4th pixel is alpha channel (which we use here)
 * guchar **yuv are allocated pointers of w*h (yuv[0]) or w*h/4 (yuv[12])
 */
void alpha_to_yuv(int width, int height, int rowstride, guchar *rgba, guchar **yuv,
	int has_alpha, int x_offset, int y_offset, int image_w, int image_h, int opacity)
{
	int x,y;

	for (y=0;y<height;y++)
	{
		for (x=0;x<width;x++)
		{
			if (y >= y_offset && y < y_offset + image_h &&
				x >= x_offset && x < x_offset + image_w)
			{
				if (has_alpha)
					yuv[0][width*y+x] = rgba[(rowstride*(y-y_offset))+(x-x_offset)*4+3] *
						(opacity/100.0);
				else
					yuv[0][width*y+x] = 255 * (opacity/100.0);

				if (x%2==0 && y%2==0)
					yuv[1][width*y/4+x/2] = yuv[2][width*y/4+x/2] = 128;
			}
			else
			{
				/* make it invisible (=black) */
				yuv[0][y*width+x] = 0;

				if (x%2==0 && y%2==0)
					yuv[1][width*y/4+x/2] = yuv[2][width*y/4+x/2] = 128;
			}
		}
	}
}

void play_text_overlay(GtkWidget *widget, gpointer data)
{
	/* we need to render the image first */
	GdkColor color, bg_color;
	GdkPixmap* pixmap;
	GdkGC *gc;
	GdkImage *image;
	int w,h,x,y,wi,lbea,rbea,asc,desc;
	char *text;
	guchar *pixel_data;
	guint32 pixel;

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	text = options->text;

	if (!options->fontdesc)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"You need to choose a font first");
		return;
	}
	if (!text)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"You need to enter text first");
		return;
	}
	if (strcmp(text, "")==0)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"You need to enter text first");
		return;
	}

	color.pixel = 0x0;
	bg_color.pixel = 0x1;

	gdk_string_extents(options->fontdesc, text,
			   &lbea, &rbea, &wi, &asc, &desc);
	h = asc+desc;
	w = rbea-lbea;

	if (verbose) printf("Drawing pixmap (%dx%d) with font %s\n", w, h,
		options->font);

	pixmap = gdk_pixmap_new(NULL, w, h, 1);

	if (!pixmap)
	{
		gtk_show_text_window(STUDIO_ERROR,
			"Could not create the image. Your X-server might be confused");
		return;
	}

	gc = gdk_gc_new(pixmap);
	gdk_gc_set_foreground(gc, &bg_color);
	gdk_gc_set_background(gc, &color);

	gdk_draw_rectangle(pixmap, gc, 1, 0, 0, w, h);

	gdk_gc_set_foreground(gc, &color);
	gdk_gc_set_background(gc, &bg_color);

	gdk_draw_string(pixmap, options->fontdesc, gc, -lbea, asc, text);

	image = gdk_image_get(pixmap, 0,0,w,h);
	pixel_data = malloc(sizeof(guchar)*w*h*4);

	for (y=0;y<h;y++)
	{
		for (x=0;x<w;x++)
		{
			pixel = gdk_image_get_pixel(image,x,y);
			pixel_data[(y*w+x)*4] = options->colors[0]*255.0;
			pixel_data[(y*w+x)*4+1] = options->colors[1]*255.0;
			pixel_data[(y*w+x)*4+2] = options->colors[2]*255.0;
			pixel_data[(y*w+x)*4+3] = pixel?0:255;
		}
	}

	options->image = gdk_pixbuf_new_from_data(pixel_data, GDK_COLORSPACE_RGB,
		1, 8, w, h, w*4, NULL, NULL);

	gdk_gc_unref(gc);
	gdk_pixmap_unref(pixmap);
	/* Fort testing purposes. If scaling works, remove the following 2
	   statements. */
	/*options->image_width = w;
	  options->image_height = h;*/

	play_image_overlay(NULL, (gpointer)options);
}

void write_yuv_to_file(FILE *fd, guchar **yuv, int width, int height)
{
  /* Convenience function for writing yuv data to a file */
  fwrite(yuv[0], sizeof(guchar), width*height, fd);
  fwrite(yuv[1], sizeof(guchar), width*height/4, fd);
  fwrite(yuv[2], sizeof(guchar), width*height/4, fd);
}

void play_image_overlay(GtkWidget *widget, gpointer data)
{
	/* write a lavpipe .pli list and start lavpipe | yuvplay */
	FILE *fd;
	char file[256], yuv_file[256], yuv_blend_file[256];
	GdkPixbuf *temp_image;
	guchar *yuv[3], *yuv_blend[3];
	int i;

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	/* if there's no image, we'd better give a warning and return */
	if (!options->image)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"You need to choose an image first");
		return;
	}

	if (options->image_width <= 0 || options->image_height <= 0)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"The size of the image needs to be greater than zero");
		return;
	}

	/* let's start writing an editlist */
	sprintf(file, "%s/.studio/effect.pli", getenv("HOME"));

	/* create a scaled gdkpixbuf and "yuv" it */
	if (options->image_width != gdk_pixbuf_get_width(options->image) ||
		options->image_height != gdk_pixbuf_get_height(options->image))
	{
		if (verbose) printf("Scaling from %dx%d to %dx%d\n",
			gdk_pixbuf_get_width(options->image),
			gdk_pixbuf_get_height(options->image),
			options->image_width, options->image_height);
		temp_image = gdk_pixbuf_scale_simple(options->image, options->image_width,
			options->image_height, GDK_INTERP_BILINEAR);
	}
	else
		temp_image = options->image;

	  yuv[0] = (guchar*)malloc(sizeof(guchar)*options->movie_width*options->movie_height);
	  yuv_blend[0] = (guchar*)malloc(sizeof(guchar)*options->movie_width*options->movie_height);
	  for(i=1;i<3;i++)
	    {
	      yuv[i] = (guchar*)malloc(sizeof(guchar)*options->movie_width*options->movie_height/4);
	      yuv_blend[i] = (guchar*)malloc(sizeof(guchar)*options->movie_width*options->movie_height/4);
	    }

	  rgb_to_yuv(options->movie_width, options->movie_height,
		     gdk_pixbuf_get_rowstride(temp_image), gdk_pixbuf_get_pixels(temp_image),
		     yuv, gdk_pixbuf_get_has_alpha(options->image), options->image_x, options->image_y,
		     options->image_width, options->image_height);
	  alpha_to_yuv(options->movie_width, options->movie_height,
		       gdk_pixbuf_get_rowstride(temp_image), gdk_pixbuf_get_pixels(temp_image),
		       yuv_blend, gdk_pixbuf_get_has_alpha(options->image), options->image_x, options->image_y,
		       options->image_width, options->image_height, options->opacity);

	  /* create the YUV files */
	  sprintf(yuv_file, "%s/.studio/image.yuv", getenv("HOME"));
	  fd = fopen(yuv_file, "w");
	  if (fd == NULL)
	    {
	      gtk_show_text_window(STUDIO_ERROR, "Error opening \'%s\': %s",
				   yuv_file, strerror(errno));
	      return;
	    }
	  write_yuv_to_file(fd, yuv, options->movie_width, options->movie_height);
	  fclose(fd);

	  sprintf(yuv_blend_file, "%s/.studio/blend.yuv", getenv("HOME"));
	  fd = fopen(yuv_blend_file, "w");
	  if (fd == NULL)
	    {
	      gtk_show_text_window(STUDIO_ERROR, "Error opening \'%s\': %s",
				   yuv_blend_file, strerror(errno));
	      return;
	    }
	  write_yuv_to_file(fd, yuv_blend, options->movie_width,
			    options->movie_height);
	  fclose(fd);

	  for(i=0;i<3;i++)
	    {
	      free(yuv[i]);
	      free(yuv_blend[i]);
	    }

	/* unref the image only if it was recreated */
	if (options->image_width != gdk_pixbuf_get_width(options->image) ||
		options->image_height != gdk_pixbuf_get_height(options->image) || options->type == GTK_EFFECT_TEXT)
		gdk_pixbuf_unref(temp_image);

	fd = fopen(file, "w");
	if (fd == NULL)
	{
		gtk_show_text_window(STUDIO_ERROR,"Error opening \'%s\': %s",
			file, strerror(errno));
		return;
	}

	fprintf(fd, "LAV Pipe List\n");
	fprintf(fd, "%s\n", GTK_SCENELIST(scenelist)->norm=='p'?"PAL":"NTSC");
	fprintf(fd, "3\n");
	fprintf(fd, "lav2yuv -o $o -f $n %s\n", options->scene_file);

	fprintf(fd, "forevery4m2 %d %d %c %s %s\n",
		options->movie_width, options->movie_height,
		options->interlacing_info, 
		GTK_SCENELIST(scenelist)->norm=='p'?"25:1":"30000:1001",
		yuv_file);
	fprintf(fd, "forevery4m2 %d %d %c %s %s\n",
		options->movie_width, options->movie_height,
		options->interlacing_info,
		GTK_SCENELIST(scenelist)->norm=='p'?"25:1":"30000:1001",
		yuv_blend_file);

	if (options->offset > 0)
	{
		/* play part of the scene first without an overlay */
		fprintf(fd, "%d\n", options->offset);
		fprintf(fd, "1\n");
		fprintf(fd, "0 %d\n", options->scene_start);
		fprintf(fd, "-\n");
	}

	if (options->length > 0)
	{
		fprintf(fd, "%d\n", options->length);
		fprintf(fd, "3\n");
		fprintf(fd, "0 %d\n", options->scene_start + options->offset);
		fprintf(fd, "1 0\n");
		fprintf(fd, "2 0\n");
		fprintf(fd, "matteblend.flt\n");
	}

	if (options->offset + options->length < options->scene_end - options->scene_start + 1)
	{
		fprintf(fd, "%d\n", options->scene_end - options->scene_start + 1 -
			options->offset - options->length);
		fprintf(fd, "1\n");
		fprintf(fd, "0 %d\n", options->scene_start + options->offset + options->length);
		fprintf(fd, "-\n");
	}

	fclose(fd);

	if (preview_or_render == 1)
	{
		sprintf(soundfile, "%s/.studio/effect.eli", getenv("HOME"));
		fd = fopen(soundfile, "w");
		if (fd == NULL)
		{
			gtk_show_text_window(STUDIO_ERROR,"Error opening \'%s\': %s",
				soundfile, strerror(errno));
			return;
		}
		fprintf(fd, "LAV Edit List\n");
		fprintf(fd, "%s\n", GTK_SCENELIST(scenelist)->norm=='p'?"PAL":"NTSC");
		fprintf(fd, "1\n");
		fprintf(fd, "%s\n",
			gtk_scenelist_get_movie(GTK_SCENELIST(scenelist),
				GTK_SCENELIST(scenelist)->selected_scene));
		fprintf(fd, "0 %d %d\n", options->scene_start, options->scene_end);
		fclose(fd);
	}

	/* done! now start lavpipe | yuvplay */
	if (preview_or_render == 0)
	{
		num_frames = options->scene_end - options->scene_start + 1;
		start_lavpipe_preview(file);
	}
	else
		start_lavpipe_render(file, renderfile);
}

void overlay_render_file_selected(GtkWidget *w, gpointer data)
{
	/* file has been selected, fetch it and let's render! */

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	/* set mode to "render" */
	preview_or_render = 1;
	sprintf(renderfile, "%s", gtk_file_selection_get_filename (GTK_FILE_SELECTION (options->fs)));

	create_progress_window(options->scene_end - options->scene_start + 1);

	if (options->type == GTK_EFFECT_TEXT)
		play_text_overlay(NULL, (gpointer)options);
	else
		play_image_overlay(NULL, (gpointer)options);
}

void effects_image_overlay_accept(GtkWidget *widget, gpointer data)
{
	/* OK was pressed, do lavpipe | yuv2lav and be happy */
	extern char video_format;
	char *temp;

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	//if (pipe_is_active(LAVPIPE) || pipe_is_active(YUVPLAY))
	//{
	//	gtk_show_text_window(STUDIO_WARNING,
	//		"Lavpipe is already active", NULL);
	//	return;		
	//}

	options->fs = gtk_file_selection_new ("Linux Video Studio - Select Location");
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (options->fs)->ok_button),
		"clicked", (GtkSignalFunc) overlay_render_file_selected, (gpointer)options);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(options->fs)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(options->fs)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	switch (video_format)
	{
		case 'a':
		case 'A':
			temp = "movie.avi";
			break;
		case 'q':
			temp = "movie.mov";
			break;
		case 'm':
			temp = "movie.movtar";
			break;
		default:
			temp = "movie.avi";
			break;
	}
	gtk_file_selection_set_filename (GTK_FILE_SELECTION(options->fs), temp);

	gtk_grab_add(options->fs);
	gtk_widget_show(options->fs);
}

void image_overlay_file_load(GtkWidget *w, gpointer data)
{
	/* select image, load it in gdkpixbuf
	 * if necessary, change thewidth/height/x/yoffset parameter adjustments/values */
	char temp[10];

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	/* set the textfield text which contains the filename of the image */
	gtk_entry_set_text(GTK_ENTRY(options->image_file_textbox),
		gtk_file_selection_get_filename (GTK_FILE_SELECTION (options->fs)));

	/* load the image in the gdkpixbuf */
	options->image = gdk_pixbuf_new_from_file(
		gtk_file_selection_get_filename (GTK_FILE_SELECTION (options->fs)));
	if (!options->image)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"Could not load image \'%s\', are you sure it's an image?",
			gtk_file_selection_get_filename (GTK_FILE_SELECTION (options->fs)));
		return;
	}

	/* gdkpixbuf is loaded, now set the parameters (x/y offset, width, height) */
	options->image_width = gdk_pixbuf_get_width(options->image);
	options->image_height = gdk_pixbuf_get_height(options->image);

	sprintf(temp, "%d", options->image_width);
	gtk_entry_set_text(GTK_ENTRY(options->image_position[0]), temp);

	sprintf(temp, "%d", options->image_height);
	gtk_entry_set_text(GTK_ENTRY(options->image_position[1]), temp);
}

void select_image_overlay_file(GtkWidget *widget, gpointer data)
{
	/* show fileselection box - image_overlay_file_load() if done */

	struct image_overlay_options *options = (struct image_overlay_options*)data;
	
	options->fs = gtk_file_selection_new ("Linux Video Studio - Select Location");
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (options->fs)->ok_button),
		"clicked", (GtkSignalFunc) image_overlay_file_load, (gpointer)options);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(options->fs)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(options->fs)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	gtk_file_selection_set_filename (GTK_FILE_SELECTION(options->fs),
		gtk_entry_get_text(GTK_ENTRY(options->image_file_textbox)));

	gtk_grab_add(options->fs);
	gtk_widget_show(options->fs);
}

void text_overlay_font_load(GtkWidget *w, gpointer data)
{
	/* OK was pressed - load the selected font */
  int res_x, res_y, i, j, field;
  char newname[512];

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	options->font = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(options->fs));

	if (!options->font)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"There was an error loading the font \'%s\'",
			gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG (options->fs)));
		return;
	}

	/* Rewrite font and load it. Adapted from Xlib Programming Manual */
	res_x = 100;
	res_y = (res_x*options->aspect_enu)/options->aspect_den;
	for (i = j = field = 0; (options->font[i] != '\0') && 
	       (field <= 14); i++) {
	  newname[j++] = options->font[i];
	  if (options->font[i] == '-') {
	    field++;
	    switch (field) {
	    case 7:
	    case 12:
	      newname[j++] = '*';
	      while ((options->font[i+1] != '-') && 
		     (options->font[i+1] != '\0')) i++;
	      break;
	    case 9:
	    case 10: /* Rewrite resolution:
			Assume a minimal resolution of 100 dpi */
	      sprintf(&newname[j],"%d",(field == 9) ? res_x : res_y);
	      while (newname[j] != '\0') j++;
	      while ((options->font[i+1] != '-') && 
		     (options->font[i+1] != '\0')) i++;
	      break;
	    }
	  }
	}
	newname[j] = '\0';
	if ((field != 14) ||
	    (NULL == (options->fontdesc = gdk_font_load(newname))))
	{
		gtk_show_text_window(STUDIO_WARNING,
			"There was an error loading the font \'%s\'",
			newname);
		options->font = NULL;
		options->fontdesc = NULL;
		return;
	}

	gtk_signal_emit_by_name(GTK_OBJECT(options->image_file_textbox),
		"changed");
}

void select_text_overlay_font(GtkWidget *w, gpointer data)
{
	/* show fontselection box - text_overlay_font_load() if done */

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	options->fs = gtk_font_selection_dialog_new ("Linux Video Studio - Select Font");
	gtk_signal_connect (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG (options->fs)->ok_button),
		"clicked", (GtkSignalFunc) text_overlay_font_load, (gpointer)options);
	gtk_signal_connect_object (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG(options->fs)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG(options->fs)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	/* Should be used for widescreen adaption */
	gtk_font_selection_dialog_set_filter(GTK_FONT_SELECTION_DIALOG(options->fs),
					     GTK_FONT_FILTER_BASE,GTK_FONT_SCALABLE,
					     NULL,NULL,NULL,NULL,NULL,NULL);
	if (options->font)
		gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(options->fs), options->font);

	gtk_grab_add(options->fs);
	gtk_widget_show(options->fs);
}

void text_overlay_color_load(GtkWidget *w, gpointer data)
{
	/* OK was pressed - load the selected color */

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	gtk_color_selection_get_color(GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(options->fs)->colorsel),
		options->colors);

	gtk_signal_emit_by_name(GTK_OBJECT(options->image_file_textbox),
		"changed");
}

void select_text_overlay_color(GtkWidget *w, gpointer data)
{
	/* show colorselection box - text_overlay_color_load() if done */

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	options->fs = gtk_color_selection_dialog_new ("Linux Video Studio - Select Color");
	gtk_signal_connect (GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (options->fs)->ok_button),
		"clicked", (GtkSignalFunc) text_overlay_color_load, options);
	gtk_signal_connect_object (GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG(options->fs)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));
	gtk_signal_connect_object (GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG(options->fs)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (options->fs));

	gtk_grab_add(options->fs);
	gtk_widget_show(options->fs);
}

void image_overlay_adj_changed(GtkAdjustment *adj, gpointer data)
{
	/* set offset/length */

	int value1, value2;
	struct image_overlay_options *options = (struct image_overlay_options*)data;

	value1 = GTK_ADJUSTMENT(options->adj[0])->value;
	value2 = GTK_ADJUSTMENT(options->adj[1])->value;

	options->offset = min(value1, value2);
	options->length = value1>value2?value1-value2:value2-value1;
}

void text_overlay_textbox_changed(GtkWidget *w, gpointer data)
{
	/* the text has been changed, recalculate width/height */

	struct image_overlay_options *options = (struct image_overlay_options*)data;

	char *text = gtk_entry_get_text(GTK_ENTRY(w));
	int lbea, rbea, wi, asc, desc, wn, h;

	strcpy(options->text, text);

	if (text && options->font)
		if (strcmp(text, "")!=0)
		{
			char temp[10];	
			gdk_string_extents(options->fontdesc, text,
			   &lbea, &rbea, &wi, &asc, &desc);
			h = asc+desc;
			wn = rbea-lbea;
			options->image_width = wn;
			options->image_height = h;

			sprintf(temp, "%d", options->image_width);
			gtk_entry_set_text(GTK_ENTRY(options->image_position[0]), temp);
			sprintf(temp, "%d", options->image_height);
			gtk_entry_set_text(GTK_ENTRY(options->image_position[1]), temp);
		}
}

void image_overlay_textbox_changed(GtkWidget *widget, int *i)
{
	*i = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
}

void effects_image_overlay_show_window(struct image_overlay_options *options)
{
	/* Create a preview (lavpipe | yuvplay) for the image overlay in *options */

	GtkWidget *window, *vbox, *hbox, *vbox2, *hbox2, *hseparator,*button=NULL;
	GtkWidget *vbox3, *scrollbar, *label;
	GtkObject *adj;
	int i;

	if (options->type != GTK_EFFECT_IMAGE && options->type != GTK_EFFECT_TEXT)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"Unknown overlay type (options->type = %d)", options->type);
		return;
	}

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_window_set_title (GTK_WINDOW(window),
		"Linux Video Studio - Create Image Overlay");
	gtk_container_set_border_width (GTK_CONTAINER (window), 20);

	hbox = gtk_hbox_new(FALSE, 10);
	if (options->type == GTK_EFFECT_IMAGE)
		vbox2 = effects_tv((gpointer)options,
			GTK_SIGNAL_FUNC(play_image_overlay));
	else
		vbox2 = effects_tv((gpointer)options,
			GTK_SIGNAL_FUNC(play_text_overlay));
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, FALSE, 0);
	gtk_widget_show(vbox2);

	vbox2 = gtk_vbox_new(FALSE, 15);

	/* options for a scene transition */

	/* just an idea:
	 * image width/height/offset could be done by a widget presenting a frame
	 * of the video and the picture, resizable, on top of it... (difficult)
	 */

	vbox3 = gtk_vbox_new(FALSE, 5);

	label = NULL;
	if (options->type == GTK_EFFECT_IMAGE)
		label = gtk_label_new("Image file: ");
	else if (options->type == GTK_EFFECT_TEXT)
		label = gtk_label_new("Text, Font and Color: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);

	hbox2 = gtk_hbox_new(FALSE, 5);
	options->image_file_textbox = gtk_entry_new();
	gtk_box_pack_start (GTK_BOX (hbox2), options->image_file_textbox, TRUE, TRUE, 0);
	if (options->type == GTK_EFFECT_TEXT)
		gtk_signal_connect(GTK_OBJECT(options->image_file_textbox), "changed",
			GTK_SIGNAL_FUNC(text_overlay_textbox_changed), (gpointer)options);
	gtk_widget_show(options->image_file_textbox);
	if (options->type == GTK_EFFECT_IMAGE)
	{
		button = gtk_image_label_button(NULL,
					"Select Image for Image Overlay",
					file_widget_open_xpm, 0, GTK_POS_BOTTOM);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(select_image_overlay_file), (gpointer)options);
	}
	else if (options->type == GTK_EFFECT_TEXT)
	{
		gtk_box_pack_start (GTK_BOX (vbox3), hbox2, TRUE, FALSE, 0);
		gtk_widget_show(hbox2);
		hbox2 = gtk_hbox_new(FALSE, 5);

		button = gtk_image_label_button(NULL,
					"Select Font",
					gnome_fontsel_xpm, 0, GTK_POS_BOTTOM);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(select_text_overlay_font), (gpointer)options);
		gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, FALSE, 0);
		gtk_widget_show(button);
		button = gtk_image_label_button(NULL,
					"Select Color",
					gnome_color_browser_xpm, 0, GTK_POS_BOTTOM);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(select_text_overlay_color), (gpointer)options);
	}
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, FALSE, 0);
	gtk_widget_show(button);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox2, TRUE, FALSE, 0);
	gtk_widget_show(hbox2);

	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Duration (length/offset): ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	for(i=0;i<2;i++)
	{
		int a;
		if (i==0) a = options->offset;
		else a = options->scene_end - options->scene_start + 1;
		options->adj[i] = gtk_adjustment_new(a, 0,
			options->scene_end - options->scene_start + 1,
			1, 10, 0);
		gtk_signal_connect (GTK_OBJECT (options->adj[i]), "value_changed",
			GTK_SIGNAL_FUNC (image_overlay_adj_changed), (gpointer)options);
	}
	scrollbar = gtk_enhanced_scale_new(options->adj,2);
	GTK_ENHANCED_SCALE(scrollbar)->all_the_same = 1;
	gtk_box_pack_start (GTK_BOX (vbox3), scrollbar, TRUE, FALSE, 0);
	gtk_widget_show(scrollbar);

	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Image opacity: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	adj = gtk_adjustment_new(options->opacity, 0, 100, 1, 10, 0);
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
		GTK_SIGNAL_FUNC (scene_transition_adj_changed), &(options->opacity));
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (adj));
	gtk_range_set_update_policy (GTK_RANGE (scrollbar), GTK_UPDATE_CONTINUOUS);
	gtk_scale_set_digits(GTK_SCALE (scrollbar), 0);
	gtk_scale_set_value_pos(GTK_SCALE (scrollbar), GTK_POS_RIGHT);
	gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 1);
	gtk_box_pack_start (GTK_BOX (vbox3), scrollbar, TRUE, FALSE, 0);
	gtk_widget_show(scrollbar);

	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	vbox3 = gtk_vbox_new(FALSE, 5);

	label = gtk_label_new("Image position/size: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox3), label, TRUE, FALSE, 0);
	gtk_widget_show(label);

	hbox2 = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("W x H: ");
	gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	options->image_position[0] = gtk_entry_new();
	gtk_widget_set_usize(options->image_position[0], 50, 23);
	if (options->type == GTK_EFFECT_TEXT)
	  gtk_entry_set_editable(GTK_ENTRY(options->image_position[0]),FALSE);
	else
	  gtk_signal_connect(GTK_OBJECT(options->image_position[0]), "changed",
		GTK_SIGNAL_FUNC(image_overlay_textbox_changed), &(options->image_width));
	gtk_box_pack_start (GTK_BOX (hbox2), options->image_position[0], TRUE, FALSE, 0);
	gtk_widget_show(options->image_position[0]);
	label = gtk_label_new(" x ");
	gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	options->image_position[1] = gtk_entry_new();
	gtk_widget_set_usize(options->image_position[1], 50, 23);
	if (options->type == GTK_EFFECT_TEXT)
	  gtk_entry_set_editable(GTK_ENTRY(options->image_position[1]),FALSE);
	else
	  gtk_signal_connect(GTK_OBJECT(options->image_position[1]), "changed",
		GTK_SIGNAL_FUNC(image_overlay_textbox_changed), &(options->image_height));
	gtk_box_pack_start (GTK_BOX (hbox2), options->image_position[1], TRUE, FALSE, 0);
	gtk_widget_show(options->image_position[1]);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox2, TRUE, FALSE, 0);
	gtk_widget_show(hbox2);

	hbox2 = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new("X , Y: ");
	gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	options->image_position[2] = gtk_entry_new();
	gtk_widget_set_usize(options->image_position[2], 50, 23);
	gtk_signal_connect(GTK_OBJECT(options->image_position[2]), "changed",
		GTK_SIGNAL_FUNC(image_overlay_textbox_changed), &(options->image_x));
	gtk_box_pack_start (GTK_BOX (hbox2), options->image_position[2], TRUE, FALSE, 0);
	gtk_widget_show(options->image_position[2]);
	label = gtk_label_new(" , ");
	gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, FALSE, 0);
	gtk_widget_show(label);
	options->image_position[3] = gtk_entry_new();
	gtk_widget_set_usize(options->image_position[3], 50, 23);
	gtk_signal_connect(GTK_OBJECT(options->image_position[3]), "changed",
		GTK_SIGNAL_FUNC(image_overlay_textbox_changed), &(options->image_y));
	gtk_box_pack_start (GTK_BOX (hbox2), options->image_position[3], TRUE, FALSE, 0);
	gtk_widget_show(options->image_position[3]);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox2, TRUE, FALSE, 0);
	gtk_widget_show(hbox2);

	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);


	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, FALSE, 0);
	gtk_widget_show(vbox2);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
	gtk_widget_show(hbox);


	hseparator = gtk_hseparator_new();
	gtk_box_pack_start (GTK_BOX (vbox), hseparator, TRUE, TRUE, 0);
	gtk_widget_show (hseparator);


	hbox = gtk_hbox_new(TRUE, 20);

	button = gtk_button_new_with_label("  Accept  ");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(effects_image_overlay_accept), (gpointer)options);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, FALSE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("  Cancel  ");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, FALSE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
	gtk_widget_show(hbox);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show(vbox);

	/* prepare preview-mode */
	preview_or_render = 0;

	gtk_grab_add(window);
	gtk_widget_show(window);
}

void lavedit_effects_create_overlay(GtkWidget *widget, char *data)
{
	/* Create an image/text overlay using lavpipe and matteblend.flt */

	struct image_overlay_options *options;
	int i;
	GtkScene *scene;
	FILE *fdd;
	char tempfile[256];
	char syscall[256];

	if (GTK_SCENELIST(scenelist)->selected_scene < 0)
	{
		gtk_show_text_window(STUDIO_WARNING,
			"Select a scene for the image overlay");
		return;
	}

	options = (struct image_overlay_options*)malloc(sizeof(struct image_overlay_options));

	sprintf(options->scene_file, gtk_scenelist_get_movie(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene));
	scene = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene);
	options->scene_start = scene->view_start;
	options->scene_end = scene->view_end;
	options->length = options->scene_end - options->scene_start + 1;
	options->offset = 0;
	options->image_x = 0; /* top left position */
	options->image_y = 0; /* top left position */
	options->image_file_textbox = NULL;
	options->image_width = 0; /* unknown for now */
	options->image_height = 0; /* unknown for now */
	options->opacity = 100; /* default opacity */

	/* now a bit tricky, get the movie settings by obtaining one frame */
	/* Video format (normal or widescreen) used */
	sprintf(tempfile, "%s/.studio/tempfile.lav2yuv.data", getenv("HOME"));
	sprintf(syscall, "\"%s\" -f 1 -P %s \"%s\" > \"%s\" 2>/dev/null",
		app_location(LAV2YUV), picture_aspect, options->scene_file, tempfile);
	system(syscall);
	fdd = fopen(tempfile, "r");
	syscall[0] = '\0';
	fgets(syscall, 255, fdd);
	fclose(fdd);
	unlink(tempfile);

	for (i=0;i<(int)strlen(syscall);i++)
		if (syscall[i] == '\n' || syscall[i] == '\0')
		{
			syscall[i] = '\0';
			break;
		}
	if (verbose) printf("lav2yuv gave us the following header: \'%s\'\n",
		syscall);
	/* Here we assume that the header is correct! */
	for (i = 0; i < (int)strlen(syscall); i++)
	  if (!strncmp(syscall+i," W",2)) {
	    sscanf(syscall+i+2,"%d",&(options->movie_width));
	    break;
	  }
	for (i = 0; i < (int)strlen(syscall); i++)
	  if (!strncmp(syscall+i," H",2)) {
	    sscanf(syscall+i+2,"%d",&(options->movie_height));
	    break;
	  }
	for (i = 0; i < (int)strlen(syscall); i++)
		if (!strncmp(syscall+i, " I", 2))
		{
			options->interlacing_info = syscall[i+2];
			break;
		}
	/* If no aspect ration present, use A1:1 */
	options->aspect_enu = 1;
	options->aspect_den = 1;
	for (i = 0; i < (int)strlen(syscall); i++)
	  if (!strncmp(syscall+i, " A", 2)){
	    sscanf(syscall+i+2,"%d:%d",&(options->aspect_enu),
		   &(options->aspect_den));
	  }
	/* Set preview aspect ratio */
	if (strncmp(picture_aspect,"4:3",8))
	  lavedit_effects_preview_width = (lavedit_effects_preview_height*16)/9;
	else
	  lavedit_effects_preview_width = (lavedit_effects_preview_height*4)/3;

	options->image = NULL;
	options->font = NULL;
	options->fontdesc = NULL;

	if (strcmp(data, "image")==0)
	{
		options->type = GTK_EFFECT_IMAGE;
	}
	else if (strcmp(data, "text")==0)
	{
		options->type = GTK_EFFECT_TEXT;
	}

	effects_image_overlay_show_window(options);
}

GtkWidget *get_effects_notebook_page()
{
	GtkWidget *hbox3, *vbox2, *button;

	vbox2 = gtk_vbox_new(FALSE, 2);
	hbox3 = gtk_hbox_new(FALSE, 10);

	button = gtk_image_label_button(" Create Scene Transition ",
			"Create a Scene Transition between current and next Scene",
			(gchar**)effect_transition_xpm, 0, GTK_POS_BOTTOM);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(lavedit_effects_create_scene_transition), NULL);
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	hbox3 = gtk_hbox_new(FALSE, 10);

	button = gtk_image_label_button(" Create Image Overlay ",
			"Create an Image Overlay over a Video Scene",
			(gchar**)effect_picture_xpm, 0, GTK_POS_BOTTOM);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(lavedit_effects_create_overlay), "image");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	hbox3 = gtk_hbox_new(FALSE, 10);

	button = gtk_image_label_button(" Create Text Overlay ",
			"Create an Text Overlay over a Video Scene",
			(gchar**)effect_text_xpm, 0, GTK_POS_BOTTOM);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(lavedit_effects_create_overlay), "text");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	hbox3 = gtk_hbox_new(FALSE, 10);

	button = gtk_image_label_button(" Create Screenshot ",
				"Create Screenshot of the Current Frame",
				scene_screenshot_xpm, 0, GTK_POS_BOTTOM);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(create_filesel3), "screen");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	return vbox2;
}
