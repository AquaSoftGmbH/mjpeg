/* Image Plug-in widget for Linux Studio
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

#ifndef __GTK_IMAGEPLUG_H__
#define __GTK_IMAGEPLUG_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdrawingarea.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_IMAGEPLUG(obj)          GTK_CHECK_CAST (obj, gtk_imageplug_get_type (), GtkImagePlug)
#define GTK_IMAGEPLUG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_IMAGEPLUG_get_type (), GtkImagePlugClass)
#define GTK_IS_IMAGEPLUG(obj)       GTK_CHECK_TYPE (obj, gtk_imageplug_get_type ())


typedef struct _GtkImagePlug        GtkImagePlug;
typedef struct _GtkImagePlugClass   GtkImagePlugClass;

struct _GtkImagePlug
{
	GtkWidget widget;
	/* backgroud and image of movie */
	GdkPixbuf *background;
	GdkPixbuf *picture;

	/* size of image/background image */
	int width;
	int height;
	int back_width;
	int back_height;

	/* At which position (of background) to draw the picture */
	int x;
	int y;

	/* Whether the picture is selected */
	int selection;

	/* Position in the array */
	int number;

	/* Name of image/video-file */
	char video_filename[256];

	/* Frame-nums of scene */
	int startscene;
	int stopscene;

	/* Frame-limits of which frames to show in movie */
	int startframe;
	int stopframe;

	/* Where this scene starts in the total array of scenes */
	int start_total;
};

struct _GtkImagePlugClass
{
	GtkWidgetClass parent_class;
	void (*button_press_event) (GtkImagePlug *imageplug, GdkEventButton *event);
};

GtkWidget* gtk_imageplug_new(GdkPixbuf *picture, char *video_filename,
	int startscene, int stopscene, int startframe,
	int stopframe, int start_total, GdkPixbuf *back, int num);
GtkWidget* gtk_imageplug_new_from_video (char *filename, int start, int stop,
	int scene_start, int scene_stop,int start_total,
	GdkPixbuf *back, int num);
guint gtk_imageplug_get_type (void);
gint gtk_imageplug_draw(GtkWidget *widget);
void gtk_imageplug_copy(GtkImagePlug *imageplug, GtkImagePlug *src);
void gtk_imageplug_set_data(GtkImagePlug *imageplug, GdkPixbuf *picture,
	char *video_filename, int startscene,
	int stopscene, int startframe, int stopframe, int start_total);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_IMAGEPLUG_H__ */
