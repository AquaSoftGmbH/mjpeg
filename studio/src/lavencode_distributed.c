/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * lavencode_mpeg done by Bernhard Praschinger
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
 * Here is the layout for the mpeg encoding options is created.
 * It is-will be reused tor all mpeg versions. 
 * also mpeg1, mpeg2, vcd, svcd, in the future maybe: DVD 
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

#include "studio.h"

/* Forward declaration */
void open_distributed_window(GtkWidget *widget, gpointer data);
void create_machine(GtkWidget *hbox);

/* Here we create the layout for the machine data */
void create_machine(GtkWidget *hbox)
{
GtkWidget *label, *table; 
int tx, ty; /* table size x, y */

tx=3;
ty=2;

  table = gtk_table_new (tx,ty, FALSE);

  tx=0;
  ty=0;

  label = gtk_label_new (" Machine name : ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

}

/* open a new window with all the options in it */
void open_distributed_window(GtkWidget *widget, gpointer data)
{
GtkWidget *distribute_window, *button;
GtkWidget *vbox, *hbox; 

  distribute_window = gtk_window_new(GTK_WINDOW_DIALOG);
  hbox = gtk_hbox_new (FALSE, 10);
 
  vbox = gtk_vbox_new (FALSE, 10);

  create_machine(hbox);
 
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox); 

//  create_audio (hbox);

//  create_video (hbox);

  /* Show the OK and cancel Button */

  hbox = gtk_hbox_new(TRUE, 20);

  button = gtk_button_new_with_label("OK");
//  gtk_signal_connect(GTK_OBJECT(button), "clicked",
//                     GTK_SIGNAL_FUNC (accept_options), (gpointer) "test");
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                            gtk_widget_destroy, GTK_OBJECT(distribute_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
          gtk_widget_destroy, GTK_OBJECT(distribute_window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
  gtk_widget_show(button);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  gtk_container_add (GTK_CONTAINER (distribute_window), vbox);
  gtk_widget_show(vbox);

  gtk_widget_show(distribute_window);

printf("\n Now in lavenecode_distributed X-) \n");

}
