/* Scene List widget for Linux Video Studio
 * Copyright (C) 2001 - Ronald Bultje
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, 
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkselection.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

#include "gtkscenelist.h"
#include "pipes.h"
#include "film.xpm"

#define SCENE_IMAGE_WIDTH 100
#define SCENE_IMAGE_HEIGHT 75

/* for the "scene_selected" signal */
enum {
	SCENE_SELECTED,
	SCENELIST_CHANGED,
	LAST_SIGNAL
};

extern int verbose;


/* SceneList functions */
static void gtk_scenelist_class_init (GtkSceneListClass *klass);
static void gtk_scenelist_init (GtkSceneList *scenelist);
static void gtk_scenelist_destroy (GtkObject *object);
static void gtk_scenelist_realize (GtkWidget *widget);
static void gtk_scenelist_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_scenelist_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_scenelist_expose (GtkWidget *widget, GdkEventExpose *event);
static gint gtk_scenelist_button_press (GtkWidget *widget, GdkEventButton *event);
static void gtk_scenelist_make_empty(GtkSceneList *scenelist);

/* Scene functions */
static GtkScene *gtk_scene_new (gint view_start,
				gint view_end,
				gint scene_start,
				gint scene_end,
				gint start_total,
				gint movie_num,
				char *movie);
static void gtk_scene_destroy (GtkScene *scene);

/* TODO: implement effects/transitions objects */
//static GtkTransition *gtk_transition_new();
//static GtkEffect *gtk_effect_new();

static guint gtk_scenelist_signals[LAST_SIGNAL] = { 0 };
static GtkWidgetClass *parent_class = NULL;

guint gtk_scenelist_get_type ()
{
	static guint scenelist_type = 0;

	if (!scenelist_type)
	{
		GtkTypeInfo scenelist_info =
		{
			"GtkSceneList",
			sizeof (GtkSceneList),
			sizeof (GtkSceneListClass),
			(GtkClassInitFunc) gtk_scenelist_class_init,
			(GtkObjectInitFunc) gtk_scenelist_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		scenelist_type = gtk_type_unique (gtk_widget_get_type (), &scenelist_info);
	}
	return scenelist_type;
}


static void gtk_scenelist_class_init (GtkSceneListClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;

	parent_class = gtk_type_class (gtk_widget_get_type ());

	object_class->destroy = gtk_scenelist_destroy;

	widget_class->realize = gtk_scenelist_realize;
	widget_class->expose_event = gtk_scenelist_expose;
	widget_class->size_request = gtk_scenelist_size_request;
	widget_class->size_allocate = gtk_scenelist_size_allocate;
	widget_class->button_press_event = gtk_scenelist_button_press;

	/* for the "scene_selected" signal */
	gtk_scenelist_signals[SCENE_SELECTED] = gtk_signal_new("scene_selected",
		GTK_RUN_FIRST, object_class->type,
		GTK_SIGNAL_OFFSET(GtkSceneListClass, scene_selected),
		gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 1,
		GTK_TYPE_INT);
	gtk_scenelist_signals[SCENELIST_CHANGED] = gtk_signal_new("scenelist_changed",
		GTK_RUN_FIRST, object_class->type,
		GTK_SIGNAL_OFFSET(GtkSceneListClass, scene_selected),
		gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals(object_class, gtk_scenelist_signals, LAST_SIGNAL);
}


static void gtk_scenelist_init (GtkSceneList *scenelist)
{
	scenelist->items = NULL;
	scenelist->background = gdk_pixbuf_new_from_xpm_data ((const char **)film_xpm);
	scenelist->num_frames = 0;
	scenelist->scene = NULL;
	scenelist->transition = NULL;
	scenelist->movie = NULL;
	scenelist->norm = '?';
	scenelist->current_scene = 0;
	scenelist->adj = NULL;
	scenelist->selected_scene = -1;
}


static GtkScene *gtk_scene_new(gint view_start, gint view_end,
			gint scene_start, gint scene_end,
			gint start_total, gint movie_num, char *movie)
{
	GtkScene *scene;
	char command[512], filename[256];
	GdkPixbuf *temp;

	scene = (GtkScene *) malloc(sizeof(GtkScene));

	scene->view_start = view_start;
	scene->view_end = view_end;
	scene->scene_start = scene_start;
	scene->scene_end = scene_end;
	scene->start_total = start_total;
	scene->movie_num = movie_num;

	sprintf(filename, "%s/.studio/temp.jpg", getenv("HOME"));
	sprintf(command, "%s -o %s -f i -i %d %s%s",
		app_location(LAVTRANS), filename, view_start, movie,
		verbose?"":" >> /dev/null 2>&1");
	system(command);
	temp = gdk_pixbuf_new_from_file (filename);
	unlink(filename);
	scene->image = gdk_pixbuf_scale_simple(temp, SCENE_IMAGE_WIDTH,
		SCENE_IMAGE_HEIGHT, GDK_INTERP_NEAREST);
	gdk_pixbuf_unref(temp);

	scene->effect = NULL;

	return scene;
}


static void gtk_scene_destroy(GtkScene *scene)
{
	if (scene->image) gdk_pixbuf_unref(scene->image);
	/* TODO: add effects stuff */
	//if (scene->effect) gtk_effect_destroy(scene->effect);
	free(scene);
}


GtkWidget *gtk_scenelist_new(gchar *editlist)
{
	GtkSceneList *scenelist;

	scenelist = gtk_type_new (gtk_scenelist_get_type ());
	if (editlist) gtk_scenelist_open_editlist(scenelist, editlist);
	return GTK_WIDGET(scenelist);
}


static void gtk_scenelist_make_empty(GtkSceneList *scenelist)
{
	if (scenelist->items) g_list_free(scenelist->items);
	scenelist->items = NULL;
	scenelist->num_frames = 0;
	if (scenelist->scene) g_list_free(scenelist->scene);
	scenelist->scene = NULL;
	if (scenelist->transition) g_list_free(scenelist->transition);
	scenelist->transition = NULL;
	if (scenelist->movie) g_list_free(scenelist->movie);
	scenelist->movie = NULL;
}


gint gtk_scenelist_open_editlist(GtkSceneList *scenelist, gchar *editlist)
{
	FILE *fd;
	char buff[256];
	int i,a,b,c,d,e;
	int total = 0, num_item = 0, num_movies;
	char *movie;

	gtk_scenelist_make_empty(scenelist);

	/* open the file */
	if (!(fd = fopen(editlist, "r"))) goto ERROR_READING;

	/* the header */
	if (!fgets(buff,255,fd)) goto ERROR_READING;
	if (strcmp(buff, "LAV Edit List\n")) goto ERROR_READING;

	/* the norm */
	if (!fgets(buff,255,fd)) goto ERROR_READING;
	if (!strcmp(buff, "PAL\n")) scenelist->norm = 'p';
	else if (!strcmp(buff, "NTSC\n")) scenelist->norm = 'n';
	else goto ERROR_READING;

	/* number of input movies */
	if (!fgets(buff,255,fd)) goto ERROR_READING;
	if (sscanf(buff, "%d\n", &num_movies) != 1) goto ERROR_READING;

	/* get the input movies */
	for (i=0;i<num_movies;i++)
	{
		movie = (char *) malloc(sizeof(char) * 256);
		if (!movie) goto ERROR_READING;
		if (!fgets(movie,255,fd)) goto ERROR_READING;
		if (movie[strlen(movie)-1] != '\n') goto ERROR_READING;
		movie[strlen(movie)-1] = '\0';
		scenelist->movie = g_list_append(scenelist->movie, movie);
	}

	/* get the input scenes */
	while(fgets(buff, 255, fd))
	{
		if (buff[0] == ':')
		{
			/* todo :-) */
		}
		else
		{
			i = sscanf(buff, "%d %d %d %d %d\n", &a, &b, &c, &d, &e);
			if (i != 3 && i != 5) goto ERROR_READING;
			if (i==3) { e = c; d = b; }
			scenelist->items = g_list_append(scenelist->items,
				(gpointer) gtk_scene_new(b,c,d,e,total,a,
				(char *) g_list_nth_data(scenelist->movie,a)));
			total += (1+c-b);
			scenelist->scene = g_list_append(scenelist->scene,
				(gpointer) num_item);
			num_item++;
		}
	}
	scenelist->num_frames = total;

	fclose(fd);

	return 1;

ERROR_READING:
	if (fd) fclose(fd);
	gtk_scenelist_make_empty(scenelist);
	return 0;
}


static void gtk_scenelist_destroy (GtkObject *object)
{
	GtkSceneList *scenelist;
	int i;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SCENELIST (object));

	scenelist = GTK_SCENELIST (object);

	/* delete the scenes */
	for (i=0;i<g_list_length(scenelist->scene);i++)
		gtk_scene_destroy(gtk_scenelist_get_scene(scenelist, i));
	g_list_free(scenelist->scene);

	/* TODO: delete the transitions */
	//for (i=0;i<g_list_length(scenelist->transition);i++)
	//	gtk_transition_destroy((GtkTransition *) g_list_nth_data(scenelist->items,
	//		g_list_nth_data(scenelist->transition, i)));
	//g_list_free(scenelist->transition);

	/* delete the movies */
	for (i=0;i<g_list_length(scenelist->movie);i++)
		free(g_list_nth_data(scenelist->movie, i));
	g_list_free(scenelist->movie);

	/* delete the glist */
	g_list_free(scenelist->items);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void gtk_scenelist_realize (GtkWidget *widget)
{
	GtkSceneList *scenelist;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SCENELIST (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
	scenelist = GTK_SCENELIST (widget);

	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.event_mask = gtk_widget_get_events (widget) | 
		GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
		GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
		GDK_POINTER_MOTION_HINT_MASK;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

	widget->style = gtk_style_attach (widget->style, widget->window);

	gdk_window_set_user_data (widget->window, widget);

	gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);
}


static void gtk_scenelist_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GtkSceneList *scenelist;

	scenelist = GTK_SCENELIST (widget);

	requisition->width = 3*gdk_pixbuf_get_width(scenelist->background);
	requisition->height = gdk_pixbuf_get_height(scenelist->background);
}


static void gtk_scenelist_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkSceneList *scenelist;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SCENELIST (widget));
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;
	scenelist = GTK_SCENELIST (widget);

	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
			allocation->x, allocation->y,
			allocation->width, allocation->height);
	}
}


static gint gtk_scenelist_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GtkSceneList *scenelist;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SCENELIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->count > 0)
		return FALSE;
 
	scenelist = GTK_SCENELIST (widget);

	gtk_scenelist_draw(widget);

	return FALSE;
}


static gint gtk_scenelist_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GtkSceneList *scenelist;
	gint num, xleft, i, selected_scene = -1;

	scenelist = GTK_SCENELIST(widget);
	gtk_scenelist_get_num_drawn(scenelist, &num, &xleft);

	/* first find out which (if any) image was clicked ('selected_scene') */
	for (i=-1;i<=num;i++)
	{
		if (event->x >= xleft+i*gdk_pixbuf_get_width(scenelist->background) &&
			event->x < xleft+(i+1)*gdk_pixbuf_get_width(scenelist->background))
		{
			selected_scene = i+scenelist->current_scene;
			if (selected_scene >= g_list_length(scenelist->scene))
				selected_scene = -1;
			if (selected_scene < 0)
				selected_scene = -1;
			break;
		}
	}

	/* now do something useful */
	if (event->button == 1)
	{
		gtk_scenelist_select(scenelist, selected_scene);
	}
#if 0
	else if (event->button == 2)
	{
		//do something :-)
	}
	else if (event->button == 3)
	{
		//display menu?
	}
#endif

	return TRUE;
}


void gtk_scenelist_select(GtkSceneList *scenelist, gint scene)
{
	gint drawn;

	if (scene < 0 || scene >= g_list_length(scenelist->scene))
		scene = -1;
	scenelist->selected_scene = scene;
	gtk_scenelist_get_num_drawn(scenelist, &drawn, NULL);
	if (scenelist->selected_scene - drawn + 1 > scenelist->current_scene && scene >= 0)
		gtk_scenelist_view(GTK_SCENELIST(scenelist), scenelist->selected_scene - drawn + 1);
	if (scenelist->selected_scene < scenelist->current_scene && scene >= 0)
		gtk_scenelist_view(GTK_SCENELIST(scenelist), scenelist->selected_scene);
	gtk_scenelist_draw(GTK_WIDGET(scenelist));
	if (scene >= 0) gtk_signal_emit_by_name(GTK_OBJECT(scenelist), "scene_selected", scene);
}


void gtk_scenelist_draw(GtkWidget *widget)
{
	gint i, num, xleft, x_off, y_off;
	GtkSceneList *scenelist;
	GtkScene *scene;

	g_return_if_fail(GTK_IS_SCENELIST(widget));
	scenelist = GTK_SCENELIST (widget);

	gdk_window_clear_area (widget->window, 0, 0,
		widget->allocation.width, widget->allocation.height);

	gtk_scenelist_get_num_drawn(scenelist, &num, &xleft);

	x_off = (gdk_pixbuf_get_width(scenelist->background) - SCENE_IMAGE_WIDTH)/2;
	y_off = (gdk_pixbuf_get_height(scenelist->background) - SCENE_IMAGE_HEIGHT)/2;

	/* TODO: transition, effects information, etc. */
	for (i=-1;i<=num;i++)
	{
		gdk_pixbuf_render_to_drawable (scenelist->background,
			widget->window, widget->style->white_gc, 0, 0,
			xleft+i*gdk_pixbuf_get_width(scenelist->background), 0,
			gdk_pixbuf_get_width(scenelist->background),
			gdk_pixbuf_get_height(scenelist->background),
			GDK_RGB_DITHER_NORMAL, 0, 0);

		if (scenelist->selected_scene == scenelist->current_scene+i && scenelist->selected_scene >= 0)
		{
			gdk_draw_rectangle(widget->window, widget->style->white_gc, TRUE,
				xleft+x_off-2+i*gdk_pixbuf_get_width(scenelist->background),
				y_off-2, SCENE_IMAGE_WIDTH+4, SCENE_IMAGE_HEIGHT+4);
		}
		else
		{
			gdk_draw_rectangle(widget->window, widget->style->black_gc, TRUE,
				xleft+x_off-1+i*gdk_pixbuf_get_width(scenelist->background),
				y_off-1, SCENE_IMAGE_WIDTH+2, SCENE_IMAGE_HEIGHT+2);
		}

		if (i+scenelist->current_scene >= 0 && i+scenelist->current_scene < g_list_length(scenelist->scene))
		{
#if 0
			scene = (GtkScene *) g_list_nth_data(scenelist->items,
				(gint) g_list_nth_data(scenelist->scene, i+scenelist->current_scene));
#endif
			scene = gtk_scenelist_get_scene(scenelist,
				i+scenelist->current_scene);
			if (!scene)
			{
				printf("Error: scene %d == NULL\n",
					i+scenelist->current_scene);
				continue;
			}
			gdk_pixbuf_render_to_drawable (scene->image, widget->window,
				widget->style->white_gc, 0, 0,
				xleft+x_off+i*gdk_pixbuf_get_width(scenelist->background),
				y_off, SCENE_IMAGE_WIDTH, SCENE_IMAGE_HEIGHT,
				GDK_RGB_DITHER_NORMAL, 0, 0);
		}
	}
}


static void gtk_scenelist_adjustment_value_changed(GtkAdjustment *adj, gpointer data)
{
	GtkSceneList *scenelist = GTK_SCENELIST(data);
	gtk_scenelist_view(GTK_SCENELIST(scenelist), adj->value);
}


void gtk_scenelist_set_adjustment(GtkSceneList *scenelist, GtkAdjustment *adj)
{
	scenelist->adj = adj;
	gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
	      GTK_SIGNAL_FUNC(gtk_scenelist_adjustment_value_changed), (gpointer) scenelist);
}


void gtk_scenelist_view(GtkSceneList *scenelist, gint num)
{
	gint num_seen;

	if (num == scenelist->current_scene) return;

	gtk_scenelist_get_num_drawn(scenelist, &num_seen, NULL);
	scenelist->current_scene = num;

	if (scenelist->current_scene > g_list_length(scenelist->scene) - num_seen)
		scenelist->current_scene = g_list_length(scenelist->scene) - num_seen;
	if (scenelist->current_scene < 0)
		scenelist->current_scene = 0;

	if (scenelist->adj) gtk_adjustment_set_value(scenelist->adj, scenelist->current_scene);

	gtk_scenelist_draw(GTK_WIDGET(scenelist));
}


gint gtk_scenelist_write_editlist(GtkSceneList *scenelist, gchar *filename)
{
	FILE *fd;
	gint i;
	GtkScene *scene;

	fd = fopen(filename, "w");
	if (!fd) return 0;

	fprintf(fd, "LAV Edit List\n");
	fprintf(fd, "%s\n", scenelist->norm=='p'?"PAL":"NTSC");
	fprintf(fd, "%d\n", g_list_length(scenelist->movie));

	for (i=0;i<g_list_length(scenelist->movie);i++)
		fprintf(fd, "%s\n", (char *) g_list_nth_data(scenelist->movie, i));

	/* TODO: transition info, blablabla, etc. etc. etc. */
	for (i=0;i<g_list_length(scenelist->scene);i++)
	{
#if 0
		scene = (GtkScene *) g_list_nth_data(scenelist->items,
			(gint)g_list_nth_data(scenelist->scene, i));
#endif
		scene = gtk_scenelist_get_scene(scenelist, i);
		if (!scene)
		{
			printf("Error: scene %d == NULL\n", i);
			return 0;
		}
		fprintf(fd, "%d %d %d %d %d\n", scene->movie_num,
			scene->view_start, scene->view_end,
			scene->scene_start, scene->scene_end);
	}

	fclose(fd);
	return 1;
}


void gtk_scenelist_new_editlist(GtkSceneList *scenelist)
{
	gtk_scenelist_make_empty(scenelist);
	gtk_scenelist_draw(GTK_WIDGET(scenelist));
}


GtkScene *gtk_scenelist_get_scene(GtkSceneList *scenelist, gint scene)
{
	if (scene < 0 || scene >= g_list_length(scenelist->scene)) return NULL;
	return (GtkScene *) g_list_nth_data(scenelist->items,
		(gint) g_list_nth_data(scenelist->scene, scene));
}

char *gtk_scenelist_get_movie(GtkSceneList *scenelist, gint scene_num)
{
	GtkScene *scene;
	if (scene_num < 0 || scene_num >= g_list_length(scenelist->scene)) return NULL;
	scene = gtk_scenelist_get_scene(scenelist, scene_num);
	return (char *) g_list_nth_data(scenelist->movie, scene->movie_num);
}


void gtk_scenelist_get_num_drawn(GtkSceneList *scenelist, gint *num, gint *left_border)
{
	/* let's see to find out how many we can actually place */
	if (num)
		*num = GTK_WIDGET(scenelist)->allocation.width/gdk_pixbuf_get_width(scenelist->background);

	/* space left to paint part of previous/next scenes on the left/right? */
	if (left_border)
		*left_border = (GTK_WIDGET(scenelist)->allocation.width -
			GTK_WIDGET(scenelist)->allocation.width/gdk_pixbuf_get_width(scenelist->background) *
			gdk_pixbuf_get_width(scenelist->background))/2;
}


void gtk_scenelist_edit_move(GtkSceneList *scenelist, gint scene_num, gint direction)
{
	GtkScene *scene1, *scene2;
	gint diff, n;

	if (direction != 1 && direction != -1) return;

	if (scene_num < 0 || scene_num >= g_list_length(scenelist->scene)) return;
	if ((scene_num == 0 && direction == -1) ||
		(scene_num  == g_list_length(scenelist->scene)-1 && direction == 1))
		return;
	scene1 = gtk_scenelist_get_scene(scenelist, scene_num);
	scene2 = gtk_scenelist_get_scene(scenelist, scene_num + direction);

	if (direction == -1)
	{
		diff = scene1->start_total - scene2->start_total;
		scene1->start_total -= diff;
		scene2->start_total += diff;
	}
	else
	{
		diff = scene2->start_total - scene1->start_total;
		scene1->start_total += diff;
		scene2->start_total -= diff;
	}

	n = (gint) g_list_nth_data(scenelist->scene, scene_num);
	scenelist->items = g_list_remove_link(scenelist->items, 
		g_list_nth(scenelist->items, n));
	scenelist->items = g_list_insert(scenelist->items,
		(gpointer)scene2, n);

	n = (gint) g_list_nth_data(scenelist->scene, scene_num + direction);
	scenelist->items = g_list_remove_link(scenelist->items, 
		g_list_nth(scenelist->items, n));
	scenelist->items = g_list_insert(scenelist->items,
		(gpointer)scene1, n);

	gtk_signal_emit_by_name(GTK_OBJECT(scenelist), "scenelist_changed");
	gtk_scenelist_draw(GTK_WIDGET(scenelist));
}


void gtk_scenelist_edit_add(GtkSceneList *scenelist, char *movie, gint view_start,
	gint view_end, gint scene_start, gint scene_end, gint scene_num)
{
	GtkScene *scene, *scene1;
	int i;

	if (scene_num < 0 || scene_num > g_list_length(scenelist->scene)) return;
	scene1 = gtk_scenelist_get_scene(scenelist, scene_num);
	for (i=0;i<g_list_length(scenelist->movie);i++)
	{
		if (!strcmp(movie, (char *)g_list_nth_data(scenelist->movie, i)))
			goto no_insert;
	}
	scenelist->movie = g_list_append(scenelist->movie, (gpointer)movie);

no_insert:
	scene = gtk_scene_new(view_start, view_end,
			scene_start, scene_end,
			scene1?scene1->start_total:0, i, movie);

	/* and finally, insert the new scene into the scenelist */
	scenelist->items = g_list_insert(scenelist->items, (gpointer) scene,
		(gint)g_list_nth_data(scenelist->scene, scene_num));
	scenelist->scene = g_list_insert(scenelist->scene,
		g_list_nth_data(scenelist->scene, scene_num),
		scene_num);

	/* change the starting point frame.nr. of each scene and move it */
	for (i=scene_num+1;i<g_list_length(scenelist->scene);i++)
	{
		((gint) g_list_nth(scenelist->scene, i)->data)++;
		scene1 = gtk_scenelist_get_scene(scenelist, i);
		scene1->start_total += (view_end - view_start + 1);
	}

	gtk_signal_emit_by_name(GTK_OBJECT(scenelist), "scenelist_changed");
	gtk_scenelist_draw(GTK_WIDGET(scenelist));
}


void gtk_scenelist_edit_delete(GtkSceneList *scenelist, gint scene_num)
{
	GtkScene *scene;
	int i, diff;

	if (scene_num < 0 || scene_num >= g_list_length(scenelist->scene)) return;
	scene = gtk_scenelist_get_scene(scenelist, scene_num);
	diff = scene->view_end - scene->view_start + 1;
	scenelist->items = g_list_remove_link(scenelist->items, g_list_nth(scenelist->items,
		(gint) g_list_nth_data(scenelist->scene, scene_num)));
	scenelist->scene = g_list_remove_link(scenelist->scene, g_list_nth(scenelist->scene, scene_num));
	free(scene);
	for (i=scene_num;i<g_list_length(scenelist->scene);i++)
	{
		((gint)(g_list_nth(scenelist->scene, i)->data))--;
		scene = gtk_scenelist_get_scene(scenelist, i);
		scene->start_total -= diff;
	}

	gtk_signal_emit_by_name(GTK_OBJECT(scenelist), "scenelist_changed");
	if (scenelist->selected_scene >= g_list_length(scenelist->scene))
		gtk_scenelist_select(scenelist, scenelist->selected_scene - 1);
	else
		gtk_scenelist_draw(GTK_WIDGET(scenelist));
}


void gtk_scenelist_edit_split(GtkSceneList *scenelist, gint scene_num, gint framenum)
{
	GtkScene *scene1, *scene2;
	gint i;

	if (scene_num < 0 || scene_num >= g_list_length(scenelist->scene)) return;
	scene1 = gtk_scenelist_get_scene(scenelist, scene_num);
	if (framenum <= 0 || framenum > (scene1->view_end - scene1->view_start)) return;

	scene2 = gtk_scene_new(scene1->view_start + framenum, scene1->view_end,
			scene1->view_start + framenum, scene1->scene_end,
			scene1->start_total + framenum, scene1->movie_num,
			gtk_scenelist_get_movie(scenelist, scene_num));
	scenelist->items = g_list_insert(scenelist->items, (gpointer)scene2,
		1 + (gint) g_list_nth_data(scenelist->scene, scene_num));
	scenelist->scene = g_list_insert(scenelist->scene,
		(gpointer) (1 + (gint) g_list_nth_data(scenelist->scene, scene_num)),
		scene_num + 1);

	scene1->scene_end = scene1->view_start + framenum - 1;
	scene1->view_end = scene1->view_start + framenum - 1;

	for (i=scene_num+2;i<g_list_length(scenelist->scene);i++)
	{
		((gint)(g_list_nth(scenelist->scene, i)->data))++;
	}

	gtk_signal_emit_by_name(GTK_OBJECT(scenelist), "scenelist_changed");
	gtk_scenelist_draw(GTK_WIDGET(scenelist));
}
