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
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

#include <unistd.h>
#include <sys/stat.h>

#include "studio.h"

/* Size of drawing area */
#define WIDTH 768
#define HEIGHT 576

/* Forward declarations */
void quit_cb (GtkWidget *widget);
void expose_event_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data);
gint event_cb(GtkWidget *widget, GdkEvent *event, gpointer data);
void view_image (void);
void load_image (char filename[FILELEN]);

static void create_ok_cancel(GtkWidget *hbox, GtkWidget *picture_window);
static void show_selected_points (GtkWidget *hbox);
static void show_selection(char filename[FILELEN]);
static void show_selected_points(GtkWidget *hbox);
static void set_defaults(void);
static void calc_area(void); 
static void show_active_points (GtkWidget *hbox);
static void update_text_fields(void);
static void accept_changes(GtkWidget *widget, gpointer data);

/* Some common variables */
/* Visible drawing surface */
GtkWidget *drawing_area, *picture_window, *point1_entry, *point2_entry;
GtkWidget *button_s, *button_l, *button_m, *actfield_p1s, *actfield_p1l;
GtkWidget *actfield_p2s, *actfield_p2l, *actfield_s, *actfield_l, *actfield_m;

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
struct point imgsize; /** Holds the size of the image */

/* Raw pixel buffer */
static guchar drawbuf[WIDTH * HEIGHT * 6];
GdkPixbuf *loader_pixbuf = NULL;
guchar *filebuf = NULL;

/* Double-buffer pixmap */
GdkPixmap *dbuf_pixmap = NULL;
int mark_mode = 0;
int valid_rect = 0;
int redraw_rect = 0;

/* for loading the image */
gint width;
gint height;
GdkPixbuf *pixbuf1;

/* =============================================================== */
/* Start of the code */

/** Here we set all values to predefined one */
void set_defaults()
{
raw_points.p1.x = 0;
raw_points.p1.y = 0;
raw_points.p2.x = 0;
raw_points.p2.y = 0;

small_points.p1.x = 0;
small_points.p1.y = 0;
small_points.p2.x = 0;
small_points.p2.y = 0;

large_points.p1.x = 0;
large_points.p1.y = 0;
large_points.p2.x = 0;
large_points.p2.y = 0;

imgsize.x = 0;
imgsize.y = 0;

}

/** Here we update the fields showing the active area */
void update_text_fields()
{
char val [LONGOPT], val2[LONGOPT], val3[LONGOPT], val4[LONGOPT], val5[LONGOPT];
char val6[LONGOPT];

  sprintf(val,"%ix%i+%i+%i", small_points.p2.x-small_points.p1.x,
    small_points.p2.y-small_points.p1.y, small_points.p1.x, small_points.p1.y);
  gtk_entry_set_text(GTK_ENTRY(actfield_s), val);

  sprintf(val2,"%ix%i+%i+%i", large_points.p2.x-large_points.p1.x,
    large_points.p2.y-large_points.p1.y, large_points.p1.x, large_points.p1.y);
  gtk_entry_set_text(GTK_ENTRY(actfield_l), val2);

  sprintf(val3,"%i / %i", small_points.p1.x, small_points.p1.y);
  gtk_entry_set_text(GTK_ENTRY(actfield_p1s), val3);

  sprintf(val4,"%i / %i", small_points.p2.x, small_points.p2.y);
  gtk_entry_set_text(GTK_ENTRY(actfield_p2s), val4);

  sprintf(val5,"%i / %i", large_points.p1.x, large_points.p1.y);
  gtk_entry_set_text(GTK_ENTRY(actfield_p1l), val5);

  sprintf(val6,"%i / %i", large_points.p2.x, large_points.p2.y);
  gtk_entry_set_text(GTK_ENTRY(actfield_p2l), val6);
}


/** Here we calculate the various active areas */
void calc_area ()
{
struct rect norm;
int i; /** counts if we have a problem in the calculation, if 0 it is ok */

imgsize.x = width;
imgsize.y = height;

/* First of all we check if we have valid values */
if ((raw_points.p1.x > imgsize.x) || (raw_points.p1.x < 0) ||
    (raw_points.p1.y > imgsize.y) || (raw_points.p1.y < 0) ||
    (raw_points.p2.x > imgsize.x) || (raw_points.p2.x < 0) ||
    (raw_points.p2.y > imgsize.y) || (raw_points.p2.y < 0)   )
  i++;
else 
  {
  /* First we order the points to one top left and the 2nd right bottom */
  if ( raw_points.p1.x > raw_points.p2.x)
    {
      norm.p1.x = raw_points.p2.x;
      norm.p2.x = raw_points.p1.x;
    }
  else 
    {
      norm.p1.x = raw_points.p1.x;
      norm.p2.x = raw_points.p2.x;
    }

  if ( raw_points.p1.y > raw_points.p2.y )
    {
      norm.p1.y = raw_points.p2.y;
      norm.p2.y = raw_points.p1.y;
    }
  else
    {
      norm.p1.y = raw_points.p1.y;
      norm.p2.y = raw_points.p2.y;
    }

  if (verbose)
    printf("Point 1 top-left %i/%i, Point 2 bottom-right %i/%i\n",
            norm.p1.x, norm.p1.y, norm.p2.x, norm.p2.y);

  small_points.p1.x = norm.p1.x;
  small_points.p1.y = norm.p1.y;
  small_points.p2.x = norm.p2.x;
  small_points.p2.y = norm.p2.y;

  if ((small_points.p1.x > 0) && ( (small_points.p1.x+8) < small_points.p2.x))
    while ((fmod( small_points.p1.x, 4)) != 0)
        small_points.p1.x++;
  else if (small_points.p1.x != 0)
    i++;
   
  if ((small_points.p1.y > 0) && ( (small_points.p1.y+8) < small_points.p2.y))
    while ((fmod( small_points.p1.y, 4)) != 0)
        small_points.p1.y++;
  else if (small_points.p1.y != 0)
    i++;
   
  if((small_points.p2.x < imgsize.x)&&((small_points.p1.x+4)<small_points.p2.x))
    while ((fmod( small_points.p2.x, 4)) != 0)
        small_points.p2.x--;
   
  if((small_points.p2.y < imgsize.y)&&((small_points.p1.y+4)<small_points.p2.y))
    while ((fmod( small_points.p2.y, 4)) != 0)
        small_points.p2.y--;

  large_points.p1.x = norm.p1.x;
  large_points.p1.y = norm.p1.y;
  large_points.p2.x = norm.p2.x;
  large_points.p2.y = norm.p2.y;

  if ( (large_points.p1.x >= 4) && ((large_points.p1.x+4) < large_points.p2.x) )
    while ((fmod( large_points.p1.x, 4)) != 0)
        large_points.p1.x--;
  else if (large_points.p1.x < 4)
        large_points.p1.x = 0;
  else 
    i++;
   
  if ( (large_points.p1.y >= 4) && ((large_points.p1.y+4) < large_points.p2.y) )
    while ((fmod( large_points.p1.y, 4)) != 0)
        large_points.p1.y--;
  else if (large_points.p1.y < 4)
        large_points.p1.y = 0;
  else 
    i++;

  if ( large_points.p2.x < (imgsize.x-4) )
    while ((fmod( large_points.p2.x, 4)) != 0)
        large_points.p2.x++;
  else if (large_points.p2.x > (imgsize.x-4) )
        large_points.p2.x = imgsize.x;
  else 
    i++;
   
  if ( large_points.p2.y < (imgsize.y-4) )
    while ((fmod( large_points.p2.y, 4)) != 0)
        large_points.p2.y++;
  else if (large_points.p2.y > (imgsize.y-4) )
        large_points.p2.y = imgsize.y;
  else 
    i++;

  update_text_fields();

  }   

}

/** Here we hand back the selected Active_size 
    @param widget The calling widget,
    @param data   dummy option                   */
void accept_changes(GtkWidget *widget, gpointer data)
{

 if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_s)) == TRUE )
   {
     sprintf(area_size, "%s", gtk_entry_get_text(GTK_ENTRY(actfield_s)));
     gtk_entry_set_text(GTK_ENTRY(combo_entry_scalerinput),area_size);
   }
 if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_l)) == TRUE )
   {
     sprintf(area_size, "%s", gtk_entry_get_text(GTK_ENTRY(actfield_l)));
     gtk_entry_set_text(GTK_ENTRY(combo_entry_scalerinput),area_size);
   }
 if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_m)) == TRUE )
   {
     sprintf(area_size, "%s", gtk_entry_get_text(GTK_ENTRY(actfield_m)));
     if (strlen(area_size) >= 7)
       gtk_entry_set_text(GTK_ENTRY(combo_entry_scalerinput),area_size);
   }
}

/** Here we create the OK and Cancel Button 
    @param hbox the box where we insert the two buttons 
    @param control_window on window we need to close */
void create_ok_cancel(GtkWidget *hbox, GtkWidget *control_window)
{
GtkWidget *button;

  button = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                  GTK_SIGNAL_FUNC (accept_changes), NULL);
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
      calc_area(); /* lets do some math */
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
void load_image (char filename[FILELEN])
{
  
  if (verbose)
    printf (" Rendering %s \n", filename);
  pixbuf1 = gdk_pixbuf_new_from_file(filename);
  width = gdk_pixbuf_get_width(pixbuf1);
  height = gdk_pixbuf_get_height(pixbuf1);

  if (verbose)
    printf(" Picture loaded, width %i, height %i \n", width, height);
}


void view_image()
{
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
  gtk_widget_set_sensitive(point1_entry, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), point1_entry, FALSE, FALSE, 0);
  gtk_widget_show(point1_entry);

  label = gtk_label_new("Point 2 (x/y) :");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  point2_entry = gtk_entry_new();
  gtk_widget_set_usize(point2_entry, 70, -2);
  gtk_widget_set_sensitive(point2_entry, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), point2_entry, FALSE, FALSE, 0);
  gtk_widget_show(point2_entry);

}

/** Here we create the boxes containing the active size strings */
void show_active_points (GtkWidget *hbox)
{
GtkWidget *table1, *label;
GSList *group;
int i, j;

  i = 0;
  j = 0; 

  table1 = gtk_table_new (5, 4, FALSE);

  label = gtk_label_new("Select calc size:");
  gtk_table_attach_defaults (GTK_TABLE(table1), label, i, i+2, j, j+1);
  gtk_widget_show(label);
  j++;

  label = gtk_label_new(" Smaler size");
  gtk_table_attach_defaults (GTK_TABLE(table1), label, i, i+1, j, j+1);
  gtk_widget_show(label);
  j++;

  label = gtk_label_new(" Larger size");
  gtk_table_attach_defaults (GTK_TABLE(table1), label, i, i+1, j, j+1);
  gtk_widget_show(label);
  j++;

  label = gtk_label_new(" Manual size");
  gtk_table_attach_defaults (GTK_TABLE(table1), label, i, i+1, j, j+1);
  gtk_widget_show(label);
  j++;

  i++;  /* 2nd colum */
  j= 1; /*starting in the second row */

  button_s = gtk_radio_button_new(NULL);
  gtk_table_attach_defaults (GTK_TABLE(table1), button_s, i, i+1, j, j+1);
  group = gtk_radio_button_group(GTK_RADIO_BUTTON(button_s)); 
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_s), TRUE);
  gtk_widget_show(button_s);
  j++;

  button_l = gtk_radio_button_new(group);
  gtk_table_attach_defaults (GTK_TABLE(table1), button_l, i, i+1, j, j+1);
  group = gtk_radio_button_group(GTK_RADIO_BUTTON(button_l)); 
  gtk_widget_show(button_l);
  j++;

  button_m = gtk_radio_button_new(group);
  gtk_table_attach_defaults (GTK_TABLE(table1), button_m, i, i+1, j, j+1);
  group = gtk_radio_button_group(GTK_RADIO_BUTTON(button_m)); 
  gtk_widget_show(button_m);

  j=0; /* going to the first line */
  i++; /* second second row */
  label = gtk_label_new("Active String");
  gtk_table_attach_defaults (GTK_TABLE(table1), label, i, i+1, j, j+1);
  gtk_widget_show(label);
  j++;

  actfield_s = gtk_entry_new();
  gtk_widget_set_usize(actfield_s, 120, -2);
  gtk_table_attach_defaults (GTK_TABLE(table1), actfield_s, i, i+1, j, j+1);
  gtk_widget_show(actfield_s);
  j++;

  actfield_l = gtk_entry_new();
  gtk_widget_set_usize(actfield_l, 120, -2);
  gtk_table_attach_defaults (GTK_TABLE(table1), actfield_l, i, i+1, j, j+1);
  gtk_widget_show(actfield_l);
  j++;

  actfield_m = gtk_entry_new();
  gtk_widget_set_usize(actfield_m, 120, -2);
  gtk_table_attach_defaults (GTK_TABLE(table1), actfield_m, i, i+1, j, j+1);
  gtk_widget_show(actfield_m);

  j=0; /* again the first line */
  i++; /* 3rd row */
  label = gtk_label_new("P1 x/y");
  gtk_table_attach_defaults (GTK_TABLE(table1), label, i, i+1, j, j+1);
  gtk_widget_show(label);
  j++;

  actfield_p1s = gtk_entry_new();
  gtk_widget_set_usize(actfield_p1s, 70, -2);
  gtk_table_attach_defaults (GTK_TABLE(table1), actfield_p1s, i, i+1, j, j+1);
  gtk_widget_show(actfield_p1s);
  j++;

  actfield_p1l = gtk_entry_new();
  gtk_widget_set_usize(actfield_p1l, 70, -2);
  gtk_table_attach_defaults (GTK_TABLE(table1), actfield_p1l, i, i+1, j, j+1);
  gtk_widget_show(actfield_p1l);

  j=0; /* again the first line */
  i++; /* 4th row */
  label = gtk_label_new("P2 x/y");
  gtk_table_attach_defaults (GTK_TABLE(table1), label, i, i+1, j, j+1);
  gtk_widget_show(label);
  j++;

  actfield_p2s = gtk_entry_new();
  gtk_widget_set_usize(actfield_p2s, 70, -2);
  gtk_table_attach_defaults (GTK_TABLE(table1), actfield_p2s, i, i+1, j, j+1);
  gtk_widget_show(actfield_p2s);
  j++;
 
  actfield_p2l = gtk_entry_new();
  gtk_widget_set_usize(actfield_p2l, 70, -2);
  gtk_table_attach_defaults (GTK_TABLE(table1), actfield_p2l, i, i+1, j, j+1);
  gtk_widget_show(actfield_p2l);
  j++;

  gtk_box_pack_start(GTK_BOX(hbox), table1, FALSE, FALSE, 0); 
  gtk_widget_show(table1);
}

/** Here we create the 1rst window with the controls */
void create_window_select(char filename[FILELEN])
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

  hbox = gtk_hbox_new (TRUE, 5);
  show_active_points (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox);

  hbox = gtk_hbox_new (TRUE, 10);  /* Creation of the OK an cancel Button */
  create_ok_cancel(hbox, control_window);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
  gtk_widget_show (hbox);

  gtk_container_add (GTK_CONTAINER (control_window), vbox);
  gtk_widget_show (vbox);
  gtk_widget_show (control_window); 

  load_image(filename);

  show_selection (filename);

}

/** Here we create the new window for the active size selection */
void show_selection(char filename[FILELEN])
{
GtkWidget *vbox, *hbox;
GdkPixbuf *pixbuf;

  gtk_widget_push_visual(gdk_rgb_get_visual(  ));
  gtk_widget_push_colormap(gdk_rgb_get_cmap(  ));
  picture_window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_widget_pop_visual(  );
  gtk_widget_pop_colormap(  );

  gtk_window_set_title(GTK_WINDOW (picture_window), "LVS - area select");

  vbox = gtk_vbox_new (FALSE,2);
  

  hbox = gtk_hbox_new (TRUE,5);
  /* Here we have a quick'n'dirty workaround. For only setting the window size
     to the size the image loaded has */
  gtk_widget_set_usize(GTK_WIDGET(picture_window), width, height);
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
