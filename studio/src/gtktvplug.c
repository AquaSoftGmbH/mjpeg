/* TV Plug-in widget for Linux Studio
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
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <string.h>

#include "gtktvplug.h"

/* Forward declarations */

static void gtk_tvplug_class_init (GtkTvPlugClass *klass);
static void gtk_tvplug_init (GtkTvPlug *tvplug);
static void gtk_tvplug_destroy (GtkObject *object);
static void gtk_tvplug_realize (GtkWidget *widget);
static void gtk_tvplug_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_tvplug_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_tvplug_expose (GtkWidget *widget, GdkEventExpose *event);
static void gtk_tvplug_set_properties (GtkTvPlug *tvplug, int port);
int guess_port(void);
void gtk_tvplug_query_attributes(GtkWidget *widget);
void show_info(void);

static GtkWidgetClass *parent_class = NULL;

/* ================================================================= */

guint gtk_tvplug_get_type ()
{
	static guint tvplug_type = 0;

	if (!tvplug_type)
	{
		GtkTypeInfo tvplug_info =
		{
			"GtkTvPlug",
			sizeof (GtkTvPlug),
			sizeof (GtkTvPlugClass),
			(GtkClassInitFunc) gtk_tvplug_class_init,
			(GtkObjectInitFunc) gtk_tvplug_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		tvplug_type = gtk_type_unique (gtk_widget_get_type (), &tvplug_info);
	}
	return tvplug_type;
}

static void gtk_tvplug_class_init (GtkTvPlugClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;

	parent_class = gtk_type_class (gtk_widget_get_type ());

	object_class->destroy = gtk_tvplug_destroy;

	widget_class->realize = gtk_tvplug_realize;
	widget_class->expose_event = gtk_tvplug_expose;
	widget_class->size_request = gtk_tvplug_size_request;
	widget_class->size_allocate = gtk_tvplug_size_allocate;
}

static void gtk_tvplug_init (GtkTvPlug *tvplug)
{
	tvplug->port = 0;
	tvplug->brightness = 0;
	tvplug->contrast = 0;
	tvplug->saturation = 0;
	tvplug->hue = 0;
}

int guess_port()
{
	XvAdaptorInfo *ai;
	int ver, rel, req, ev, err;
	int adaptors;
	int i;

	if (Success != XvQueryExtension(GDK_DISPLAY(),&ver,&rel,&req,&ev,&err))
	{
		puts("Server doesn't support Xvideo");
		return 0;
	}

	if (Success != XvQueryAdaptors(GDK_DISPLAY(),DefaultRootWindow(GDK_DISPLAY()),&adaptors,&ai))
	{
		puts("Oops: XvQueryAdaptors failed");
		return 0;
	}

	g_print("Guessing port..... ");

	for (i = 0; i < adaptors; i++)
	{
		if (strcmp(ai[i].name, "video4linux") == 0)
		{
			g_print("Found video4linux-device on port %ld\n", ai[i].base_id);
			return ai[i].base_id;
		}
	}

	return 0;
}

void gtk_tvplug_query_attributes(GtkWidget *widget)
{
	GtkTvPlug *tvplug;
	Atom attr;
	XvAttribute *at;
	int j;
	int attributes, val;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TVPLUG (widget));
	tvplug = GTK_TVPLUG (widget);

	at = XvQueryPortAttributes(GDK_DISPLAY(),tvplug->port,&attributes);
	for (j = 0; j < attributes; j++)
	{
		if (strcmp(at[j].name, "XV_BRIGHTNESS") == 0)
		{
			attr = XInternAtom(GDK_DISPLAY(), at[j].name, False);
			XvGetPortAttribute(GDK_DISPLAY(), tvplug->port, attr, &val);
			tvplug->brightness = val;
		}
		else if (strcmp(at[j].name, "XV_CONTRAST") == 0)
		{
			attr = XInternAtom(GDK_DISPLAY(), at[j].name, False);
			XvGetPortAttribute(GDK_DISPLAY(), tvplug->port, attr, &val);
			tvplug->contrast = val;
		}
		else if (strcmp(at[j].name, "XV_SATURATION") == 0)
		{
			attr = XInternAtom(GDK_DISPLAY(), at[j].name, False);
			XvGetPortAttribute(GDK_DISPLAY(), tvplug->port, attr, &val);
			tvplug->saturation = val;
		}
		else if (strcmp(at[j].name, "XV_HUE") == 0)
		{
			attr = XInternAtom(GDK_DISPLAY(), at[j].name, False);
			XvGetPortAttribute(GDK_DISPLAY(), tvplug->port, attr, &val);
			tvplug->hue = val;
		}
	}
	if (at)
		XFree(at);
}

void gtk_tvplug_set(GtkWidget *widget, char *what, int value)
{
	GtkTvPlug *tvplug;
	Atom atom;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TVPLUG (widget));
	tvplug = GTK_TVPLUG (widget);

	if (strcmp(what, "hue") == 0)
	{
		atom = XInternAtom(GDK_DISPLAY(), "XV_HUE", False);
		tvplug->hue = value;
	}
	else if (strcmp(what, "contrast") == 0)
	{
		atom = XInternAtom(GDK_DISPLAY(), "XV_CONTRAST", False);
		tvplug->contrast = value;
	}
	else if (strcmp(what, "brightness") == 0)
	{
		atom = XInternAtom(GDK_DISPLAY(), "XV_BRIGHTNESS", False);
		tvplug->brightness = value;
	}
	else if (strcmp(what, "colour") == 0)
	{
		atom = XInternAtom(GDK_DISPLAY(), "XV_SATURATION", False);
		tvplug->saturation = value;
	}
	else
	{
		printf("Value \"%s\" unknown!\n", what);
		return;
	}

	XvSetPortAttribute(GDK_DISPLAY(), tvplug->port,atom,value);
}

void show_info()
{
	Atom attr;
	XvAdaptorInfo *ai;
	XvEncodingInfo *ei;
	XvAttribute *at;
	XvImageFormatValues *fo;
	int ver, rel, req, ev, err, val;
	int adaptors,encodings,attributes,formats;
	int i,j,p;

	/* query+print Xvideo properties */
	if (Success != XvQueryExtension(GDK_DISPLAY(),&ver,&rel,&req,&ev,&err))
	{
		puts("Server doesn't support Xvideo");
	}

	if (Success != XvQueryAdaptors(GDK_DISPLAY(),DefaultRootWindow(GDK_DISPLAY()),&adaptors,&ai))
	{
		puts("Oops: XvQueryAdaptors failed");
	}

	printf("%d adaptors available.\n",adaptors);
	for (i = 0; i < adaptors; i++)
	{
		printf("  name:  %s\n"
		"  type:  %s%s%s%s%s\n"
		"  ports: %ld\n"
		"  first: %ld\n",
		ai[i].name,
		(ai[i].type & XvInputMask)  ? "input "  : "",
		(ai[i].type & XvOutputMask) ? "output " : "",
		(ai[i].type & XvVideoMask)  ? "video "  : "",
		(ai[i].type & XvStillMask)  ? "still "  : "",
		(ai[i].type & XvImageMask)  ? "image "  : "",
		ai[i].num_ports,
		ai[i].base_id);
		printf("  format list\n");

		for (j = 0; j < ai[i].num_formats; j++)
		{
			printf("    depth=%d, visual=%ld\n",
			ai[i].formats[j].depth,
			ai[i].formats[j].visual_id);
		}

		for (p = ai[i].base_id; p < ai[i].base_id+ai[i].num_ports; p++)
		{
			printf("  encoding list for port %d\n",p);
			if (Success != XvQueryEncodings(GDK_DISPLAY(), p, &encodings, &ei))
			{
				puts("Oops: XvQueryEncodings failed");
				continue;
			}

			for (j = 0; j < encodings; j++)
			{
				printf("    id=%ld, name=%s, size=%ldx%ld\n",
				ei[j].encoding_id, ei[j].name,
				ei[j].width, ei[j].height);		
			}
			XvFreeEncodingInfo(ei);

			printf("  attribute list for port %d\n",p);
			at = XvQueryPortAttributes(GDK_DISPLAY(),p,&attributes);
			for (j = 0; j < attributes; j++)
			{
				fprintf(stderr,"    %s%s%s, %i -> %i",
				at[j].name,
				(at[j].flags & XvGettable) ? " get" : "",
				(at[j].flags & XvSettable) ? " set" : "",
				at[j].min_value,at[j].max_value);
				attr = XInternAtom(GDK_DISPLAY(), at[j].name, False);

				if (at[j].flags & XvGettable)
				{
					XvGetPortAttribute(GDK_DISPLAY(), p, attr, &val);
					fprintf(stderr,", val=%d",val);
				}
				fprintf(stderr,"\n");
			}
			if (at)
				XFree(at);

			fo = XvListImageFormats(GDK_DISPLAY(), p, &formats);
			printf("  image format list for port %d\n",p);
			for(j = 0; j < formats; j++)
			{
				fprintf(stderr, "    0x%x (%4.4s) %s\n",
				fo[j].id,
				(char*)&fo[j].id,
				(fo[j].format == XvPacked) ? "packed" : "planar");
			}
			if (fo)
			XFree(fo);
		}
		printf("\n");
	}
	if (adaptors > 0)
		XvFreeAdaptorInfo(ai);
}

GtkWidget* gtk_tvplug_new (int port)
{
	GtkTvPlug *tvplug;

	if (port == -1)
	{
		show_info();
		return NULL;
	}
	else if (port == 0)
	{
		port = guess_port();
		if (port == 0)
		{
			g_print("No suitable video4linux port found,"
				" please supply one by using the \"-p <num>\" switch."
				" Use the \"-t\" switch to see the list of ports available on your computer.\n"
				"If you cannot find a video4linux device, you probably need to enable"
				" the Xvideo extension in your X-server\n");
			return NULL;
		}
		else
		{
			tvplug = gtk_type_new (gtk_tvplug_get_type ());
			gtk_tvplug_set_properties(tvplug, port); //, width, height);
		}
	}
	else
	{
		tvplug = gtk_type_new (gtk_tvplug_get_type ());
		gtk_tvplug_set_properties(tvplug, port); //, width, height);
	}

	gtk_tvplug_query_attributes(GTK_WIDGET(tvplug));

	return GTK_WIDGET (tvplug);
}

static void gtk_tvplug_destroy (GtkObject *object)
{
	GtkTvPlug *tvplug;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_TVPLUG (object));

	tvplug = GTK_TVPLUG (object);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

void gtk_tvplug_set_properties (GtkTvPlug *tvplug, int port)
{
	g_return_if_fail (tvplug != NULL);
	g_return_if_fail (GTK_IS_TVPLUG (tvplug));

	tvplug->port = port;
}

static void gtk_tvplug_realize (GtkWidget *widget)
{
	GtkTvPlug *tvplug;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TVPLUG (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
	tvplug = GTK_TVPLUG (widget);

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

static void gtk_tvplug_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GtkTvPlug *tvplug;

	tvplug = GTK_TVPLUG (widget);

	requisition->width = 256;
	requisition->height = 192;
}

static void gtk_tvplug_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkTvPlug *tvplug;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TVPLUG (widget));
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;
	tvplug = GTK_TVPLUG (widget);

	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
			allocation->x, allocation->y,
			allocation->width, allocation->height);
	}
}

void draw_tv(GtkWidget *widget, int port)
{
	GdkGC *gc;
	GtkTvPlug *tvplug;
	int w,h,x,y;

	tvplug = GTK_TVPLUG(widget);

	tvplug->port = port;

	w = widget->allocation.width;
	h = widget->allocation.height;
	x = 0;
	y = 0;

	/* draw TV */
	gc = gdk_gc_new(GTK_WIDGET(tvplug)->window);
	XvSelectPortNotify(GDK_DISPLAY(), tvplug->port, 1);
	XvSelectVideoNotify(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(widget->window), 1);
	XvPutVideo(GDK_DISPLAY(),tvplug->port,GDK_WINDOW_XWINDOW(widget->window),
		GDK_GC_XGC(gc),x,y,w,h,x,y,w,h);
	gdk_gc_destroy(gc);
}

static gint gtk_tvplug_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GtkTvPlug *tvplug;
	GdkGC *gc;
	int w,h, x, y;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_TVPLUG (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->count > 0)
		return FALSE;
 
	tvplug = GTK_TVPLUG (widget);

	gdk_window_clear_area (widget->window,
		0, 0,
		widget->allocation.width,
		widget->allocation.height);

	w = widget->allocation.width;
	h = widget->allocation.height;
	x = 0;
	y = 0;

	gc = gdk_gc_new(GTK_WIDGET(tvplug)->window);
	XvSelectPortNotify(GDK_DISPLAY(), tvplug->port, 1);
	XvSelectVideoNotify(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(widget->window), 1);
	XvPutVideo(GDK_DISPLAY(),tvplug->port,GDK_WINDOW_XWINDOW(widget->window),
		GDK_GC_XGC(gc),x,y,w,h,x,y,w,h);
	gdk_gc_destroy(gc);

	return FALSE;
}
