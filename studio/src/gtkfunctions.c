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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "gtkfunctions.h"

#include "gnome-error.xpm"
#include "gnome-info.xpm"
#include "gnome-warning.xpm"

void gtk_show_text_window(int type, char *message, char *message2)
{
	GtkWidget *window, *hbox, *vbox, *label, *button, *hseparator, *pixmap_widget, *vbox2;
	char *title = "Linux Video Studio";
	gchar **data = NULL;

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

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	vbox = gtk_vbox_new(FALSE, 5);

	gtk_window_set_title (GTK_WINDOW(window),title);
	gtk_container_set_border_width (GTK_CONTAINER (window), 20);

	hbox = gtk_hbox_new(FALSE, 10);

	pixmap_widget = gtk_widget_from_xpm_data(data);
	gtk_box_pack_start (GTK_BOX (hbox), pixmap_widget, TRUE, TRUE, 0);
	gtk_widget_show (pixmap_widget);

	vbox2 = gtk_vbox_new(FALSE, 0);

	label = gtk_label_new (message);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	if (message2)
	{
		label = gtk_label_new (message2);
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
		gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, TRUE, 0);
		gtk_widget_show (label);
	}

	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show (vbox2);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start (GTK_BOX (vbox), hseparator, TRUE, TRUE, 0);
	gtk_widget_show (hseparator);

	button = gtk_button_new_with_label("  Close  ");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(window));
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show(vbox);

	gtk_grab_add(window);
	gtk_widget_show(window);
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
