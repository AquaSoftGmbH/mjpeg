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
#include <errno.h>
#include <gdk/gdkx.h>

#include "studio.h"
#include "pipes.h"
#include "lavedit.h"
#include "gtkfunctions.h"
#include "gtkimageplug.h"

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

int current_position, total_frames_edit;
GtkObject *lavedit_slider_adj;
GtkWidget *lavedit_tv, *hbox_scrollwindow;
GtkWidget *label_lavedit_time, *label_lavedit_frames;
char editlist_savefile[256];
int wait_for_add_frame_confirmation = 0;

/* Forward declarations */
void lavplay_edit_stopped(void);
char *check_editlist(char *editlist);
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

GtkWidget *create_tv_stuff(GtkWidget *window)
{
	GtkWidget *vbox2, *hbox3, *pixmap_widget, *scrollbar, *button;
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();

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
	GtkScene *scene;

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

	for (i=0;i<g_list_length(GTK_SCENELIST(scenelist)->scene);i++)
	{
		scene = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist), i);
		if (cur_pos >= scene->start_total &&
			cur_pos < scene->start_total + scene->view_end - scene->view_start)
		{
			if (GTK_SCENELIST(scenelist)->selected_scene != i)
			{
				int drawn;
				GTK_SCENELIST(scenelist)->selected_scene = i;

				/* hack */
				gtk_scenelist_get_num_drawn(GTK_SCENELIST(scenelist),
					&drawn, NULL);
				if (GTK_SCENELIST(scenelist)->selected_scene - drawn + 1 >
				    GTK_SCENELIST(scenelist)->current_scene && scene >= 0)
					gtk_scenelist_view(GTK_SCENELIST(scenelist),
						GTK_SCENELIST(scenelist)->selected_scene-drawn+1);
				if (GTK_SCENELIST(scenelist)->selected_scene <
				    GTK_SCENELIST(scenelist)->current_scene && scene >= 0)
					gtk_scenelist_view(GTK_SCENELIST(scenelist),
						GTK_SCENELIST(scenelist)->selected_scene);

				gtk_scenelist_draw(scenelist);
			}
				//gtk_scenelist_select(GTK_SCENELIST(scenelist), i);
			break;
		}
	}
}

void quit_lavplay_edit_with_error(char *msg)
{
	quit_lavplay_edit();
	if (verbose) g_print("Lavplay error: %s\n", msg);
	gtk_show_text_window(STUDIO_ERROR, "Lavplay returned an error: %s", msg);
}

void process_lavplay_edit_input(char *msg)
{
	char norm;
	int cur_pos, cur_speed;

	if(msg[0]=='@')
	{
		int n, tot=0;
		for (n=0;msg[n];n++)
			if (msg[n]=='/')
				tot++;
		if (tot==2)
			sscanf(msg+1,"%c%d/%d/%d",&norm,&cur_pos,&total_frames_edit,&cur_speed);
		else
		{
			int norm_num;
			sscanf(msg+1,"%d/%d/%d/%d",&norm_num,&cur_pos,&total_frames_edit,&cur_speed);
			norm = (norm_num==25)?'p':'n';
		}
		set_lavplay_edit_log(norm, cur_pos, cur_speed);
		return;
	}

	else if (strncmp(msg, "**ERROR:", 8) == 0)
	{
		/* Error handling */
		quit_lavplay_edit_with_error(msg+9);
	}
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

	gtk_show_text_window(STUDIO_INFO, "Image saved to \'%s\'", file);
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
		/* oops something went wrong */
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry), "");
		gtk_show_text_window(STUDIO_WARNING, "Error opening \'%s\': %s",
			gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry)),
			sys_errlist[errno]);
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
			GTK_SCENELIST(scenelist)->norm = 'n';
		else
			GTK_SCENELIST(scenelist)->norm = 'p';
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
		/* oops something went wrong */
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry), "");
		gtk_show_text_window(STUDIO_WARNING, "Error opening \'%s\': %s",
			gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(current_open_movies_combo)->entry)),
			sys_errlist[errno]);
	}
}

void add_scene_to_editlist(GtkWidget *widget, gpointer data)
{
	/* a scene has been selected to be added to the editlist, do it! */
	char temp[256];
	GtkImagePlug *im = GTK_IMAGEPLUG(add_movie_images[add_movie_scene_selected_scene]);

	/* if it's data-less */
	if (im->picture == NULL)
		return;

	/* tell lavplay */
	sprintf(temp, "ea %s %d %d %d",
		im->video_filename,
		im->startframe,
		im->stopframe,
		GTK_SCENELIST(scenelist)->selected_scene<0 ? 0 :
			gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
				GTK_SCENELIST(scenelist)->selected_scene)->start_total); 
	command_to_lavplay_edit(NULL,temp);

	/* tell the scenelist */
	gtk_scenelist_edit_add(GTK_SCENELIST(scenelist),
		im->video_filename, im->startframe, im->stopframe,
		im->startscene, im->stopscene,
		GTK_SCENELIST(scenelist)->selected_scene<0 ? 0 :
			GTK_SCENELIST(scenelist)->selected_scene);
	gtk_scenelist_select(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene<0 ? 0 :
			GTK_SCENELIST(scenelist)->selected_scene);

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
	if (!pipe_is_active(LAVPLAY_E) &&
		g_list_length(GTK_SCENELIST(scenelist)->scene) > 0)
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

void clear_editlist(GtkWidget *widget, gpointer data)
{
	gtk_scenelist_new_editlist(GTK_SCENELIST(scenelist));
	save_eli_temp_file();
}

int open_eli_file(char *filename)
{
	gtk_scenelist_open_editlist(GTK_SCENELIST(scenelist), filename);
	gtk_scenelist_draw(scenelist);
	save_eli_temp_file();
	return 1;
}

void save_eli_file(char *target)
{
	if (!gtk_scenelist_write_editlist(GTK_SCENELIST(scenelist), target))
		gtk_show_text_window(STUDIO_INFO,
			"Error saving editlist to file \'%s\': %s",
			target, (char *) sys_errlist[errno]);
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
	if (GTK_SCENELIST(scenelist)->selected_scene >= 0)
	{
		gtk_scenelist_edit_split(GTK_SCENELIST(scenelist),
			GTK_SCENELIST(scenelist)->selected_scene,
			current_position - gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
			GTK_SCENELIST(scenelist)->selected_scene)->start_total);
		save_eli_temp_file();
	}
	else
		gtk_show_text_window(STUDIO_INFO,
			"First select an image to split");
}

void scene_move(GtkWidget *widget, gpointer data)
{
	GtkScene *scene1, *scene2;
	int what = (int) data;
	char temp[256];

	if ((what == -1 && GTK_SCENELIST(scenelist)->selected_scene==0) ||
		(what == 1 && GTK_SCENELIST(scenelist)->selected_scene ==
		g_list_length(GTK_SCENELIST(scenelist)->scene)-1))
	{
		gtk_show_text_window(STUDIO_INFO,
			"Cannot move image to the %s, image is already there!",
			what==-1 ? "beginning" : "end");
		return;
	}

	if (GTK_SCENELIST(scenelist)->selected_scene<0)
	{
		gtk_show_text_window(STUDIO_INFO,
			"Select a scene to move first");
		return;
	}
	/* Lavplay calls to move scene */
	if (what == -1)
	{
		scene1 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
			GTK_SCENELIST(scenelist)->selected_scene);
		scene2 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
			GTK_SCENELIST(scenelist)->selected_scene-1);
		sprintf(temp,"em %d %d %d",
			scene1->start_total,
			scene1->start_total + scene1->view_end - scene1->view_start,
			scene2->start_total);
	}
	else
	{
		scene1 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
			GTK_SCENELIST(scenelist)->selected_scene);
		scene2 = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
			GTK_SCENELIST(scenelist)->selected_scene+1);
		sprintf(temp,"em %d %d %d",
			scene1->start_total,
			scene1->start_total + scene1->view_end - scene1->view_start,
			scene1->start_total - scene1->view_end + scene1->view_start +
			scene2->view_end - scene2->view_start);
	}
	command_to_lavplay_edit(NULL, temp);

	/* tell scenelist */
	gtk_scenelist_edit_move(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene,
		what);

	save_eli_temp_file();
}

void delete_scene(GtkWidget *widget, gpointer data)
{
	GtkScene *scene;
	char temp[256];

	if (GTK_SCENELIST(scenelist)->selected_scene < 0)
	{
		gtk_show_text_window(STUDIO_INFO,
			"Select a scene to delete first");
		return;
	}

	/* tell lavplay */
	scene = gtk_scenelist_get_scene(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene);
	sprintf(temp, "ed %d %d",
		scene->start_total,
		scene->start_total + scene->view_end - scene->view_start);
	command_to_lavplay_edit(NULL,temp);

	/* tell scenelist */
	gtk_scenelist_edit_delete(GTK_SCENELIST(scenelist),
		GTK_SCENELIST(scenelist)->selected_scene);

	save_eli_temp_file();
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

static void scene_clicked_signal(GtkWidget *widget, gint scene_num, gpointer data)
{
	GtkScene *scene;
	char buff[256];
	scene = gtk_scenelist_get_scene(GTK_SCENELIST(widget), GTK_SCENELIST(widget)->selected_scene);
	if (verbose) g_print("Scene %d (frame %d-%d in %s, frame %d in editlist) was clicked\n",
		GTK_SCENELIST(widget)->selected_scene, scene->view_start, scene->view_end,
		(char *)g_list_nth_data(GTK_SCENELIST(widget)->movie, scene->movie_num),
		scene->start_total);

	sprintf(buff, "s%d", scene->start_total);
	command_to_lavplay_edit(NULL, buff);
}

static void scrollbar_expose_event(GtkWidget *widget, gpointer data)
{
	GtkWidget *scrollbar = GTK_WIDGET(data);
	GtkObject *adj;
	gint num;
printf("Signal\n");
	gtk_scenelist_get_num_drawn(GTK_SCENELIST(scenelist),&num,NULL);
	adj = gtk_adjustment_new(GTK_SCENELIST(scenelist)->current_scene,
		0, g_list_length(GTK_SCENELIST(scenelist)->scene), 1, 3, num);
	gtk_range_set_adjustment(GTK_RANGE(scrollbar), GTK_ADJUSTMENT(adj));
	gtk_scenelist_set_adjustment(GTK_SCENELIST(scenelist), GTK_ADJUSTMENT(adj));
}

GtkWidget *create_lavedit_layout(GtkWidget *window)
{
	GtkWidget *hbox, *vbox, *vbox2, *hbox2, *scrollbar;
	GtkObject *adj;
	gint num;
	GtkWidget *edit_notebook, *hbox3, *label;
	char filename[256];

	sprintf(editlist_savefile, "project.eli");
	sprintf(filename, "%s/.studio/%s", getenv("HOME"), editlist_filename);

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

	vbox2 = gtk_vbox_new(FALSE, 0);

	scenelist = gtk_scenelist_new(filename);
	gtk_signal_connect(GTK_OBJECT(scenelist), "scene_selected",
		GTK_SIGNAL_FUNC(scene_clicked_signal), NULL);
	gtk_box_pack_start(GTK_BOX (vbox2), scenelist, TRUE, TRUE, 0);
	gtk_widget_show(scenelist);

	gtk_scenelist_get_num_drawn(GTK_SCENELIST(scenelist),&num,NULL);
	adj = gtk_adjustment_new(GTK_SCENELIST(scenelist)->current_scene,
		0, g_list_length(GTK_SCENELIST(scenelist)->scene),
		1, 3, num);
	scrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(adj));
	gtk_signal_connect(GTK_OBJECT(scenelist), "scenelist_changed",
	      GTK_SIGNAL_FUNC(scrollbar_expose_event), (gpointer)scrollbar);
	gtk_signal_connect(GTK_OBJECT(scenelist), "realize",
	      GTK_SIGNAL_FUNC(scrollbar_expose_event), (gpointer)scrollbar);
	gtk_range_set_adjustment(GTK_RANGE(scrollbar), GTK_ADJUSTMENT(adj));
	gtk_scenelist_set_adjustment(GTK_SCENELIST(scenelist), GTK_ADJUSTMENT(adj));
	gtk_box_pack_start(GTK_BOX (vbox2), scrollbar, TRUE, TRUE, 0);
	gtk_widget_show(scrollbar);

	gtk_box_pack_start (GTK_BOX (vbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	return hbox;
}
