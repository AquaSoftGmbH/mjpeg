/* Image Plug-in widget for Linux Video Studio
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
#include <time.h>

#include "gtkimageplug.h"
#include "pipes.h"

/*enum
{
	CLICKED,
	LAST_SIGNAL
};*/

extern int verbose;

/* Forward declarations */

static void gtk_imageplug_class_init (GtkImagePlugClass *klass);
static void gtk_imageplug_init (GtkImagePlug *imageplug);
static void gtk_imageplug_destroy (GtkObject *object);
static void gtk_imageplug_realize (GtkWidget *widget);
static void gtk_imageplug_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_imageplug_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_imageplug_expose (GtkWidget *widget, GdkEventExpose *event);
static int gtk_imageplug_selection_clear(GtkWidget *widget, GdkEventSelection *event, gint *selection);
static gint gtk_imageplug_button_press (GtkWidget *widget, GdkEventButton *event);
//static gint gtk_imageplug_draw(GtkWidget *widget);

//static guint gtk_imageplug_signals[LAST_SIGNAL] = { 0 };
static GtkWidgetClass *parent_class = NULL;

guint gtk_imageplug_get_type ()
{
	static guint imageplug_type = 0;

	if (!imageplug_type)
	{
		GtkTypeInfo imageplug_info =
		{
			"GtkImagePlug",
			sizeof (GtkImagePlug),
			sizeof (GtkImagePlugClass),
			(GtkClassInitFunc) gtk_imageplug_class_init,
			(GtkObjectInitFunc) gtk_imageplug_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		imageplug_type = gtk_type_unique (gtk_widget_get_type (), &imageplug_info);
	}
	return imageplug_type;
}

static void gtk_imageplug_class_init (GtkImagePlugClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;

	parent_class = gtk_type_class (gtk_widget_get_type ());

	object_class->destroy = gtk_imageplug_destroy;

	widget_class->realize = gtk_imageplug_realize;
	widget_class->expose_event = gtk_imageplug_expose;
	widget_class->size_request = gtk_imageplug_size_request;
	widget_class->size_allocate = gtk_imageplug_size_allocate;
	widget_class->button_press_event = gtk_imageplug_button_press;

	/*gtk_imageplug_signals[CLICKED] = gtk_signal_new("clicked", GTK_RUN_FIRST, object_class->type,
		GTK_SIGNAL_OFFSET(GtkImagePlugClass, clicked), gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 1,
		GTK_TYPE_GDK_EVENT);

	gtk_object_class_add_signals(object_class, gtk_imageplug_signals, LAST_SIGNAL);*/
}

static int gtk_imageplug_selection_clear(GtkWidget *widget, GdkEventSelection *event, gint *selection)
{
	*selection = 0;
	gtk_imageplug_draw(widget);
	return TRUE;
}


static void gtk_imageplug_init (GtkImagePlug *imageplug)
{
	imageplug->selection = 0;
	imageplug->x = 16;
	imageplug->y = 25;
	imageplug->width = 100;
	imageplug->height = 75;
	gtk_signal_connect (GTK_OBJECT(imageplug), "selection_clear_event",
		GTK_SIGNAL_FUNC (gtk_imageplug_selection_clear),&(imageplug->selection));
}

GtkWidget *gtk_imageplug_new(GdkPixbuf *picture, char *video_filename,
	int startscene, int stopscene, int startframe,
	int stopframe, int start_total, GdkPixbuf *back, int num)
{
	GtkImagePlug *imageplug;

	imageplug = gtk_type_new (gtk_imageplug_get_type ());

	imageplug->picture = picture;
	sprintf(imageplug->video_filename, video_filename);
	imageplug->startscene = startscene;
	imageplug->stopscene = stopscene;
	imageplug->startframe = startframe;
	imageplug->stopframe = stopframe;
	imageplug->start_total = start_total;
	imageplug->background = back;

	if (back != NULL)
	{
		imageplug->back_width = gdk_pixbuf_get_width(back);
		imageplug->back_height = gdk_pixbuf_get_height(back);
	}
	else
	{
		imageplug->back_width = 104;
		imageplug->back_height = 79;
		imageplug->x = 2;
		imageplug->y = 2;
	}

	imageplug->number = num;

	return GTK_WIDGET(imageplug);
}

GtkWidget* gtk_imageplug_new_from_video (char *filename, int start, int stop,
	int scene_start, int scene_stop, int start_total, GdkPixbuf *back, int num)
{
	GtkImagePlug *imageplug;
	char filename_img_tmp[256], command[256];
	GdkPixbuf *temp = NULL;

	imageplug = gtk_type_new (gtk_imageplug_get_type ());

	imageplug->startframe = start;
	imageplug->stopframe = stop;

	if (strcmp(filename, "") != 0)
	{
		sprintf(filename_img_tmp, "%s/.studio/.temp.jpg", getenv("HOME"));
		sprintf(command, "\"%s\" -o \"%s\" -f i -i %d \"%s\"%s",
			app_location(LAVTRANS), filename_img_tmp, start, filename,
			verbose?"":" >> /dev/null 2>&1");
		system(command);
		temp = gdk_pixbuf_new_from_file (filename_img_tmp);
		unlink(filename_img_tmp);
	}

	strcpy(imageplug->video_filename, filename);
	imageplug->background = back;
	imageplug->start_total = start_total;
	if (strcmp(filename, "") != 0)
	{
		imageplug->picture = gdk_pixbuf_scale_simple(temp,imageplug->width,
			imageplug->height,GDK_INTERP_NEAREST);
	}
	else
	{
		imageplug->picture = NULL;
	}

	if (back != NULL)
	{
		imageplug->back_width = gdk_pixbuf_get_width(back);
		imageplug->back_height = gdk_pixbuf_get_height(back);
	}
	else
	{
		imageplug->back_width = 104;
		imageplug->back_height = 79;
		imageplug->x = 2;
		imageplug->y = 2;
	}

	imageplug->number = num;
	imageplug->startscene = scene_start;
	imageplug->stopscene = scene_stop;

	return GTK_WIDGET (imageplug);
}

static void gtk_imageplug_destroy (GtkObject *object)
{
	GtkImagePlug *imageplug;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_IMAGEPLUG (object));

	imageplug = GTK_IMAGEPLUG (object);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void gtk_imageplug_realize (GtkWidget *widget)
{
	GtkImagePlug *imageplug;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMAGEPLUG (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
	imageplug = GTK_IMAGEPLUG (widget);

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


static gint gtk_imageplug_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GtkImagePlug *imageplug;
	imageplug = GTK_IMAGEPLUG (widget);

	if (event->button == 1)
	{
		if (imageplug->picture)
		{
			imageplug->selection = 1; /*gtk_selection_owner_set (widget,
				GDK_SELECTION_PRIMARY,GDK_CURRENT_TIME);*/
			gtk_imageplug_draw(widget);
			/*gtk_signal_emit(GTK_OBJECT(imageplug), gtk_imageplug_signals[CLICKED], event);*/
			/*gtk_signal_emit_by_name(GTK_OBJECT(imageplug), "button_press_event", event);*/
		}
	}
/*	else if (event->button == 2)
	{
		//do something :-)
	}
	else if (event->button == 3)
	{
		//display menu?
	}*/

	return FALSE;
}

static void gtk_imageplug_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GtkImagePlug *imageplug;

	imageplug = GTK_IMAGEPLUG (widget);

	requisition->width = imageplug->back_width;
	requisition->height = imageplug->back_height;
}

static void gtk_imageplug_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkImagePlug *imageplug;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMAGEPLUG (widget));
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;
	imageplug = GTK_IMAGEPLUG (widget);

	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
			allocation->x, allocation->y,
			allocation->width, allocation->height);
	}
}

static gint gtk_imageplug_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GtkImagePlug *imageplug;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_IMAGEPLUG (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->count > 0)
		return FALSE;
 
	imageplug = GTK_IMAGEPLUG (widget);

	gtk_imageplug_draw(widget);

	return FALSE;
}

gint gtk_imageplug_draw(GtkWidget *widget)
{
	GtkImagePlug *imageplug;

	g_return_val_if_fail (GTK_IS_IMAGEPLUG (widget), FALSE);
	imageplug = GTK_IMAGEPLUG (widget);

	gdk_window_clear_area (widget->window, 0, 0,
		widget->allocation.width, widget->allocation.height);

	if (imageplug->background != NULL)
	{
		gdk_pixbuf_render_to_drawable (imageplug->background, widget->window,
			widget->style->white_gc, 0, 0, 0, 0, imageplug->back_width,
			imageplug->back_height, GDK_RGB_DITHER_NORMAL, 0, 0);
	}

	if (imageplug->selection)
	{
		gdk_draw_rectangle(widget->window, widget->style->white_gc,TRUE, imageplug->x-2, imageplug->y-2,
			imageplug->width+4,imageplug->height+4);
	}
	else
	{
		gdk_draw_rectangle(widget->window, widget->style->black_gc,TRUE, imageplug->x-1, imageplug->y-1,
			imageplug->width+2,imageplug->height+2);
	}

	if (imageplug->picture != NULL)
	{
		gdk_pixbuf_render_to_drawable (imageplug->picture, widget->window, widget->style->white_gc, 0, 0,
			imageplug->x, imageplug->y, imageplug->width, imageplug->height, GDK_RGB_DITHER_NORMAL, 0, 0);
	}

	return TRUE;
}

/* copy data from src to imageplug (by gtk_imageplug_set_data()) */
void gtk_imageplug_copy(GtkImagePlug *imageplug, GtkImagePlug *src)
{
	g_return_if_fail (imageplug != NULL);

	gtk_imageplug_set_data(imageplug,
		src->picture,
		src->video_filename,
		src->startscene,
		src->stopscene,
		src->startframe,
		src->stopframe,
		src->start_total);
}

/* set data of imageplug and call gtk_imageplug_draw() */
void gtk_imageplug_set_data(GtkImagePlug *imageplug, GdkPixbuf *picture,
	char *video_filename, int startscene, int stopscene, int startframe,
	int stopframe, int start_total)
{
	g_return_if_fail (imageplug != NULL);

	imageplug->picture = picture;
	sprintf(imageplug->video_filename, video_filename);
	imageplug->startscene = startscene;
	imageplug->stopscene = stopscene;
	imageplug->startframe = startframe;
	imageplug->stopframe = stopframe;
	imageplug->start_total = start_total;

	gtk_imageplug_draw(GTK_WIDGET(imageplug));
}
