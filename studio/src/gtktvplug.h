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

#ifndef __GTK_TVPLUG_H__
#define __GTK_TVPLUG_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TVPLUG(obj)          GTK_CHECK_CAST (obj, gtk_tvplug_get_type (), GtkTvPlug)
#define GTK_TVPLUG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_tvplug_get_type (), GtkTvPlugClass)
#define GTK_IS_TVPLUG(obj)       GTK_CHECK_TYPE (obj, gtk_tvplug_get_type ())


typedef struct _GtkTvPlug        GtkTvPlug;
typedef struct _GtkTvPlugClass   GtkTvPlugClass;

struct _GtkTvPlug
{
	GtkWidget widget;
	int port;
	int hue;
	int brightness;
	int saturation;
	int contrast;
};

struct _GtkTvPlugClass
{
	GtkWidgetClass parent_class;
};


GtkWidget* gtk_tvplug_new (int port);
guint gtk_tvplug_get_type (void);
void draw_tv(GtkWidget *widget, int port);
void gtk_tvplug_set(GtkWidget *widget, char *what, int value);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TVPLUG_H__ */
