/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * lavencode_util done by Bernhard Praschinger
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

/* 
 * Here we create some additional functions, like setting the active size 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
/* #include <gnome.h>   Seems that I do not need that. */
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

#include <unistd.h>
#include <sys/stat.h>

#include "studio.h"

/* Size of drawing area */
#define WIDTH 720
#define HEIGHT 576

/* Forward declarations */
void quit_cb (GtkWidget *widget);
void expose_event_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data);
gint event_cb(GtkWidget *widget, GdkEvent *event, gpointer data);
void view_image (void );

static void create_ok_cancel(GtkWidget *hbox, GtkWidget *picture_window);
static void show_selected_points (GtkWidget *hbox);
static void show_selection(void);
static void show_selected_points(GtkWidget *hbox);
static void set_defaults(void);


/* Some common variables */
/* Visible drawing surface */
GtkWidget *drawing_area, *picture_window, *point1_entry, *point2_entry;

struct point {
             int x;
             int y;
             };

struct rect {
            struct point p1;
            struct point p2;
            };

struct rect raw_points;   /** holds the mouse selected points */
struct rect small_points; /** holds the smaller area calc from the raw points */
struct rect large_points; /** holds the larger area calc from the raw points */

/* Raw pixel buffer */
static guchar drawbuf[WIDTH * HEIGHT * 6];
GdkPixbuf *loader_pixbuf = NULL;
guchar *filebuf = NULL;

/* Double-buffer pixmap */
GdkPixmap *dbuf_pixmap = NULL;
int mark_mode = 0;
int valid_rect = 0;
int redraw_rect = 0;


/* =============================================================== */
/* Start of the code */

/** Here we set all values to predefined one */
void set_defaults()
{
raw_points.p1.x = 0;
raw_points.p1.y = 0;
raw_points.p1.x = 0;
raw_points.p2.y = 0;

small_points.p1.x = 0;
small_points.p1.y = 0;
small_points.p1.x = 0;
small_points.p2.y = 0;

large_points.p1.x = 0;
large_points.p1.y = 0;
large_points.p1.x = 0;
large_points.p2.y = 0;

}

/** Here we create the OK and Cancel Button 
    @param hbox the box where we insert the two buttons 
    @param control_window on window we need to close */
void create_ok_cancel(GtkWidget *hbox, GtkWidget *control_window)
{
GtkWidget *button;

  button = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", 
                GTK_SIGNAL_FUNC(quit_cb), button );
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                            gtk_widget_destroy, GTK_OBJECT(control_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", 
                GTK_SIGNAL_FUNC(quit_cb), button );
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
          gtk_widget_destroy, GTK_OBJECT(control_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

}

/** here we close the grafic window 
    @para widget a dummy option */
void quit_cb (GtkWidget *widget)
{
  g_free(filebuf);
  gtk_widget_destroy(GTK_WIDGET(picture_window));
}

void expose_event_cb(GtkWidget *widget, GdkEventExpose *event,
    gpointer data)
{
  /* Don't repaint entire window upon each exposure */
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

  /* Refresh double buffer, then copy the "dirtied" area to
   * the on-screen GdkWindow
   */
  gdk_window_copy_area(widget->window,
    widget->style->fg_gc[GTK_STATE_NORMAL],
    event->area.x, event->area.y,
    dbuf_pixmap,
    event->area.x, event->area.y,
    event->area.width, event->area.height);
}

gint event_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
char val1[LONGOPT], val2[LONGOPT];
int found = FALSE;
  GdkEventButton *bevent = (GdkEventButton *)event;
  GdkEventMotion *mevent = (GdkEventMotion *)mevent;

  switch ((gint)event->type)
  {
  case GDK_BUTTON_PRESS:
    /* Handle mouse button press */
      if (verbose)
        printf ("Button press on %g and %g.\n", bevent->x, bevent->y);
      mark_mode = 1;
      valid_rect = 1;
      redraw_rect = 1;
      found = TRUE;
      raw_points.p1.x = bevent->x;
      raw_points.p1.y = bevent->y;
      sprintf(val1, " %g / %g ",bevent->x, bevent->y);
      gtk_entry_set_text(GTK_ENTRY(point1_entry), val1);
      break;

  case GDK_BUTTON_RELEASE:
    /* Handle mouse button release */
      if (verbose)
        printf ("Button release on %g and %g.\n", bevent->x, bevent->y);
      mark_mode = 0;
      found = TRUE;
      raw_points.p2.x = bevent->x;
      raw_points.p2.y = bevent->y;
      sprintf(val2, " %g / %g ",bevent->x, bevent->y);
      gtk_entry_set_text(GTK_ENTRY(point2_entry), val2);
      break;
  }

  /* Event not handled; try parent item */
  return found;
}

static void init_drawing_buffer(  )
{
  gint x, y;
  gint pixel_offset;
  gint rowstride = WIDTH * 3;

  for (y = 0; y < HEIGHT; y++)
  {
    for (x = 0; x < WIDTH; x++)
    {
      pixel_offset = y * rowstride + x * 3;

      drawbuf[pixel_offset] = y * 255 / HEIGHT;
      drawbuf[pixel_offset + 1] = 128 - (x + y) * 255 /
        (WIDTH * HEIGHT);
      drawbuf[pixel_offset + 2] = x * 255 / WIDTH;
    }
  }
}

/* Hier wird das Bild geladen, und angezeigt */
void view_image ()
{
gint width;
gint height;
GdkPixbuf *pixbuf1;
gchar *filename = ("file.png");
  
  if (verbose)
    printf (" Rendering %s \n", filename);
  pixbuf1 = gdk_pixbuf_new_from_file(filename);
  width = gdk_pixbuf_get_width(pixbuf1);
  height = gdk_pixbuf_get_height(pixbuf1);

  if (verbose)
    printf(" Picture loaded, width %i, height %i \n", width, height);

  gdk_pixbuf_render_to_drawable_alpha(pixbuf1, dbuf_pixmap,
    0, 0, 0, 0, width, height,
    GDK_PIXBUF_ALPHA_BILEVEL, 128,
    GDK_RGB_DITHER_NORMAL, 0, 0);

 gdk_pixbuf_unref(pixbuf1);
}

/** Here we create the the line containing the selected points */
void show_selected_points(GtkWidget *hbox)
{
GtkWidget *label;

  label = gtk_label_new("Point 1 (x/y) :");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  point1_entry = gtk_entry_new();
  gtk_widget_set_usize(point1_entry, 70, -2);
  gtk_box_pack_start(GTK_BOX(hbox), point1_entry, FALSE, FALSE, 0);
  gtk_widget_show(point1_entry);

  label = gtk_label_new("Point 2 (x/y) :");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  point2_entry = gtk_entry_new();
  gtk_widget_set_usize(point2_entry, 70, -2);
  gtk_box_pack_start(GTK_BOX(hbox), point2_entry, FALSE, FALSE, 0);
  gtk_widget_show(point2_entry);


}

/** Here we create the 1rst window with the controls */
void create_window_select(char filename[LONGOPT], char *size[LONGOPT])
{
GtkWidget *control_window, *vbox, *hbox;

  set_defaults();
 
  control_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (control_window), "LVS - Area control");

  vbox = gtk_vbox_new (FALSE, 2);

  hbox = gtk_hbox_new (TRUE, 5);
  show_selected_points (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox);

  hbox = gtk_hbox_new (TRUE, 10);  /* Creation of the OK an cancel Button */
  create_ok_cancel(hbox, control_window);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox);

  gtk_container_add (GTK_CONTAINER (control_window), vbox);
  gtk_widget_show (vbox);
  gtk_widget_show (control_window); 

  show_selection ();
}

/** Here we create the new window for the active size selection */
void show_selection()
{
GtkWidget *vbox, *hbox;
GdkPixbuf *pixbuf;

printf("\n in lavencode_util.c \n");

  gtk_widget_push_visual(gdk_rgb_get_visual(  ));
  gtk_widget_push_colormap(gdk_rgb_get_cmap(  ));
  picture_window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_widget_pop_visual(  );
  gtk_widget_pop_colormap(  );

  gtk_window_set_title(GTK_WINDOW (picture_window), "LVS - area select");

  vbox = gtk_vbox_new (FALSE,2);
  

  hbox = gtk_hbox_new (TRUE,5);
  gtk_widget_set_usize(GTK_WIDGET(picture_window), WIDTH, HEIGHT);
  gtk_window_set_policy(GTK_WINDOW(picture_window), TRUE, FALSE, FALSE);

  /* Create drawing-area widget */
  drawing_area = gtk_drawing_area_new (  );
  gtk_widget_add_events(GTK_WIDGET(drawing_area),
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  gtk_signal_connect(GTK_OBJECT(drawing_area), "expose_event",
    GTK_SIGNAL_FUNC(expose_event_cb), NULL);

  /* Mouse click handling (any event actually) */
  gtk_signal_connect(GTK_OBJECT(drawing_area), "event",
                     GTK_SIGNAL_FUNC(event_cb), NULL);

  gtk_window_set_title (GTK_WINDOW (picture_window), "LVS - Area select");
  gtk_container_set_border_width (GTK_CONTAINER (picture_window), 0);

  gtk_container_add (GTK_CONTAINER (picture_window), drawing_area);

  gtk_widget_show_all(picture_window);
  /* Create double-buffered pixmap; must do it here, after
   * app->window is created. Inherits color map and visual
   * from app.
   */
  dbuf_pixmap = gdk_pixmap_new(picture_window->window, WIDTH, HEIGHT, -1);

  /* Create a GdkPixbuf out of our background gradient, then
   * copy it to our double-buffered pixmap */
  init_drawing_buffer(  );

  pixbuf = gdk_pixbuf_new_from_data(drawbuf, GDK_COLORSPACE_RGB,
    FALSE, 8, WIDTH, HEIGHT, WIDTH * 3, NULL, NULL);
  gdk_pixbuf_render_to_drawable(pixbuf, dbuf_pixmap,
    picture_window->style->fg_gc[GTK_STATE_NORMAL],
    0, 0, 0, 0, WIDTH, HEIGHT,
    GDK_RGB_DITHER_NORMAL, 0, 0);

  /* loading Images here */
  view_image();

  gtk_box_pack_start (GTK_BOX (hbox), picture_window, FALSE, FALSE, 5);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox);

  gtk_container_add (GTK_CONTAINER (picture_window), vbox);
  gtk_widget_show (vbox);
  gtk_widget_show (picture_window);

}
