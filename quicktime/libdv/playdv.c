/* 
 *  playdv.c
 *
 *     Copyright (C) Charles 'Buck' Krasic - April 2000
 *     Copyright (C) Erik Walthinsen - April 2000
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  decoder.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */

#define GTKDISPLAY 1
#define Y_ONLY 0

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include "libdv.h"

int main(int argc, char *argv[]) 
{
	FILE *f;
	static gint frame_count;
	GtkWidget *window, *image;
	guint8 rgb_frame[720 * 576 * 4];
	unsigned char *rgb_rows[576];
	unsigned char *buffer = calloc(1, 144000);
	dv_t *dv;
	int i;
	unsigned char *audio_buffer = calloc(1, 131072);

// Initialize DV decoder
	dv = dv_new();

// Open file
	if (argc >= 2)
    	f = fopen(argv[1],"r");
	else
    	f = stdin;

// Initialize display
	gtk_init(&argc,&argv);  
	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	image = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window),image);
	gtk_drawing_area_size(GTK_DRAWING_AREA(image), 720, 480);
	gtk_widget_set_usize(GTK_WIDGET(image), 720, 480);
	gtk_widget_show(image);
	gtk_widget_show(window);
	gdk_flush();
	while (gtk_events_pending())
    	gtk_main_iteration();
	gdk_flush();

	for(i = 0; i < 576; i++)
	{
		rgb_rows[i] = &rgb_frame[720 * 4 * i];
	}

// Read a frame
	while(!feof(f)) 
	{
		if(fread(buffer, DV_NTSC_SIZE, 1, f) < 1) break;
		dv_read_video(dv, rgb_rows, buffer, DV_NTSC_SIZE, DV_RGB8880);
//		dv_read_audio(dv, audio_buffer, buffer, DV_NTSC_SIZE);

    	frame_count++;
#if GTKDISPLAY
    	gdk_draw_rgb_32_image(image->window, image->style->fg_gc[image->state],
                        	0, 0, 720, 480, GDK_RGB_DITHER_NORMAL, rgb_frame, 720 * 4);
    	gdk_flush();
    	while(gtk_events_pending())
    	  	gtk_main_iteration();
    	gdk_flush();
#endif
	}

	dv_delete(dv);
}
