/* Linux Video Studio TV - a TV application
 * Copyright (C) 2001 Ronald Bultje
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gtktvplug.h"
#include "gtkfunctions.h"
#include "channels.h"

#include "stv_accept.xpm"
#include "stv_reject.xpm"
#include "stv_arrow_up.xpm"
#include "stv_arrow_down.xpm"

extern GtkWidget *tv;
extern int verbose;
static GtkWidget *chanlist_scrollbar = NULL, *chanlist_entry = NULL;


static void row_selected(GtkCList *clist, gint row, gint column, GdkEvent *event, gpointer data)
{
   char *ldata;

   gtk_clist_get_text(clist, row, 1, &ldata);
   gtk_adjustment_set_value(GTK_TVPLUG(tv)->frequency_adj, atoi(ldata));
   gtk_clist_get_text(clist, row, 0, &ldata);
   gtk_entry_set_text(GTK_ENTRY(chanlist_entry), ldata);
   if (channels)
   {
      int i;
      for (i=0;channels[i];i++)
      {
         if (!strcmp(channels[i]->name, ldata))
         {
            previous_channel = current_channel;
            current_channel = i;
            if (channelcombo)
               if (strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(channelcombo)->entry)), ldata))
                  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(channelcombo)->entry), ldata);
            break;
         }
      }
   }
}

static void save_data(GtkWidget *widget, GtkCList *clist)
{
   int i;
   char *data[2];
   char *ldata[2];
   char buf[256];

   if (!strcmp(gtk_entry_get_text(GTK_ENTRY(chanlist_entry)),""))
   {
      /* no name given */
      gtk_show_text_window(STUDIO_WARNING, "Please supply a channel name");
      return;
   }

   for (i=0;i<clist->rows;i++)
   {
      gtk_clist_get_text(clist, i, 0, &ldata[0]);
      gtk_clist_get_text(clist, i, 1, &ldata[1]);
      if (strcmp(ldata[0], gtk_entry_get_text(GTK_ENTRY(chanlist_entry))) == 0 ||
         atoi(ldata[1]) == GTK_TVPLUG(tv)->frequency_adj->value)
      {
         /* just change this */
         gtk_clist_set_text(clist, i, 0, gtk_entry_get_text(GTK_ENTRY(chanlist_entry)));
         sprintf(buf, "%d", (int)GTK_TVPLUG(tv)->frequency_adj->value);
         gtk_clist_set_text(clist, i, 1, buf);
         return;
      }
   }

   data[0] = gtk_entry_get_text(GTK_ENTRY(chanlist_entry));
   sprintf(buf, "%.1f", GTK_TVPLUG(tv)->frequency_adj->value);
   data[1] = buf;
   gtk_clist_append(clist, data);
}

static void del_data(GtkWidget *widget, GtkCList *clist)
{
   int i;
   char *data;

   for (i=0;i<clist->rows;i++)
   {
      gtk_clist_get_text(clist, i, 0, &data);
      if (!strcmp(data, gtk_entry_get_text(GTK_ENTRY(chanlist_entry))))
      {
         gtk_clist_remove(clist, i);
         return;
      }
   }

   /* if we get here, it's not good */
   gtk_show_text_window(STUDIO_WARNING, "Please select a channel to delete");
}

static void clist_value_changed(GtkAdjustment *adj, gpointer data)
{
   GtkCList *clist = GTK_CLIST(data);
   gtk_clist_moveto(clist, adj->value, 0, 0.5, 0.0);
}

static void channel_editor_exposed(GtkWidget *channellist, GdkEvent *event, gpointer data)
{
   if (channels)
   {
      GtkWidget *slider = GTK_WIDGET(data);
      GtkAdjustment *new;
      new = GTK_ADJUSTMENT(gtk_adjustment_new(0,0,GTK_CLIST(channellist)->rows,1,3,
         GTK_CLIST(channellist)->row_height/channellist->allocation.height));
      gtk_signal_connect(GTK_OBJECT(new), "value_changed",
         GTK_SIGNAL_FUNC(clist_value_changed), (gpointer) channellist);
      gtk_clist_set_vadjustment(GTK_CLIST(channellist), new);
      gtk_range_set_adjustment(GTK_RANGE(slider), new);
   }
}

static void move_data_down(GtkWidget *w, gpointer data2)
{
   int i;
   GtkCList *clist = GTK_CLIST(data2);
   char *data[4];

   for (i=0;i<clist->rows;i++)
   {
      gtk_clist_get_text(clist, i, 0, &data[0]);
      if (!strcmp(gtk_entry_get_text(GTK_ENTRY(chanlist_entry)), data[0]))
      {
         if (i+1<clist->rows)
         {
            gtk_clist_swap_rows(clist, i, i+1);
            gtk_clist_select_row(clist, i+1, 0);
         }
         else
            gtk_show_text_window(STUDIO_WARNING, "Channel is already down there");
         return;
      }
   }

   /* if we get here, it's not good */
   gtk_show_text_window(STUDIO_WARNING, "Please select a channel to move");
}

static void move_data_up(GtkWidget *w, gpointer data2)
{
   int i;
   GtkCList *clist = GTK_CLIST(data2);
   char *data[4];

   for (i=0;i<clist->rows;i++)
   {
      gtk_clist_get_text(clist, i, 0, &data[0]);
      if (!strcmp(gtk_entry_get_text(GTK_ENTRY(chanlist_entry)), data[0]))
      {
         if (i>0)
         {
            gtk_clist_swap_rows(clist, i-1, i);
            gtk_clist_select_row(clist, i-1, 0);
         }
         else
            gtk_show_text_window(STUDIO_WARNING, "Channel is already on top");
         return;
      }
   }

   /* if we get here, it's not good */
   gtk_show_text_window(STUDIO_WARNING, "Please select a channel to move");
}

static void freq_slider_changed(GtkAdjustment *adj, char *what)
{
    gtk_tvplug_set(tv, what, adj->value);
}

GtkWidget *get_channel_list_notebook_page(GtkWidget *channellist)
{
   GtkWidget *vbox, *hbox, *button, *vboxmain, *hboxmain;
   GtkTooltips *tooltip;
   char frequency[256];
   char *list[2];

   tooltip = gtk_tooltips_new();
   vboxmain = gtk_vbox_new(FALSE, 0);
   hboxmain = gtk_hbox_new(FALSE, 0);
   vbox = gtk_vbox_new(FALSE, 20);
   hbox = gtk_hbox_new(FALSE, 0);

   list[0] = "Channel Name"; list[1] = "Frequency";
   gtk_signal_connect(GTK_OBJECT(channellist), "select_row",
         GTK_SIGNAL_FUNC(row_selected), NULL);
   gtk_box_pack_start(GTK_BOX (hbox), channellist, TRUE, TRUE, 0);
   gtk_clist_set_selection_mode(GTK_CLIST(channellist), GTK_SELECTION_SINGLE);
   if (channels)
   {
      int i;
      GtkWidget *slider;
      slider = gtk_vscrollbar_new(NULL);
      gtk_box_pack_start(GTK_BOX (hbox), slider, TRUE, TRUE, 0);
      gtk_widget_show(slider);
      for(i=0;channels[i];i++)
      {
         sprintf(frequency, "%d", channels[i]->frequency);
         list[0] = channels[i]->name; list[1] = frequency;
         gtk_clist_append(GTK_CLIST(channellist), list);
      }
      gtk_signal_connect(GTK_OBJECT(channellist), "expose_event",
         GTK_SIGNAL_FUNC(channel_editor_exposed), (gpointer) slider);
   }
   gtk_widget_show(channellist);
   gtk_widget_set_usize(channellist, -1, 150);
   gtk_box_pack_start(GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   gtk_widget_show(hbox);

   gtk_signal_connect(GTK_OBJECT(GTK_TVPLUG(tv)->frequency_adj), "value_changed",
      GTK_SIGNAL_FUNC(freq_slider_changed), "frequency");
   gtk_object_ref(GTK_OBJECT(GTK_TVPLUG(tv)->frequency_adj));
   chanlist_scrollbar = gtk_hscale_new(GTK_TVPLUG(tv)->frequency_adj);
   gtk_scale_set_draw_value(GTK_SCALE(chanlist_scrollbar), 1);
   gtk_scale_set_value_pos(GTK_SCALE(chanlist_scrollbar), GTK_POS_BOTTOM);
   gtk_box_pack_start(GTK_BOX (vbox), chanlist_scrollbar, TRUE, TRUE, 0);
   gtk_widget_show(chanlist_scrollbar);
   gtk_widget_set_usize(chanlist_scrollbar, 150, -1);
   gtk_tooltips_set_tip(tooltip, chanlist_scrollbar, "Channel Frequency", NULL);

   chanlist_entry = gtk_entry_new();
   gtk_box_pack_start(GTK_BOX (vbox), chanlist_entry, TRUE, FALSE, 0);
   gtk_widget_show(chanlist_entry);

   hbox = gtk_hbox_new(FALSE, 5);
   button = gtk_image_label_button("Add",
			"Add Current Entry to Channel List",
			stv_accept_xpm, 5, GTK_POS_RIGHT);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(save_data), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   button = gtk_image_label_button("Del",
			"Remove Current Entry from Channel List",
			stv_reject_xpm, 5, GTK_POS_RIGHT);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(del_data), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   button = gtk_image_label_button("Up",
			"Push Current Entry One Position Up",
			stv_arrow_up_xpm, 5, GTK_POS_RIGHT);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(move_data_up), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   button = gtk_image_label_button("Down",
			"Pull Current Entry One Position Down",
			stv_arrow_down_xpm, 5, GTK_POS_RIGHT);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(move_data_down), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   gtk_box_pack_start(GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
   gtk_widget_show(hbox);

   gtk_box_pack_start(GTK_BOX (hboxmain), vbox, FALSE, FALSE, 10);
   gtk_widget_show(vbox);
   gtk_box_pack_start(GTK_BOX (vboxmain), hboxmain, FALSE, FALSE, 10);
   gtk_widget_show(hboxmain);

   return vboxmain;
}

void increase_channel(int num)
{
   int i=0, old_cur;

   if (!channels)
      return;

   old_cur = current_channel;
   current_channel += num;

   if (current_channel < 0)
      current_channel = 0;
   for(i=0;channels[i];i++);
   if (current_channel>=i)
      current_channel = i-1;

   if (current_channel != old_cur)
   {
      previous_channel = old_cur;
      if (verbose) g_print("Changing channel to \'%s\' (frequency: %d)\n",
         channels[current_channel]->name, channels[current_channel]->frequency);
      gtk_tvplug_set(tv, "frequency", channels[current_channel]->frequency);
      if (channelcombo)
      {
         if (strcmp(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(channelcombo)->entry)),
            channels[current_channel]->name))
         {
            gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(channelcombo)->entry),
               channels[current_channel]->name);
         }
      }
   }
}

void goto_channel(int num)
{
   if (num < 0) return;
   increase_channel(num-current_channel);
}

void goto_previous_channel()
{
   if (previous_channel < 0) return;
   goto_channel(previous_channel);
}

static gint remove_timeout(gpointer data)
{
   gint *num = (gint *) data;
   *num = -1;
   return FALSE;
}

void set_timeout_goto_channel(int num)
{
   static int chan_num = -1;

   if (chan_num != -1)
   {
      chan_num = num + chan_num*10;
      goto_channel(chan_num-1);
   }
   else
   {
      chan_num = num;
      goto_channel(chan_num-1);
      gtk_timeout_add(1000, (GtkFunction) remove_timeout, (gpointer)(&chan_num));
   }
}
