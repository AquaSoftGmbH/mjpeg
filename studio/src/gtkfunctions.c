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

/* Only goal of this file is for some easy access to functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "gtkfunctions.h"

#include "gnome-error.xpm"
#include "gnome-info.xpm"
#include "gnome-warning.xpm"

void gtk_show_text_window(int type, char *format, ...)
{
	extern GtkWidget *window;
	GtkWidget *pop_window, *hbox, *vbox, *label, *button, *hseparator, *pixmap_widget, *vbox2;
	char *title = "Linux Video Studio";
	gchar **data = NULL;
	//int x,y,w,h;
	int n,m;
	va_list args;
	char message[1024];

	va_start (args, format);
	vsnprintf(message, sizeof(message)-1, format, args);

	/* TODO: add '\n's if line is too long */
#define LINE_LENGTH 50
	n = 0;
nextline:
	n += LINE_LENGTH;
	if (n > (int)strlen(message))
		goto done;

	/* check for spaces */
	for (m=n;m>n-LINE_LENGTH;m--)
	{
		if (message[m] == '\n' || message[m] == ' ')
		{
			message[m] = '\n';
			n = m;
			goto nextline;
		}
	}

	/* problem: we need to move on above 50... line's too long */	
	m = n;
	while(1)
	{
		if (m > (int)strlen(message))
			goto done;
		if (message[m] == ' ' || message[m] == '\n')
		{
			message[m] = '\n';
			n = m;
			goto nextline;
		}
		m++;
	}
done:
#undef LINE_LENGTH

	switch(type)
	{
		case STUDIO_WARNING:
			title = "Linux Video Studio - Warning";
			data = (gchar**)gnome_warning_xpm;
			break;
		case STUDIO_ERROR:
			title = "Linux Video Studio - Error";
			data = (gchar**)gnome_error_xpm;
			break;
		case STUDIO_INFO:
			title = "Linux Video Studio - Info";
			data = (gchar**)gnome_info_xpm;
			break;
	}

	pop_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	vbox = gtk_vbox_new(FALSE, 5);

	gtk_window_set_title (GTK_WINDOW(pop_window),title);
	gtk_container_set_border_width (GTK_CONTAINER (pop_window), 20);

	hbox = gtk_hbox_new(FALSE, 10);

	pixmap_widget = gtk_widget_from_xpm_data(data);
	gtk_box_pack_start (GTK_BOX (hbox), pixmap_widget, TRUE, TRUE, 0);
	gtk_widget_show (pixmap_widget);

	vbox2 = gtk_vbox_new(FALSE, 0);

	label = gtk_label_new (message);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show (vbox2);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start (GTK_BOX (vbox), hseparator, TRUE, TRUE, 0);
	gtk_widget_show (hseparator);

	button = gtk_button_new_with_label("  Close  ");
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
		G_CALLBACK(gtk_widget_destroy), G_OBJECT(pop_window));
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_container_add (GTK_CONTAINER (pop_window), vbox);
	gtk_widget_show(vbox);

	gtk_grab_add(pop_window);
	gtk_window_set_transient_for(GTK_WINDOW(pop_window), GTK_WINDOW(window));
//	gdk_window_get_origin(window->window, &x, &y);
//	gdk_window_get_size(window->window, &w, &h);
//	gtk_widget_set_uposition(pop_window, x+w/4, y+h/4);
	gtk_widget_show(pop_window);

	va_end (args);
}

GtkWidget *gtk_widget_from_xpm_data(gchar **data)
{
	GtkWidget *widget;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	pixmap = gdk_pixmap_colormap_create_from_xpm_d( NULL,
		gdk_colormap_get_system(), &mask, NULL,(gchar **)data);
	widget = gtk_pixmap_new(pixmap, mask);

	return widget;
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

GtkWidget *gtk_image_label_button(gchar *text, gchar *tttext, gchar **imagedata, gint spacing, GtkPositionType pos)
{
   GtkWidget *button, *box, *pixmap=NULL, *label;
   GtkTooltips *tooltip;

   button = gtk_button_new();
   if (text && imagedata)
   {
      if (pos == GTK_POS_BOTTOM || pos == GTK_POS_TOP)
         box = gtk_vbox_new(FALSE, spacing);
      else if (pos == GTK_POS_LEFT || pos == GTK_POS_RIGHT)
         box = gtk_hbox_new(FALSE, spacing);
      else
         return NULL;
   }
   tooltip = gtk_tooltips_new();
   gtk_tooltips_set_tip(tooltip, button, tttext, NULL);
   if (imagedata)
   {
      pixmap = gtk_widget_from_xpm_data(imagedata);
      if (pos == GTK_POS_RIGHT || pos == GTK_POS_BOTTOM)
      {
         if (text)
            gtk_box_pack_start(GTK_BOX (box), pixmap, TRUE, FALSE, 0);
         else
            gtk_container_add(GTK_CONTAINER(button), pixmap);
         gtk_widget_show(pixmap);
      }
   }
   if (text)
   {
      label = gtk_label_new(text);
      if (imagedata)
         gtk_box_pack_start(GTK_BOX (box), label, TRUE, FALSE, 0);
      else
         gtk_container_add(GTK_CONTAINER(button), label);
      gtk_widget_show(label);
   }
   if (imagedata && (pos == GTK_POS_LEFT || pos == GTK_POS_TOP))
   {
      if (text)
         gtk_box_pack_start(GTK_BOX (box), pixmap, FALSE, TRUE, 0);
      else
         gtk_container_add(GTK_CONTAINER(button), pixmap);
      gtk_widget_show(pixmap);
   }
   if (text && imagedata)
   {
      gtk_container_add (GTK_CONTAINER (button), box);
      gtk_widget_show(box);
   }

   return button;
}
