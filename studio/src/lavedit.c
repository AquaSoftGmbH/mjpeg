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

#include "studio.h"
#include "pipes.h"
#include "lavedit.h"
#include "gtkfunctions.h"

#include "film.xpm"
#include "file_new.xpm"
#include "file_save.xpm"
#include "file_open.xpm"
#include "file_widget_open.xpm"
#include "scene_add.xpm"
#include "scene_split.xpm"
#include "scene_delete.xpm"
#include "scene_detection.xpm"
#include "scene_left.xpm"
#include "scene_right.xpm"
#include "scene_screenshot.xpm"
#include "arrow_left.xpm"
#include "arrow_right.xpm"
#include "editor_play.xpm"
#include "editor_play_i.xpm"
#include "editor_pause.xpm"
#include "editor_step.xpm"
#include "editor_step_i.xpm"
#include "editor_fast.xpm"
#include "editor_fast_i.xpm"

#define NUM_ADD_MOVIE_SCENES 6

/* Variables */
/* these are for the add-movie-notebook */
char current_open_movies[5][256];
GtkWidget *current_open_movies_combo;
GtkWidget *add_movie_images[NUM_ADD_MOVIE_SCENES];
int add_movie_scene_page, add_movie_scene_selected_scene = 0;
int scene_detection_reply = 0;

int movie_number[MAX_NUM_SCENES], number_of_movies;
int current_position, total_frames_edit;
GtkObject *lavedit_slider_adj;
GtkWidget *lavedit_tv, *hbox_scrollwindow;
GtkWidget *label_lavedit_time, *label_lavedit_frames;
char editlist_savefile[256];
int wait_for_add_frame_confirmation = 0;
char movie_array[MAX_NUM_SCENES][256];

/* Forward declarations */
void lavplay_edit_stopped(void);
char *check_editlist(char *editlist);
static void display_max_num_scenes_warning(void);
GtkWidget *create_tv_stuff(GtkWidget *window);
void set_lavplay_edit_log(int norm, int cur_pos, int cur_speed);
void quit_lavplay_edit_with_error(char *msg);
void process_lavplay_edit_input(char *msg);
void lavplay_edit_input(gpointer data, gint source, GdkInputCondition condition);
void create_lavplay_edit_child(void);
void file_ok_sel_save_as( GtkWidget *w, GtkFileSelection *fs );
void file_ok_sel_screeny( GtkWidget *w, GtkFileSelection *fs );
void file_ok_sel_open( GtkWidget *w, GtkFileSelection *fs );
void set_add_scene_movie_selection(GtkWidget *widget, gpointer data);
void file_ok_sel_add_scene_to_glist( GtkWidget *w, GtkFileSelection *fs );
void prepare_for_scene_detection( GtkWidget *w, GtkFileSelection *fs );
void add_scene_movie_change_page( GtkWidget *widget, char *direction);
int open_add_movie_scene_editlist(void);
void add_movie_scene_image_clicked( GtkWidget *widget, gpointer data);
static void create_filesel3(GtkWidget *widget, char *what_to_do);
void clear_editlist(GtkWidget *widget, gpointer data);
void save_eli_file(char *target);
void image_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data);
void lavplay_edit_slider_value_changed(GtkAdjustment *adj);
void command_to_lavplay_edit(GtkWidget *widget, char *data);
void add_scene_to_editlist( GtkWidget *widget, gpointer data );
#if 0
void add_new_movie_scene( GtkWidget *w, GtkFileSelection *fs );
#endif
void play_edit_file(GtkWidget *widget, gpointer data);
void split_scene(GtkWidget *widget, gpointer data);
void delete_scene(GtkWidget *widget, gpointer data);
void scene_move(GtkWidget *widget, gpointer data);
GtkWidget *get_editing_routines_notebook_page(void);
GtkWidget *get_add_movie_notebook_page(void);

/* ================================================================= */

/* returns NULL if 'editlist' is not a movie/editlist
 * returns editlist if editlist is an editlist itself
 * else returns the name of the newly created editlist
 */
char *check_editlist(char *editlist)
{
	FILE *fp;
	char tempchar[256];

	if (NULL == (fp = fopen(editlist,"r")))
	{
		printf("Oops, error opening %s (check_editlist)\n",editlist);
		return NULL;
	}
	if (fgets(tempchar,255,fp) == NULL)
	{
		printf("Error reading the file!\n");
		fclose(fp);
		return NULL;
	}
	if (strcmp(tempchar, "LAV Edit List\n") != 0)
	{
		char command[256];
		static char file[200];
		int x,i=0;

		/* Not an editlist, so create one */
		printf("Not an editlist!!!\n");
		fclose(fp);

		for (x=0;x<strlen(editlist);x++)
		{
			if (editlist[x] == '/')
				i=x;
		}

		sprintf(file, "%s/.studio/%s.eli", getenv("HOME"),
			editlist+i+1);
		sprintf(command, "%s -S %s -T -1 %s%s",
			LAV2YUV_LOCATION, file, editlist,
			verbose?"":" >> /dev/null 2>&1");
		system(command);

		/* this means we're done - scene_detection_filename is ready */
		if (NULL == (fp = fopen(file,"r")))
		{
			printf("Oops, error opening %s (check_editlist (2))\n",
				file);
			return NULL;
		}
		if (fgets(tempchar,255,fp) == NULL)
		{
			printf("Error reading the file!\n");
			fclose(fp);
			return NULL;
		}
		if (strcmp(tempchar, "LAV Edit List\n") != 0)
		{
			printf("%s is still not an editlist, giving up\n",
				file);
			fclose(fp);
			return NULL;
		}

		/* okay, we've got an editlist now */
		fclose(fp);
		return file;
	}

	/* we got here, so it's an editlist */
	fclose(fp);
	return editlist;
}

/* displays a warning that we're passing MAX_NUM_SCENES */
static void display_max_num_scenes_warning()
{
	char text[512];

	sprintf(text, "Warning: you were just trying to add a new scene\n"
		"to your scenelist, but you've already passed\n"
		"the maximal number of scenes that Linux Video\n"
		"Studio was compiled with (%d).\n\n"
		"Please recompile and use --with-max-num-scenes\n"
		"during configuration to change this.", MAX_NUM_SCENES);

	gtk_show_text_window(STUDIO_WARNING, text, NULL);
}

GtkWidget *create_tv_stuff(GtkWidget *window)
{
	GtkWidget *vbox2, *hbox3, *pixmap_widget, *scrollbar, *button;
	/*GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkStyle *style;*/
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();
/*	style = gtk_widget_get_style(window);*/

	vbox2 = gtk_vbox_new(FALSE,0);

	lavedit_tv = gtk_event_box_new();
	gtk_box_pack_start (GTK_BOX (vbox2), lavedit_tv, FALSE, FALSE, 0);
	gtk_widget_set_usize(GTK_WIDGET(lavedit_tv), tv_width_edit, tv_height_edit);
	gtk_widget_show(lavedit_tv);
	set_background_color(lavedit_tv,0,0,0);
	lavedit_slider_adj = gtk_adjustment_new(0, 0, 2500000, 10000, 100000, 0);
	gtk_signal_connect (GTK_OBJECT (lavedit_slider_adj), "value_changed",
		GTK_SIGNAL_FUNC (lavplay_edit_slider_value_changed), NULL);
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (lavedit_slider_adj));

	gtk_range_set_update_policy (GTK_RANGE (scrollbar), GTK_UPDATE_CONTINUOUS);
	gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 0);
	gtk_box_pack_start(GTK_BOX (vbox2), scrollbar, FALSE, FALSE, 10);
	gtk_widget_show(scrollbar);

	hbox3 = gtk_hbox_new(TRUE, 5);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_fast_i_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_fast_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Fast Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay_edit),"p-3");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_play_i_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_play_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay_edit),"p-1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_step_i_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_step_i_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "One Frame Backwards", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay_edit),"-");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_pause_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_pause_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Pause", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay_edit),"p0");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_step_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_step_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "One Frame Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay_edit),"+");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_play_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_play_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay_edit),"p1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(button);

	button = gtk_button_new();
/*	pixmap = gdk_pixmap_create_from_xpm_d( window->window, &mask,
		&style->bg[GTK_STATE_NORMAL],(gchar **)editor_fast_xpm );
	pixmap_widget = gtk_pixmap_new(pixmap, mask);*/
	pixmap_widget = gtk_widget_from_xpm_data(editor_fast_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Play Fast Forward", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(command_to_lavplay_edit),"p3");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 10);
	gtk_widget_set_usize(button, 32, 32);
	gtk_widget_show(hbox3);

	return vbox2;
}

void lavplay_edit_slider_value_changed(GtkAdjustment *adj)
{
	double val;
	char out[256];

	if (pipe_is_active(LAVPLAY_E) && !adjustment_b_changed)
	{
		val = adj->value / 2500000;
		sprintf(out,"s%ld",(long)(val*total_frames_edit));
		command_to_lavplay_edit(NULL,out);
	}
	else
		adjustment_b_changed = FALSE;
}

void set_lavplay_edit_log(int norm, int cur_pos, int cur_speed)
{
	int i;
	int f, h, m, s;
	char temp[256];

	s = cur_pos/(norm=='p'?25:30);
	f = cur_pos%(norm=='p'?25:30);
	m = s/60;
	s = s%60;
	h = m/60;
	m = m%60;

	adjustment_b_changed = TRUE;
	if (lavedit_slider_adj)
		gtk_adjustment_set_value(GTK_ADJUSTMENT(lavedit_slider_adj),
			((2500000 / total_frames_edit)*(cur_pos+1)));

	current_position = cur_pos;

	sprintf(temp, "%d/%d", cur_pos, total_frames_edit);
	gtk_label_set_text(GTK_LABEL(label_lavedit_frames), temp);

	sprintf(temp, "%2d:%2.2d:%2.2d:%2.2d",h,m,s,f); 
	gtk_label_set_text(GTK_LABEL(label_lavedit_time), temp);

	for (i=0;i<number_of_files;i++)
	{
		if (!image[i])
		{
			printf("***WARNING: number_of_files was not set correctly\n");
			number_of_files = i-1;
			break;
		}

		if (cur_pos >= GTK_IMAGEPLUG(image[i])->start_total &&
			cur_pos <= GTK_IMAGEPLUG(image[i])->start_total +
			GTK_IMAGEPLUG(image[i])->stopframe -
			GTK_IMAGEPLUG(image[i])->startframe)
		{
			if (i != current_image)
			{
				if (current_image >=0 && current_image < number_of_files)
				{
					GTK_IMAGEPLUG(image[current_image])->selection = 0;
					gtk_imageplug_draw(image[current_image]);
				}
				current_image = i;
				GTK_IMAGEPLUG(image[current_image])->selection = 1;
				gtk_imageplug_draw(image[current_image]);
			}
		}
	}
}

void quit_lavplay_edit_with_error(char *msg)
{
	quit_lavplay_edit();
	if (verbose) g_print("Lavplay error: %s\n", msg);
	gtk_show_text_window(STUDIO_ERROR, "Lavplay returned an error:", msg);
}

void process_lavplay_edit_input(char *msg)
{
	char norm;
	int cur_pos, cur_speed;
#if 0
	/* TODO: remove once the add-new-movie-from-scene works */
	if(strncmp(msg, "--DEBUG: ---", 12) == 0 && wait_for_add_frame_confirmation)
	{
		int a,b,i,diff;
		char filename_img_tmp[256], command[256];
		char temp[256];
		GdkPixbuf *temp2;

		sscanf(msg+13,"Added frames %d - %d from movie %s",&a,&b,temp);
		GTK_IMAGEPLUG(image[current_image+1])->startframe = a;
		GTK_IMAGEPLUG(image[current_image+1])->stopframe = b;
		GTK_IMAGEPLUG(image[current_image+1])->startscene = a;
		GTK_IMAGEPLUG(image[current_image+1])->stopscene = b;
		if (current_image < number_of_files-2)
		{
			diff = b-a+1;
			for (i=current_image+2;i<number_of_files;i++)
			{
				GTK_IMAGEPLUG(image[i])->start_total += diff;
			}
		}
		wait_for_add_frame_confirmation = 0;

		sprintf(filename_img_tmp, "%s/.studio/.temp.jpg",
			getenv("HOME"),number_of_files);
		sprintf(command, "%s -o %s -f i %s -i %d%s",
			LAVTRANS_LOCATION, filename_img_tmp, temp, a,
			verbose?"":" >> /dev/null 2>&1");
		system(command);
		temp2 = gdk_pixbuf_new_from_file (filename_img_tmp);
		unlink(filename_img_tmp);

		GTK_IMAGEPLUG(image[current_image+1])->picture =
			gdk_pixbuf_scale_simple(temp2,
			GTK_IMAGEPLUG(image[current_image])->width,
			GTK_IMAGEPLUG(image[current_image])->height,
			GDK_INTERP_NEAREST);

		if (current_image>=0)
		{
			GTK_IMAGEPLUG(image[current_image])->selection = 0;
			gtk_imageplug_draw(image[current_image]);
		}
		current_image++;
		GTK_IMAGEPLUG(image[current_image])->selection = 1;
		gtk_imageplug_draw(image[current_image]);

		save_eli_temp_file();
		if (verbose) printf("Editlist updated!\n");
	}
#endif
	if(msg[0]=='@')
	{
		sscanf(msg+1,"%c%d/%d/%d",&norm,&cur_pos,&total_frames_edit,&cur_speed);
		set_lavplay_edit_log(norm, cur_pos, cur_speed);
		return;
	}

	else if (strncmp(msg, "**ERROR:", 8) == 0)
	{
		/* Error handling */
		quit_lavplay_edit_with_error(msg+9);
	}
	else if (strncmp(msg, "--DEBUG:", 8)==0)
		return;
}

void quit_lavplay_edit()
{
	close_pipe(LAVPLAY_E);
}

void lavplay_edit_stopped()
{
	if (lavedit_slider_adj)
		gtk_adjustment_set_value(GTK_ADJUSTMENT(lavedit_slider_adj), 0);
}

void create_lavplay_edit_child()
{
	char *lavplay_command[256];
	char temp1[256], temp2[256];
	int n;

	char SDL_windowhack[32];
	sprintf(SDL_windowhack,"%ld",GDK_WINDOW_XWINDOW(lavedit_tv->window));
	setenv("SDL_WINDOWID", SDL_windowhack, 1);

	n=0;
	lavplay_command[n] = LAVPLAY_LOCATION; n++;
	lavplay_command[n] = "-q"; n++;
	lavplay_command[n] = "-g"; n++;
	lavplay_command[n] = "-v"; n++;
	lavplay_command[n] = "1"; n++;
	if (encoding_syntax_style != 140)
	{
		lavplay_command[n] = "--size"; n++;
		sprintf(temp2, "%dx%d", tv_width_edit, tv_height_edit);
		lavplay_command[n] = temp2; n++;
	}
	lavplay_command[n] = encoding_syntax_style==140?"-S":"-pS"; n++;
	sprintf(temp1, "%s/.studio/%s", getenv("HOME"), editlist_filename);
	lavplay_command[n] = temp1; n++;
	lavplay_command[n] = NULL;

	start_pipe_command(lavplay_command, LAVPLAY_E); /* lavplay */
}

void file_ok_sel_save_as( GtkWidget *w, GtkFileSelection *fs )
{
	char *file;

	file = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
	if (verbose) printf("(Save as/Save) File: %s\n", file);
	save_eli_file(file);
}

void file_ok_sel_open( GtkWidget *w, GtkFileSelection *fs )
{
	char *file;

	file = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
	if (verbose) printf("(Open) File: %s\n", file);
	open_eli_file(file);

	//create_lavplay_edit_child();
}

void file_ok_sel_screeny( GtkWidget *w, GtkFileSelection *fs )
{
	char *file;
	char command[256], temp[256];

	file = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
	if (verbose) printf("(Screenshot to) File: %s\n", file);

	sprintf(temp, "%s/.studio/%s", getenv("HOME"), editlist_filename);
	sprintf(command, "%s -o %s -f i %s -i %d%s",
		LAVTRANS_LOCATION, file, temp, current_position,
			verbose?"":" >> /dev/null 2>&1");
	system(command);

	sprintf(temp, "Image saved to %s", file);
	gtk_show_text_window(STUDIO_INFO, temp, NULL);
}

void prepare_for_scene_detection( GtkWidget *w, GtkFileSelection *fs )
{
	extern int current_file;
	extern char files_recorded[256][256];

	current_file = 1;
	strcpy(files_recorded[0],
		gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));

	scene_detection();
}

void file_ok_sel_add_scene_to_glist( GtkWidget *w, GtkFileSelection *fs )
{
	/* add selected movie (editlist?) to add-movie-notebook */
	char *file;
	GList *temp = NULL;
	int i;

	/* we should check whether it's an editlist here.
	 * if not, we should offer scene detection
	 */
	file = check_editlist(gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
	if (file == NULL) return; /* not an editlist/movie */

	/* check length of list, rather not make it > 5 */
	if (strcmp(current_open_movies[4], "") != 0)
	{
		for (i=0;i<4;i++)
			sprintf(current_open_movies[i], current_open_movies[i+1]);
	}

	for(i=0;i<5;i++)
	{
		if (strcmp(current_open_movies[i], "")==0)
		{
			sprintf(current_open_movies[i], file);
			break;
		}
	}

	/* add newly-opened movie to the selection */
	for(i=0;i<5;i++)
	{
		if (strcmp(current_open_movies[i],"")==0)
			break;
		temp = g_list_append(temp, current_open_movies[i]);
	}

	/* tell the combo-box that we have a new member */
	gtk_combo_set_popdown_strings (GTK_COMBO (current_open_movies_combo),
		temp);

	/* set the new movie to be the active one (for the signal handler) */
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry),file);
}

void add_scene_movie_change_page(GtkWidget *widget, char *direction)
{
	/* go one page forward or back in the add-movie-notebook page */
	/* direction = 1 means =>, direction = -1 means <= */
	int x;

	if (add_movie_scene_selected_scene != 0)
	{
		GTK_IMAGEPLUG(add_movie_images[add_movie_scene_selected_scene])->selection = 0;
		gtk_imageplug_draw(add_movie_images[add_movie_scene_selected_scene]);
		add_movie_scene_selected_scene = 0;
		GTK_IMAGEPLUG(add_movie_images[add_movie_scene_selected_scene])->selection = 1;
		gtk_imageplug_draw(add_movie_images[add_movie_scene_selected_scene]);
	}

	add_movie_scene_page += atoi(direction);
	if (add_movie_scene_page < 0) add_movie_scene_page = 0;

	while (1)
	{
		x = open_add_movie_scene_editlist();
		if (x!=1) break;
	}

	if (x==-1)
	{
		char temp[256];

		/* oops something went wrong */
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry), "");
		sprintf(temp, "Error opening %s",
			gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry)));
		gtk_show_text_window(STUDIO_WARNING, temp, NULL);
	}
}

int open_add_movie_scene_editlist()
{
	/* open the current editlist and display the scenes */
	char *file_selected;
	char movies[256][256];
	char temp_entry[256];
	FILE *fp;
	int num_movs, a,b,c,d,e,x,y, total=0;

	file_selected = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry));

	/* "" needs no opening, ignore it */
	if (strcmp(file_selected, "")==0) return 0;

	if (NULL == (fp = fopen(file_selected,"r")))
	{
		printf("Oops, error opening %s (open_add_movie_scene_editlist)\n",file_selected);
		return -1;
	}
	if (fgets(temp_entry,255,fp) == NULL)
	{
		printf("Error reading the file!\n");
		fclose(fp);
		return -1;
	}
	if (strcmp(temp_entry,"LAV Edit List\n") != 0)
	{
		printf("Not an editlist!!!\n");
		fclose(fp);
		return -1;
	}
	if (NULL != fgets(temp_entry,255,fp))
	{
		if (strcmp(temp_entry, "NTSC\n") == 0)
			pal_or_ntsc = 'n';
		else
			pal_or_ntsc = 'p';
	}
	if (NULL != fgets(temp_entry,255,fp))
	{
		sscanf(temp_entry, "%d\n", &num_movs);
	}
	for(x=0;x<num_movs;x++)
	{
		fgets(movies[x], 255, fp); /* the movies */
	}

	/* get rid of '\n's */
	for(x=0;x<num_movs;x++)
	{
		for(y=0;y<strlen(movies[x]);y++)
			if (movies[x][y] == '\n')
			{
				movies[x][y] = '\0';
				break;
			}				
	}

	/* skip a number of pages */
	for (x=0;x<add_movie_scene_page*NUM_ADD_MOVIE_SCENES;x++)
	{
		if (fgets(temp_entry,255,fp) == NULL)
		{
			add_movie_scene_page--;
			fclose (fp);
			return 1; /* means re-open */
		}
		else
		{
			y = sscanf(temp_entry, "%d %d %d (%d %d)\n",&a,&b,&c,&d,&e);
			total += c-b+1;
		}
	}

	for (x=0;x<NUM_ADD_MOVIE_SCENES;x++)
	{
		if (fgets(temp_entry,255,fp) == NULL)
		{
			if (x==0)
			{
				add_movie_scene_page--;
				fclose (fp);
				return 1; /* means re-open */
			}
			else
			{
				gtk_imageplug_set_data(
					GTK_IMAGEPLUG(add_movie_images[x]),
					NULL,"",0,0,0,0,0);
			}
		}
		else
		{
			char command[256], file[256];
			GdkPixbuf *temp;

			y = sscanf(temp_entry, "%d %d %d (%d %d)\n", &a, &b, &c, &d, &e);
			sprintf(file, "%s/.studio/.temp.jpg", getenv("HOME"));
			sprintf(command, "%s -f i -o %s -i %d %s%s",
				LAVTRANS_LOCATION, file, total, file_selected,
				verbose?"":" >> /dev/null 2>&1");
			system(command);

			temp = gdk_pixbuf_new_from_file (file);
			unlink(file);

			gtk_imageplug_set_data(
				GTK_IMAGEPLUG(add_movie_images[x]),
				gdk_pixbuf_scale_simple(temp,
					GTK_IMAGEPLUG(add_movie_images[x])->width,
					GTK_IMAGEPLUG(add_movie_images[x])->height,
					GDK_INTERP_NEAREST),
				movies[a], y==5?d:b, y==5?e:c, b, c, total);

			total += c-b+1;
		}
	}

	fclose(fp);
	return 0;
}

void set_add_scene_movie_selection(GtkWidget *widget, gpointer data)
{
	/* a movie in the add-movie-combobox has been selected, open it! */
	int x = 1;

	add_movie_scene_page = 0;
	while (1)
	{
		x = open_add_movie_scene_editlist();
		if (x!=1) break;
	}

	if (x==-1)
	{
		char temp[256];

		/* oops something went wrong */
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry), "");
		sprintf(temp, "Error opening %s",
			gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry)));
		gtk_show_text_window(STUDIO_WARNING, temp, NULL);
	}
}

void add_scene_to_editlist(GtkWidget *widget, gpointer data)
{
	/* a scene has been selected to be added to the editlist, do it! */
	char temp[256], file_jpg[256];
	int i, diff;
	GdkPixbuf *temp_buf;
	GtkImagePlug *im = GTK_IMAGEPLUG(add_movie_images[add_movie_scene_selected_scene]);

	if (number_of_files >= MAX_NUM_SCENES)
	{
		display_max_num_scenes_warning();
		return;
	}

	/* if it's data-less */
	if (im->picture == NULL)
		return;

	sprintf(temp, "ea %s %d %d %d",
		im->video_filename,
		im->startframe,
		im->stopframe,
		current_image == -1 ? 0 : GTK_IMAGEPLUG(image[current_image])->stopframe+1); 
	command_to_lavplay_edit(NULL,temp);

	/* Move images one step further to make space for the new one */
	if (current_image < number_of_files-1 || number_of_files == 0)
	{
		if (number_of_files > 0)
		{
			diff = im->stopframe - im->startframe + 1;

			for (i=number_of_files;i>current_image+1;i--)
			{
				if (i == number_of_files)
				{
					image[i] = gtk_imageplug_new(
						GTK_IMAGEPLUG(image[i-1])->picture,
						GTK_IMAGEPLUG(image[i-1])->video_filename,
						GTK_IMAGEPLUG(image[i-1])->startscene,
						GTK_IMAGEPLUG(image[i-1])->stopscene,
						GTK_IMAGEPLUG(image[i-1])->startframe,
						GTK_IMAGEPLUG(image[i-1])->stopframe,
						GTK_IMAGEPLUG(image[i-1])->start_total+diff,
						GTK_IMAGEPLUG(image[i-1])->background, i);
					gtk_box_pack_start(GTK_BOX(hbox_scrollwindow),
						image[i], FALSE, FALSE, 0);
					gtk_signal_connect(GTK_OBJECT(image[i]),
						"button_press_event",
						GTK_SIGNAL_FUNC(image_clicked), NULL);
					gtk_widget_show(image[i]);
				}
				else
				{
					gtk_imageplug_copy(
						GTK_IMAGEPLUG(image[i]), 
						GTK_IMAGEPLUG(image[i-1]));
					GTK_IMAGEPLUG(image[i])->start_total += diff;
				}
			}
		}

		/* Add picture to array (current_image+1) */
		if (number_of_files == 0 && current_image == -1)
		{
			if (verbose) printf("Bad, bad programmer! "
				"If number_of_files == 1, current_image should be -1!!!\n");
			current_image = -1;
		}

		GTK_IMAGEPLUG(image[current_image+1])->selection = 0;

		sprintf(file_jpg, "%s/.studio/.temp.jpg", getenv("HOME"));
		sprintf(temp, "%s -f i -i %d -o %s %s%s",
			LAVTRANS_LOCATION, im->start_total,
			file_jpg, im->video_filename,
			verbose?"":" >> /dev/null 2>&1");
		system(temp);
		temp_buf = gdk_pixbuf_new_from_file (file_jpg);
		unlink(file_jpg);
		gtk_imageplug_set_data(
			GTK_IMAGEPLUG(image[current_image+1]),
				gdk_pixbuf_scale_simple(temp_buf,
					im->width, im->height,
					GDK_INTERP_NEAREST),
				im->video_filename, im->startscene, im->stopscene,
				im->startframe, im->stopframe,
				current_image==-1 ? 0 :
				GTK_IMAGEPLUG(image[current_image])->start_total +
				GTK_IMAGEPLUG(image[current_image])->stopframe - 
				GTK_IMAGEPLUG(image[current_image])->startframe + 1 );
	}
	else
	{
		i = number_of_files;
/*		image[i] = gtk_imageplug_new_from_video(im->video_filename,
			im->startframe, im->stopframe,
			im->startscene, im->stopscene,
			GTK_IMAGEPLUG(image[i-1])->start_total +
			GTK_IMAGEPLUG(image[i-1])->stopframe - 
			GTK_IMAGEPLUG(image[i-1])->startframe + 1,
			GTK_IMAGEPLUG(image[i-1])->background, i );*/
		image[i] = gtk_imageplug_new(
			im->picture, im->video_filename, im->startscene,
			im->stopscene, im->startframe, im->stopframe,
			GTK_IMAGEPLUG(image[i-1])->start_total +
			GTK_IMAGEPLUG(image[i-1])->stopframe - 
			GTK_IMAGEPLUG(image[i-1])->startframe + 1,
			GTK_IMAGEPLUG(image[i-1])->background, i);
		gtk_box_pack_start(GTK_BOX(hbox_scrollwindow),
			image[i], FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(image[i]), "button_press_event",
			GTK_SIGNAL_FUNC(image_clicked), NULL);
		gtk_widget_show(image[i]);
	}

	/* If, in the movie list, 'file' is not there - add it */
	for (i=0;i<number_of_movies;i++)
	{
		if(strcmp(movie_array[i], im->video_filename) == 0)
			break;
		else if(i == number_of_movies - 1)
		{
			strcpy(movie_array[number_of_movies], im->video_filename);
			number_of_movies++;
			break;
		}
	}
	/* if number_of_movies == 0 ... */
	if (number_of_movies == 0)
	{
		strcpy(movie_array[number_of_movies], im->video_filename);
		number_of_movies++;
	}

	number_of_files++;
	save_eli_temp_file();
}

void add_movie_scene_image_clicked(GtkWidget *widget, gpointer data)
{
	/* An image in the add-movie-scene-list has been clicked, activate! */
	/* number is the number of the image (0-7) */
	if (GTK_IMAGEPLUG(widget)->picture == NULL)
	{
		/*GTK_IMAGEPLUG(widget)->selection = 0;
		gtk_imageplug_draw(widget);*/
		return;
	}

	if (verbose)
		printf("Add-movie-scene-image %d gave clicked signal\n",
			GTK_IMAGEPLUG(widget)->number);

	if (add_movie_scene_selected_scene != GTK_IMAGEPLUG(widget)->number)
	{
		GTK_IMAGEPLUG(add_movie_images[add_movie_scene_selected_scene])->
			selection = 0;
		gtk_imageplug_draw(add_movie_images[add_movie_scene_selected_scene]);
		add_movie_scene_selected_scene = GTK_IMAGEPLUG(widget)->number;
	}
}

#if 0
/* TODO: remove once the add-movie-from-scene works */
void add_new_movie_scene( GtkWidget *w, GtkFileSelection *fs )
{
	char *file;
	char temp[256];
	int i;

	if (number_of_files >= MAX_NUM_SCENES)
	{
		display_max_num_scenes_warning();
		return;
	}

	file = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs));
	if (verbose) printf("(Add new movie/scene) File: %s\n", file);

	sprintf(temp, "ea %s %d %d %d", file, -1, -1,
		current_image == -1 ? 0 : GTK_IMAGEPLUG(image[current_image])->stopframe+1); 
	command_to_lavplay_edit(NULL,temp);

	/* Move images one step further to make space for the new one */
	if (current_image < number_of_files-1)
	{
		for (i=number_of_files;i>current_image+1;i--)
		{
			if (i == number_of_files)
			{
				image[i] = gtk_imageplug_new(
					GTK_IMAGEPLUG(image[i-1])->picture,
					GTK_IMAGEPLUG(image[i-1])->video_filename,
					GTK_IMAGEPLUG(image[i-1])->startscene,
					GTK_IMAGEPLUG(image[i-1])->stopscene,
					GTK_IMAGEPLUG(image[i-1])->startframe,
					GTK_IMAGEPLUG(image[i-1])->stopframe,
					GTK_IMAGEPLUG(image[i-1])->start_total,
					GTK_IMAGEPLUG(image[i-1])->background, i);
				gtk_box_pack_start(GTK_BOX(hbox_scrollwindow),
					image[i], FALSE, FALSE, 0);
				gtk_signal_connect(GTK_OBJECT(image[i]),
					"button_press_event",
					GTK_SIGNAL_FUNC(image_clicked), NULL);
				gtk_widget_show(image[i]);
			}
			else
			{
				gtk_imageplug_copy(image[i], image[i-1]);
			}
		}

		/* Add picture to array (current_image+1) */
		GTK_IMAGEPLUG(image[current_image+1])->selection = 0;
		strcpy(GTK_IMAGEPLUG(image[current_image+1])->video_filename,
			file);
		gtk_imageplug_draw(image[current_image+1]);
	}
	else
	{
		i = number_of_files;
		image[i] = gtk_imageplug_new_from_video(file,0,	0, 0, 0,
			GTK_IMAGEPLUG(image[i-1])->start_total +
			GTK_IMAGEPLUG(image[i-1])->stopframe - 
			GTK_IMAGEPLUG(image[i-1])->startframe + 1,
			GTK_IMAGEPLUG(image[i-1])->background,i);
		gtk_box_pack_start(GTK_BOX(hbox_scrollwindow),
			image[i], FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(image[i]), "button_press_event",
			GTK_SIGNAL_FUNC(image_clicked), NULL);
		gtk_widget_show(image[i]);
	}

	/* If, in the movie list, 'file' is not there - add it */
	for (i=0;i<number_of_movies;i++)
	{
		if(strcmp(movie_array[i], file) == 0)
			break;
		else if(i == number_of_movies - 1)
		{
			strcpy(movie_array[number_of_movies], file);
			number_of_movies++;
			break;
		}
	}

	/* This tells us to wait for the info from the command line
	to update the start_total of all images after the added one
	and the stopframes from the added scene */
	wait_for_add_frame_confirmation = 1;
	number_of_files++;
	//save_eli_temp_file();
}
#endif

void create_filesel3(GtkWidget *widget, char *what_to_do)
{
	GtkWidget *filew;
	char *temp = NULL;
	
	filew = gtk_file_selection_new ("Linux Video Studio - Select Location");

	if (strcmp(what_to_do, "save_as") == 0)
	{
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
			"clicked", (GtkSignalFunc) file_ok_sel_save_as, filew);
		temp = editlist_savefile;
	}
	else if (strcmp(what_to_do, "open") == 0)
	{
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
			"clicked", (GtkSignalFunc) file_ok_sel_open, filew);
		temp = editlist_savefile;
	}
	else if (strcmp(what_to_do, "screen") == 0)
	{
		if (current_position >= 0)
		{
			gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
				"clicked", (GtkSignalFunc) file_ok_sel_screeny, filew);
			temp = "image.jpg";
		}
		else
		{
			/* give warning */
		}
	}
#if 0
	else if (strcmp(what_to_do, "add_new") == 0)
	{
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
			"clicked", (GtkSignalFunc) add_new_movie_scene, filew);
		temp = "movie.avi";
	}
#endif
	else if (strcmp(what_to_do, "add_movie_selection") == 0)
	{
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
			"clicked", (GtkSignalFunc) file_ok_sel_add_scene_to_glist, filew);
		temp = "scene_list.eli";
	}
	else if (strcmp(what_to_do, "scene_detection") == 0)
	{
		gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
			"clicked", (GtkSignalFunc) prepare_for_scene_detection, filew);
		temp = "movie.avi";
	}


	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->ok_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(filew)->cancel_button),
		"clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT (filew));
	gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), temp);
	gtk_widget_show(filew);

}

void play_edit_file(GtkWidget *widget, gpointer data)
{
	if (!pipe_is_active(LAVPLAY_E) && number_of_files > 0)
	{
		create_lavplay_edit_child();
	}
}

void command_to_lavplay_edit(GtkWidget *widget, char *data)
{
	char command[256];

	sprintf(command, "%s\n", data);
	write_pipe(LAVPLAY_E,command);
}

void image_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	GtkImagePlug *imageplug;
	imageplug = GTK_IMAGEPLUG(widget);

	if (imageplug->picture == NULL)
	{
		/*imageplug->selection = 0;
		gtk_imageplug_draw(widget);*/
		return;
	}

	if (event->type == GDK_2BUTTON_PRESS || event->type==GDK_3BUTTON_PRESS)
	{
		if (verbose) printf("Image %d gave double-clicked signal, "
			"starting frame editor...\n",
			imageplug->number);

		open_frame_edit_window();
	}
	else
	{
		char out[256];

		if (verbose) printf("Image %d gave clicked signal, "
			"going to frame %d now\n",
			imageplug->number, imageplug->start_total);

		sprintf(out,"s%d",imageplug->start_total);
		command_to_lavplay_edit(NULL,out);

		if (current_image != imageplug->number)
		{
			if (current_image>=0)
			{
				GTK_IMAGEPLUG(image[current_image])->selection = 0;
				gtk_imageplug_draw(image[current_image]);
			}

			current_image = imageplug->number;
		}
	}
}

void clear_editlist(GtkWidget *widget, gpointer data)
{
	int line;

	for (line=1;line<MAX_NUM_SCENES;line++)
	{
		if (image[line] != NULL)
		{
			gtk_widget_hide(image[line]);
			gtk_widget_destroy(image[line]);
			image[line] = NULL;
		}
		else
			break;
	}
	current_image = -1;
	current_position = -1;
	number_of_files = 0;
	gtk_imageplug_set_data(GTK_IMAGEPLUG(image[0]), NULL, "", 0,0,0,0,0);

	save_eli_temp_file();
}

int open_eli_file(char *filename)
{
	int line,a,b,c,d,e,x;
	FILE *fp;
	GdkPixbuf *bg;
	char temp_entry[256];
	char filename_temp[256];

	quit_lavplay_edit();

	for (line=0;line<MAX_NUM_SCENES;line++)
	{
		if (image[line] != NULL)
		{
			gtk_widget_hide(image[line]);
			gtk_widget_destroy(image[line]);
			image[line] = NULL;
		}
		else
			break;
	}

	bg = gdk_pixbuf_new_from_xpm_data ((const char **)film_xpm);

	if (NULL == (fp = fopen(filename,"r")))
	{
		printf("Oops, error opening %s (open_eli_file)\n",filename);
		goto ERROR_FILE;
	}
	if (fgets(temp_entry,255,fp) == NULL)
	{
		printf("Error reading the file!\n");
		fclose(fp);
		goto ERROR_FILE;
	}
	if (strcmp(temp_entry, "LAV Edit List\n") != 0)
	{
		/* TODO: we should definately offer scene detection here */
		printf("Not an editlist!!!\n");
		fclose(fp);
		goto ERROR_FILE;
	}
	if (fgets(temp_entry,255,fp) == NULL)
	{
		printf("Error reading the file!\n");
		fclose(fp);
		goto ERROR_FILE;
	}
	if (strcmp(temp_entry, "PAL\n")==0)
		pal_or_ntsc = 'p';
	else
		pal_or_ntsc = 'n';
	if (fgets(temp_entry,255,fp) == NULL)
	{
		printf("Error reading the file!\n");
		fclose(fp);
		goto ERROR_FILE;
	}
	sscanf(temp_entry, "%d\n", &number_of_movies);
	for (line=0;line<number_of_movies;line++)
	{
		if (fgets(temp_entry,255,fp) == NULL)
		{
			printf("Error reading the file!\n");
			fclose(fp);
			goto ERROR_FILE;
		}
		for (x=0;x<strlen(temp_entry);x++)
		{
			if (temp_entry[x] == '\n')
			{
				temp_entry[x] = '\0';
				break;
			}
		}
		strcpy(movie_array[line],temp_entry);
	}
	number_of_files = 0; x=0; /* x = 'start_total' */
	while (NULL != fgets(temp_entry,255,fp))
	{
		if (number_of_files>=MAX_NUM_SCENES) break; /* editlist is too big */
		line = sscanf(temp_entry,"%d %d %d (%d %d)",&a,&b,&c,&d,&e);
		if (line!=3 && line!=5) break; /* invalid entry */

		image[number_of_files] = gtk_imageplug_new_from_video(
			movie_array[a], b, c, line==5?d:b, line==5?e:c,x,bg,number_of_files);
		gtk_box_pack_start(GTK_BOX(hbox_scrollwindow),
			image[number_of_files], FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(image[number_of_files]),
			"button_press_event", GTK_SIGNAL_FUNC(image_clicked), NULL);
		gtk_widget_show(image[number_of_files]);

		number_of_files++;
		x += c-b+1;
	}

	if (number_of_files<MAX_NUM_SCENES-1)
		image[number_of_files+1] = NULL;

	fclose(fp);

	if (number_of_files == 0 || number_of_movies == 0)
	{
		printf("Empty editing list file!\n");
		goto ERROR_FILE;
	}

	current_position = -1;
	current_image = -1;
	save_eli_temp_file();
	return 1;

ERROR_FILE:
	sprintf(filename_temp, "%s/.studio/%s", getenv("HOME"), editlist_filename);
	current_image = -1;
	current_position = -1;
	number_of_files = 0;
	image[0] = gtk_imageplug_new_from_video("",0,0,0,0,0,bg, 0);
	image[1] = NULL;
	gtk_box_pack_start(GTK_BOX(hbox_scrollwindow), image[0], FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(image[0]), "button_press_event",
		GTK_SIGNAL_FUNC(image_clicked), NULL);
	gtk_widget_show(image[0]);
	if (strcmp(filename, filename_temp)!=0)
	{
		sprintf(filename_temp, "Error opening %s", filename);
		gtk_show_text_window(STUDIO_WARNING, filename_temp, NULL);
	}
	return 0;
}

void save_eli_file(char *target)
{
	FILE *fp;
	int x,n;
	char temp[256];

	fp = fopen(target,"w");
	if (NULL == fp)
	{
		char temp[256];
		fprintf(stderr,"can't open editlist file %s\n",target);
		sprintf(temp, "Could not save editlist to %s", target);
		gtk_show_text_window(STUDIO_WARNING, temp, NULL);
		return;
	}

	fprintf(fp,"LAV Edit List\n");
	if (verbose) fprintf(stderr,"Writing - LAV Edit List\n");

	if (pal_or_ntsc == 'p')
	{
		if (verbose) fprintf(stderr,"Writing - PAL\n");
		fprintf(fp,"PAL\n");
	}
	else
	{
		if (verbose) fprintf(stderr,"Writing - NTSC\n");
		fprintf(fp,"NTSC\n");
	}

	/* clean up movie_array */
	for (n=0;n<number_of_movies;n++)
	{
		int m;
		for (m=0;m<n;m++)
		{
			if (strcmp(movie_array[n],movie_array[m])==0)
			{
				int i;
				for (i=n+1;i<number_of_movies;i++)
				{
					sprintf(movie_array[i-1], movie_array[i]);
				}
				number_of_movies--;
				break;
			}
		}
	}

	fprintf(fp, "%d\n", number_of_movies);

	for (n=0;n<number_of_movies;n++)
	{
		if (verbose) fprintf(stderr,"Writing - %s\n", movie_array[n]);
		fprintf(fp, "%s\n", movie_array[n]);
	}

	for (n=0;n<number_of_files;n++)
	{
		for (x=0;x<MAX_NUM_SCENES;x++)
		{
			if (strcmp(movie_array[x], GTK_IMAGEPLUG(image[n])->video_filename) == 0)
			break;
		}
		/* The last two numbers are new - not part of official editlists!
		   they will work with normal lavplay, though :-) */
		if (verbose) fprintf(stderr,"Writing - %d %d %d (%d %d)\n", x,
			GTK_IMAGEPLUG(image[n])->startframe,
			GTK_IMAGEPLUG(image[n])->stopframe,
			GTK_IMAGEPLUG(image[n])->startscene,
			GTK_IMAGEPLUG(image[n])->stopscene);
		fprintf(fp, "%d %d %d (%d %d)\n", x,
			GTK_IMAGEPLUG(image[n])->startframe,
			GTK_IMAGEPLUG(image[n])->stopframe,
			GTK_IMAGEPLUG(image[n])->startscene,
			GTK_IMAGEPLUG(image[n])->stopscene);
	}

	if (verbose) printf("Editlist saved\n");

	sprintf(temp, "%s/.studio/%s", getenv("HOME"), editlist_filename);
	if (strcmp(target, temp)!=0)
	{
		sprintf(temp, "Editlist saved to %s", target);
		gtk_show_text_window(STUDIO_INFO, temp, NULL);
	}

	fclose(fp);
}

void save_eli_temp_file()
{
	char filename[256];

	sprintf(filename, "%s/.studio/%s", getenv("HOME"), editlist_filename);
	save_eli_file(filename);
}

void reset_lavedit_tv()
{
	gtk_widget_set_usize(lavedit_tv, tv_width_edit, tv_height_edit);
}

void split_scene(GtkWidget *widget, gpointer data)
{
	if (number_of_files >= MAX_NUM_SCENES)
	{
		display_max_num_scenes_warning();
		return;
	}
	if (number_of_files == 0)
		return;

	if (current_image >= 0 && current_image < number_of_files)
	{
		GdkPixbuf *temp;
		char filename_img_tmp[256], command[256];

		if (current_image < number_of_files-1)
		{
			int i;
			for (i=number_of_files;i>current_image+1;i--)
			{
				if (i == number_of_files)
				{
					image[i] = gtk_imageplug_new(
						GTK_IMAGEPLUG(image[i-1])->picture,
						GTK_IMAGEPLUG(image[i-1])->video_filename,
						GTK_IMAGEPLUG(image[i-1])->startscene,
						GTK_IMAGEPLUG(image[i-1])->stopscene,
						GTK_IMAGEPLUG(image[i-1])->startframe,
						GTK_IMAGEPLUG(image[i-1])->stopframe,
						GTK_IMAGEPLUG(image[i-1])->start_total,
						GTK_IMAGEPLUG(image[i-1])->background, i);

					gtk_box_pack_start(GTK_BOX(hbox_scrollwindow),
						image[i], FALSE, FALSE, 0);
					gtk_signal_connect(GTK_OBJECT(image[i]),
						"button_press_event",
						GTK_SIGNAL_FUNC(image_clicked), NULL);
					gtk_widget_show(image[i]);
				}
				else
				{
					gtk_imageplug_copy(
						GTK_IMAGEPLUG(image[i]), 
						GTK_IMAGEPLUG(image[i-1]));
				}
				
			}
		}

		/* Current scene should be split into two */
		gtk_imageplug_copy(
			GTK_IMAGEPLUG(image[current_image+1]),
			GTK_IMAGEPLUG(image[current_image])   );

		GTK_IMAGEPLUG(image[current_image])->selection = 0;
		GTK_IMAGEPLUG(image[current_image])->stopframe =
			current_position - 1 -
			GTK_IMAGEPLUG(image[current_image])->start_total +
			GTK_IMAGEPLUG(image[current_image])->startframe;
		GTK_IMAGEPLUG(image[current_image])->stopscene =
			GTK_IMAGEPLUG(image[current_image])->stopframe;
		gtk_imageplug_draw(image[current_image]);

		current_image++;

		sprintf(filename_img_tmp, "%s/.studio/.temp.jpg",
			getenv("HOME"));
		sprintf(command, "%s -o %s -f i -i %d %s%s",
			LAVTRANS_LOCATION, filename_img_tmp, current_position,
			GTK_IMAGEPLUG(image[current_image])->video_filename,
			verbose?"":" >> /dev/null 2>&1");
		system(command);
		temp = gdk_pixbuf_new_from_file (filename_img_tmp);
		GTK_IMAGEPLUG(image[current_image])->picture =
			gdk_pixbuf_scale_simple(temp,
			GTK_IMAGEPLUG(image[current_image-1])->width,
			GTK_IMAGEPLUG(image[current_image-1])->height,
			GDK_INTERP_NEAREST);
		GTK_IMAGEPLUG(image[current_image])->selection = 1;
		GTK_IMAGEPLUG(image[current_image])->start_total = current_position;
		GTK_IMAGEPLUG(image[current_image])->startframe =
			GTK_IMAGEPLUG(image[current_image-1])->stopframe+1;
		GTK_IMAGEPLUG(image[current_image])->startscene =
			GTK_IMAGEPLUG(image[current_image])->startframe;

		gtk_imageplug_draw(image[current_image]);
		number_of_files++;
	}
	else
		gtk_show_text_window(STUDIO_INFO,
			"First select an image to split", NULL);
}

void scene_move(GtkWidget *widget, gpointer data)
{
	int what = (int) data;

	if (number_of_files == 0)
		return;

	if ((what == -1 && current_image==0) ||
		(what == 1 && current_image == number_of_files-1))
	{
		char temp[256];
		sprintf(temp,"Image is already at the %s!\n",
			what == -1 ? "beginning" : "end");
		gtk_show_text_window(STUDIO_INFO,
			"Cannot move image",
			temp);
		return;
	}

	if (current_image<0 || current_image>=number_of_files)
	{
		gtk_show_text_window(STUDIO_INFO,
			"Select a scene to move first",NULL);
	}
	else
	{
		char temp[256];
		int i = -1, diff=0;
		GtkWidget *temp_imageplug;

		/* let's start making a 'temp imageplug' */
		temp_imageplug = gtk_imageplug_new(
			GTK_IMAGEPLUG(image[current_image])->picture,
			GTK_IMAGEPLUG(image[current_image])->video_filename,
			GTK_IMAGEPLUG(image[current_image])->startscene,
			GTK_IMAGEPLUG(image[current_image])->stopscene,
			GTK_IMAGEPLUG(image[current_image])->startframe,
			GTK_IMAGEPLUG(image[current_image])->stopscene,
			GTK_IMAGEPLUG(image[current_image])->start_total,
			GTK_IMAGEPLUG(image[current_image])->background, 0);

		/* Lavplay calls to move scene */
		if (what == -1)
		{
			sprintf(temp,"em %d %d %d",
				GTK_IMAGEPLUG(image[current_image])->start_total,
				GTK_IMAGEPLUG(image[current_image])->start_total + 
				GTK_IMAGEPLUG(image[current_image])->stopframe -
				GTK_IMAGEPLUG(image[current_image])->startframe,
				GTK_IMAGEPLUG(image[current_image-1])->start_total);
		}
		else
		{
			sprintf(temp,"em %d %d %d",
				GTK_IMAGEPLUG(image[current_image])->start_total,
				GTK_IMAGEPLUG(image[current_image])->start_total + 
				GTK_IMAGEPLUG(image[current_image])->stopframe -
				GTK_IMAGEPLUG(image[current_image])->startframe,
				GTK_IMAGEPLUG(image[current_image+1])->start_total -
				GTK_IMAGEPLUG(image[current_image])->stopframe + 
				GTK_IMAGEPLUG(image[current_image])->startframe +
				GTK_IMAGEPLUG(image[current_image+1])->stopframe -
				GTK_IMAGEPLUG(image[current_image+1])->startframe);
		}
		command_to_lavplay_edit(NULL, temp);

		if (what == -1)
		{
			i = current_image-1;
			diff = GTK_IMAGEPLUG(image[i])->start_total;
		}
		else if (what == 1)
		{
			i = current_image+1;
			diff = GTK_IMAGEPLUG(image[current_image])->start_total;
		}

		gtk_imageplug_copy(
			GTK_IMAGEPLUG(image[current_image]),
			GTK_IMAGEPLUG(image[i]) );
		gtk_imageplug_copy(
			GTK_IMAGEPLUG(image[i]),
			GTK_IMAGEPLUG(temp_imageplug));

		/* correct start_total - don't understand, this is pobably correct :-) */
		if (what == -1)
		{
			GTK_IMAGEPLUG(image[i])->start_total = diff;
			GTK_IMAGEPLUG(image[current_image])->start_total = diff + 1 +
				GTK_IMAGEPLUG(image[i])->stopframe -
				GTK_IMAGEPLUG(image[i])->startframe;
		}
		else if (what == 1)
		{
			GTK_IMAGEPLUG(image[current_image])->start_total = diff;
			GTK_IMAGEPLUG(image[i])->start_total = diff + 1 +
				GTK_IMAGEPLUG(image[current_image])->stopframe -
				GTK_IMAGEPLUG(image[current_image])->startframe;
		}

		/* change selection */
		GTK_IMAGEPLUG(image[current_image])->selection = 0;
		gtk_imageplug_draw(image[current_image]);
		current_image += what;
		GTK_IMAGEPLUG(image[current_image])->selection = 1;
		gtk_imageplug_draw(image[current_image]);
	}
}

void delete_scene(GtkWidget *widget, gpointer data)
{
	if (number_of_files == 0)
		return;

	if (current_image < 0 || current_image >= number_of_files)
	{
		gtk_show_text_window(STUDIO_INFO,
			"Select a scene to delete first",NULL);
	}
	else
	{
		int i, current_temp_pos, diff;
		char temp[256];

		current_temp_pos = current_position;
		if (current_position > GTK_IMAGEPLUG(image[current_image])->start_total)
		{
			if (current_position -
				GTK_IMAGEPLUG(image[current_image])->stopframe +
				GTK_IMAGEPLUG(image[current_image])->startframe <
				GTK_IMAGEPLUG(image[current_image])->start_total)
				current_temp_pos =
					GTK_IMAGEPLUG(image[current_image])->start_total;
			else
				current_temp_pos = current_position -
					GTK_IMAGEPLUG(image[current_image])->stopframe +
					GTK_IMAGEPLUG(image[current_image])->startframe;
		}

		diff = GTK_IMAGEPLUG(image[current_image])->stopframe -
			GTK_IMAGEPLUG(image[current_image])->startframe;
		sprintf(temp, "ed %d %d",
			GTK_IMAGEPLUG(image[current_image])->start_total,
			GTK_IMAGEPLUG(image[current_image])->start_total + diff);
		command_to_lavplay_edit(NULL,temp);
		diff++;

		if (current_image != -1)
		{
			for (i=current_image;i<MAX_NUM_SCENES;i++)
			{
				if (image[i+1] != NULL)
				{
					gtk_imageplug_copy(
						GTK_IMAGEPLUG(image[i]),
						GTK_IMAGEPLUG(image[i+1]) );
					GTK_IMAGEPLUG(image[i])->start_total -= diff;
				}
				else
				{
					if (number_of_files > 1)
					{
						gtk_widget_hide(image[i]);
						gtk_widget_destroy(image[i]);
						image[i] = NULL;
					}
					else
					{
						GTK_IMAGEPLUG(image[i])->selection = 0;
						gtk_imageplug_set_data(
							GTK_IMAGEPLUG(image[i]),
							NULL,"",0,0,0,0,0);
					}
					break;
				}
			}
		}

		if (current_image == number_of_files-1)
			current_image--;

		if (current_image >= 0)
		{
			GTK_IMAGEPLUG(image[current_image])->selection = 1;
			gtk_imageplug_draw(image[current_image]);
		}

		number_of_files--;

		save_eli_temp_file();
	}
}

GtkWidget *get_editing_routines_notebook_page()
{
	GtkWidget *hbox3, *vbox2, *button, *box, *pixmap_widget, *label;
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();

	vbox2 = gtk_vbox_new(FALSE, 12);
	hbox3 = gtk_hbox_new(TRUE, 10);

	button = gtk_button_new_with_label("[Start]");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(play_edit_file), NULL);
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("[Stop]");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(quit_lavplay_edit), NULL);
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 10);
	gtk_widget_show(hbox3);

	hbox3 = gtk_hbox_new(TRUE, 10);

	button = gtk_button_new(); //_with_label("New");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(file_new_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Create New Empty Editlist", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" New Editlist ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(clear_editlist), NULL);
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new(); //_with_label("Open");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(file_open_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Open Editlist from File", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Open Editlist ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(create_filesel3), "open");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new(); //_with_label("Save As");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(file_save_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Save Current Editlist to File", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Save Editlist ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(create_filesel3), "save_as");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	hbox3 = gtk_hbox_new(TRUE, 10);

	button = gtk_button_new(); //_with_label("Delete");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(scene_delete_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Delete Scene from Editlist", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Delete Scene ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(delete_scene), NULL);
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new(); //_with_label("Split");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(scene_split_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Split Scene into Two Scenes", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Split Scene ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(split_scene), (gpointer) 1);
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);
#if 0
	button = gtk_button_new_with_label("Add new scene");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(create_filesel3), "add_new");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);
#endif
	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	hbox3 = gtk_hbox_new(TRUE, 10);

	button = gtk_button_new(); //_with_label(" Move Scene to Left ");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(scene_left_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Move Scene One Position to the Left", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Move Scene to Left ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(scene_move), (gpointer) -1);
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new(); //_with_label(" Move Scene to Right ");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(scene_right_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Move Scene One Position to the Right", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Move Scene to Right ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(scene_move), (gpointer) 1);
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	hbox3 = gtk_hbox_new(TRUE, 10);

	button = gtk_button_new(); //_with_label(" Create Screenshot ");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(scene_screenshot_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Create a Screenshot of the Current Frame", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Create Screenshot ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(create_filesel3), "screen");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

/*	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);
	hbox3 = gtk_hbox_new(TRUE, 10);*/

	button = gtk_button_new(); //_with_label("Do Scene Detection");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(scene_detection_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Do Scene Detection on a New Movie", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Scene Detection ");
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(create_filesel3), "scene_detection");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	return vbox2;
}

GtkWidget *get_add_movie_notebook_page()
{
	GtkWidget *hbox3, *vbox2, *button, *label, *pixmap_widget, *box; //, *table;
/*	GdkPixbuf *bg;*/
	int i;
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();

	vbox2 = gtk_vbox_new(FALSE, 2);
	hbox3 = gtk_hbox_new(FALSE, 10);

	/* first a possibility to select which movie to add a scene from */
/*	label = gtk_label_new("Movie: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox3), label, TRUE, TRUE, 0);
	gtk_widget_show(label);
*/
	current_open_movies_combo = gtk_combo_new();
	/*gtk_combo_set_popdown_strings (GTK_COMBO (current_open_movies_combo),
		current_open_movies);*/
	for (i=0;i<5;i++)
		current_open_movies[i][0] = '\0';
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(current_open_movies_combo)->entry),
		"changed", GTK_SIGNAL_FUNC (set_add_scene_movie_selection), NULL);
	gtk_box_pack_start (GTK_BOX (hbox3), current_open_movies_combo,
		TRUE, TRUE, 0);
	gtk_widget_show (current_open_movies_combo);

	button = gtk_button_new(); //_with_label("Open");
	pixmap_widget = gtk_widget_from_xpm_data(file_widget_open_xpm);
	gtk_widget_show(pixmap_widget);
	gtk_tooltips_set_tip(tooltip, button, "Open movie or editlist", NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmap_widget);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(create_filesel3), "add_movie_selection");
	gtk_box_pack_start (GTK_BOX (hbox3), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	/* Now a table consisting of the scenes of a movie */
	//table = gtk_table_new (1, NUM_ADD_MOVIE_SCENES, TRUE);

	/*bg = gdk_pixbuf_new_from_xpm_data ((const char **)film_xpm);*/
	for (i=0;i<NUM_ADD_MOVIE_SCENES;i++)
	{
		if (i%2 == 0)
		{
			hbox3 = gtk_hbox_new(TRUE, 10);
		}

		add_movie_images[i] = gtk_imageplug_new_from_video("",0,0,0,0,0,NULL,i);
		gtk_signal_connect(GTK_OBJECT(add_movie_images[i]), "button_press_event",
			GTK_SIGNAL_FUNC(add_movie_scene_image_clicked), NULL);
		//gtk_table_attach_defaults (GTK_TABLE (table), add_movie_images[i],
			//i, i+1, 0, 1);
		gtk_box_pack_start (GTK_BOX (hbox3), add_movie_images[i], FALSE, FALSE, 0);
		gtk_widget_show(add_movie_images[i]);

		if (i%2 == 1)
		{
			gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
			gtk_widget_show(hbox3);
		}
	}

	/* and at last a button "add scene" and two buttons to see more scenes */
	hbox3 = gtk_hbox_new(FALSE, 10);

	button = gtk_button_new(); //_with_label("<=");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(arrow_left_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Go One Page Back", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(add_scene_movie_change_page), "-1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new(); //_with_label("Add selected scene");
	box = gtk_hbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(scene_add_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Add Scene to Editlist", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	label = gtk_label_new(" Add Scene ");
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(add_scene_to_editlist), NULL);
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	button = gtk_button_new(); //_with_label("=>");
	box = gtk_vbox_new(FALSE, 0);
	pixmap_widget = gtk_widget_from_xpm_data(arrow_right_xpm);
	gtk_tooltips_set_tip(tooltip, button, "Go One Page Further", NULL);
	gtk_box_pack_start (GTK_BOX (box), pixmap_widget, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_widget);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(add_scene_movie_change_page), "+1");
	gtk_box_pack_start (GTK_BOX (hbox3), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);
	gtk_widget_show(hbox3);

	return vbox2;
}

GtkWidget *create_lavedit_layout(GtkWidget *window)
{
	GtkWidget *hbox, *vbox, *vbox2, *hbox2, *scrolled_window;
	GtkWidget *edit_notebook, *hbox3, *label;
	char filename[100];

	current_image = -1;
	sprintf(editlist_savefile, "project.eli");

	vbox = gtk_vbox_new(FALSE,0);
	hbox = gtk_hbox_new(FALSE,20);
	hbox2 = gtk_hbox_new(FALSE,20);

	vbox2 = create_tv_stuff(window);

	/* a timer to see where we are in the movie (requested feature) */
	hbox3 = gtk_hbox_new ( TRUE, 10);

	label = gtk_label_new("Time: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	//gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	gtk_box_pack_start (GTK_BOX (hbox3), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	label_lavedit_time = gtk_label_new("0.00.00:00");
	gtk_misc_set_alignment(GTK_MISC(label_lavedit_time),
		1.0, GTK_MISC(label)->yalign);
	//gtk_table_attach_defaults (GTK_TABLE (table), label_lavedit_time, 1, 2, 0, 1);
	gtk_box_pack_start (GTK_BOX (hbox3), label_lavedit_time, TRUE, TRUE, 0);
	gtk_widget_show (label_lavedit_time);

	label = gtk_label_new("Frames: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	//gtk_table_attach_defaults (GTK_TABLE (table), label, 2, 3, 0, 1);
	gtk_box_pack_start (GTK_BOX (hbox3), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	label_lavedit_frames = gtk_label_new("0/0");
	gtk_misc_set_alignment(GTK_MISC(label_lavedit_frames),
		1.0, GTK_MISC(label)->yalign);
	//gtk_table_attach_defaults (GTK_TABLE (table), label_lavedit_frames, 3, 4, 0, 1);
	gtk_box_pack_start (GTK_BOX (hbox3), label_lavedit_frames, TRUE, TRUE, 0);
	gtk_widget_show (label_lavedit_frames);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 10);
	gtk_widget_show(hbox3);

	gtk_box_pack_start(GTK_BOX (hbox2), vbox2, FALSE, FALSE, 20);
	gtk_widget_show(vbox2);

	vbox2 = gtk_vbox_new(FALSE,0);

	/* the notebook containing the different editing tabs */
	edit_notebook = gtk_notebook_new();

	hbox3 = get_editing_routines_notebook_page();
	label = gtk_label_new("Editing");
	gtk_notebook_append_page (GTK_NOTEBOOK (edit_notebook), hbox3, label);
	gtk_widget_show(label);
	gtk_widget_show(hbox3);

	hbox3 = get_add_movie_notebook_page();
	label = gtk_label_new("Add Scenes");
	gtk_notebook_append_page (GTK_NOTEBOOK (edit_notebook), hbox3, label);
	gtk_widget_show(label);
	gtk_widget_show(hbox3);

	hbox3 = get_effects_notebook_page();
	label = gtk_label_new("Effects");
	gtk_notebook_append_page (GTK_NOTEBOOK (edit_notebook), hbox3, label);
	gtk_widget_show(label);
	gtk_widget_show(hbox3);

	gtk_box_pack_start(GTK_BOX(vbox2), edit_notebook, FALSE, FALSE, 5);
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (edit_notebook), GTK_POS_TOP);
	gtk_widget_show(edit_notebook);

	gtk_box_pack_start (GTK_BOX (hbox2), vbox2, FALSE, FALSE, 10);
	gtk_widget_show(vbox2);

	gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 10);
	gtk_widget_show(hbox2);

	hbox_scrollwindow = gtk_hbox_new(FALSE,0);

	sprintf(filename, "%s/.studio/%s", getenv("HOME"), editlist_filename);
	if (!open_eli_file(filename))
	{
		printf("Uh oh, something went wrong when opening the edit list file...\n");
	}

	/* the scroll-window containing the scene-image-widgets */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				GTK_POLICY_ALWAYS, GTK_POLICY_NEVER);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window),
		hbox_scrollwindow);
	gtk_widget_show(hbox_scrollwindow);

	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, FALSE, FALSE, 0);
	gtk_widget_show(scrolled_window);

	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	return hbox;
}
