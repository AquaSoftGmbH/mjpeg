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

/* TODO:
- auto-channel detection
- only allow window resizing at pixel "blocks" (like xawtv)
- fullscreen
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <getopt.h>
#include <sys/stat.h>
#include <errno.h>
#include "gtktvplug.h"
#include "gtkfunctions.h"
#include "parseconfig.h"

#include "slider_hue.xpm"
#include "slider_contrast.xpm"
#include "slider_sat_colour.xpm"
#include "slider_brightness.xpm"

typedef struct {
   int  frequency;        /* channel frequency         */
   char name[256];        /* name given to the channel */
} Channel;

typedef struct {
   GtkWidget *entry;
   GtkObject *adj;
   GtkWidget *scrollbar;
} EntryAndSlider;

static GtkWidget *tv = NULL, *window = NULL, *channelcombo = NULL;
static EntryAndSlider entry_and_slider;
static gboolean verbose = FALSE;
static gboolean full_screen = FALSE;
static Channel **channels = NULL; /* make it NULL-terminated PLEASE! */
static gint current_channel=-1, port=0;
static char *tv_config_file = "studio";

/* Found this one in bluefish (http://bluefish.openoffice.org/) */
#define DIR_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)	/* same as 0755 */
static int chk_dir(char *name)
{
   struct stat filestat;

   if ((stat(name, &filestat) == -1) && (errno == ENOENT)) {
      if (verbose) printf("chk_dir, %s does not exist, creating\n", name);
      if (mkdir(name, DIR_MODE) != 0) {
         printf("ERROR: Unable to create directory config directory %s\n", name);
         return 0;
      };
   };
   return 1;
}

static void load_options(int *width, int *height)
{
   char *val;
   int  i, num=0, tot;
   char filename[256];
   char value_get[256];

   sprintf(filename, "%s/%s", getenv("HOME"), ".studio");
   chk_dir(filename);
   sprintf(filename, "%s/%s/%s.tv-conf",getenv("HOME"),".studio", tv_config_file);
   cfg_parse_file(filename);

   port = cfg_get_int("StudioTV", "default_port");
   *width = cfg_get_int("StudioTV", "default_width");
   *height = cfg_get_int("StudioTV", "default_height");

   if ((tot = cfg_get_int("StudioTV", "num_chans")) < 1)
      return;

   if (channels)
   {
      for (i=0;channels[i];i++)
         free(channels[i]);
      free(channels);
   }

   channels = (Channel**)malloc(sizeof(Channel*)*(tot+1));

   for(num=0;num<tot;num++)
   {
      sprintf(value_get, "channel_frequency_%d", num);
      i = cfg_get_int("StudioTV",value_get);
      sprintf(value_get, "channel_name_%d", num);
      val = cfg_get_str("StudioTV",value_get);
      channels[num] = (Channel*)malloc(sizeof(Channel));
      channels[num]->frequency = i;
      sprintf(channels[num]->name, val);
   }

   channels[tot] = NULL;
}

static void save_options()
{
   char filename[256];
   FILE *fd;
   int i, numchans;

   sprintf(filename, "%s/%s", getenv("HOME"), ".studio");
   chk_dir(filename);
   sprintf(filename, "%s/%s/%s.tv-conf",getenv("HOME"),".studio", tv_config_file);
   fd = fopen(filename, "w");
   if (!fd)
   {
      printf("WARNING: cannot open config file: %s\n", filename);
      return;
   }

   fprintf(fd, "[StudioTV]\n");
   fprintf(fd, "default_port = %d\n", port);
   fprintf(fd, "default_width = %d\n", tv->allocation.width);
   fprintf(fd, "default_height = %d\n", tv->allocation.height);

   if (channels)
   {
      numchans = 0;
      for (i=0;channels[i];i++)
      {
         fprintf(fd, "channel_frequency_%d = %d\n", i, channels[i]->frequency);
         fprintf(fd, "channel_name_%d = %s\n", i, channels[i]->name);
         numchans = i;
      }
      fprintf(fd, "num_chans = %d\n", numchans);
   }

   fclose(fd);
}

static gint delete_event( GtkWidget *widget, GdkEvent *event, gpointer data )
{
   save_options();
   g_print ("Thanks for using Linux Video Studio TV\n");
   return(FALSE);
}

static void destroy( GtkWidget *widget, gpointer data )
{
   if (verbose) g_print ("Quitting...\n");
   gtk_main_quit();
}

static void version()
{
   g_print("Linux Video Studio TV version " VERSION "\n");
   exit(1);
}

static void usage()
{
   g_print("Linux Video Studio TV - a simple yet functional TV app\n");
   g_print("Usage: \'stv [options]\' - where options are:\n");
   g_print("  -h     : this information\n");
   g_print("  -t     : probe Xvideo ports for video cards\n");
   g_print("  -p num : Xvideo port number to use (see -t)\n");
   g_print("  -v     : version information\n");
   g_print("  -d     : debug/verbose mode\n");
   g_print("  -c name: config name (default: \'studio\')\n");
   g_print("\n");
   g_print("(c) copyright Ronald Bultje, 2001 - under the terms of the GPL\n");
   exit(1);
}

static void toggle_fullscreen()
{
   if (verbose) g_print("Going to %s mode\n",
      full_screen?"windowed":"full-screen");

   full_screen = full_screen?FALSE:TRUE;

   /* TODO */
}

static void video_slider_changed(GtkAdjustment *adj, char *what)
{
    gtk_tvplug_set(tv, what, adj->value);
}

static GtkWidget *get_video_sliders_widget()
{
   GtkWidget *hbox, *vbox, *pixmap, *scrollbar;
   GtkTooltips *tooltip;
   GtkObject *adj;
   int i=0;
   char *titles[4] = {
      "Hue",
      "Contrast",
      "Brightness",
      "Colour Saturation"
   };
   char *names[4] = {
      "hue",
      "contrast",
      "brightness",
      "colour"
   };
   int values[12] = {
      GTK_TVPLUG(tv)->hue,
      GTK_TVPLUG(tv)->contrast,
      GTK_TVPLUG(tv)->brightness,
      GTK_TVPLUG(tv)->saturation,
      GTK_TVPLUG(tv)->hue_min,
      GTK_TVPLUG(tv)->contrast_min,
      GTK_TVPLUG(tv)->brightness_min,
      GTK_TVPLUG(tv)->saturation_min,
      GTK_TVPLUG(tv)->hue_max,
      GTK_TVPLUG(tv)->contrast_max,
      GTK_TVPLUG(tv)->brightness_max,
      GTK_TVPLUG(tv)->saturation_max,
   };
   char **pixmaps[4] = {
      slider_hue_xpm,
      slider_contrast_xpm,
      slider_brightness_xpm,
      slider_sat_colour_xpm,
   };

   tooltip = gtk_tooltips_new();
   hbox = gtk_hbox_new(TRUE, 20);

   for (i=0;i<4;i++)
   {
      vbox = gtk_vbox_new (FALSE, 0);
      adj = gtk_adjustment_new(values[i], values[i+4], values[i+8],
         1, (values[i+8]-values[i+4])/10, 0);
      gtk_signal_connect(adj, "value_changed",
         GTK_SIGNAL_FUNC(video_slider_changed), names[i]);
      scrollbar = gtk_vscale_new(GTK_ADJUSTMENT (adj));
      gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 0);
      gtk_box_pack_start(GTK_BOX (vbox), scrollbar, TRUE, TRUE, 10);
      gtk_widget_show(scrollbar);
      gtk_widget_set_usize(scrollbar, -1, 150);
      gtk_tooltips_set_tip(tooltip, scrollbar, titles[i], NULL);
      pixmap = gtk_widget_from_xpm_data(pixmaps[i]);
      gtk_widget_show(pixmap);
      gtk_box_pack_start(GTK_BOX(vbox), pixmap, FALSE, FALSE, 10);
      gtk_box_pack_start(GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
      gtk_widget_show(vbox);
   }

   return hbox;
}

static void row_selected(GtkCList *clist, gint row, gint column, GdkEvent *event, gpointer data)
{
   char *ldata;

   gtk_clist_get_text(clist, row, 0, &ldata);
   gtk_entry_set_text(GTK_ENTRY(entry_and_slider.entry), ldata);
   gtk_clist_get_text(clist, row, 1, &ldata);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(entry_and_slider.adj), atoi(ldata));
}

static void save_data(GtkWidget *widget, GtkCList *clist)
{
   int i;
   char *data[2];
   char *ldata[2];
   char buf[256];

   for (i=0;i<clist->rows;i++)
   {
      gtk_clist_get_text(clist, i, 0, &ldata[0]);
      gtk_clist_get_text(clist, i, 1, &ldata[1]);
      if (strcmp(ldata[0], gtk_entry_get_text(GTK_ENTRY(entry_and_slider.entry))) == 0 ||
         atoi(ldata[1]) == GTK_ADJUSTMENT(entry_and_slider.adj)->value)
      {
         /* just change this */
         gtk_clist_set_text(clist, i, 0, gtk_entry_get_text(GTK_ENTRY(entry_and_slider.entry)));
         sprintf(buf, "%d", (int)GTK_ADJUSTMENT(entry_and_slider.adj)->value);
         gtk_clist_set_text(clist, i, 1, buf);
         return;
      }
   }

   data[0] = gtk_entry_get_text(GTK_ENTRY(entry_and_slider.entry));
   sprintf(buf, "%d", (int)GTK_ADJUSTMENT(entry_and_slider.adj)->value);
   data[1] = buf;
   gtk_clist_append(clist, data);
}

static void del_data(GtkWidget *widget, GtkCList *clist)
{
   if ((gint)clist->selection->data >= 0)
      gtk_clist_remove(clist, (gint)clist->selection->data);
}

static void save_list(GtkWidget *widget, GtkCList *clist)
{
   int i;
   char *data[2];
   GList *channellist = NULL;

   if (channels)
   {
      for(i=0;channels[i];i++)
         free(channels[i]);
      free(channels);
   }

   if (clist->rows)
      channels = (Channel**)malloc(sizeof(Channel*)*(clist->rows+1));
   for (i=0;i<clist->rows;i++)
   {
      channels[i] = (Channel*)malloc(sizeof(Channel));
      gtk_clist_get_text(clist, i, 0, &data[0]);
      gtk_clist_get_text(clist, i, 1, &data[1]);
      sprintf(channels[i]->name, data[0]);
      channels[i]->frequency = atoi(data[1]);
   }
   if (clist->rows)
      channels[clist->rows] = NULL;

   if (channels)
   {
      for (i=0;channels[i];i++)
         channellist = g_list_append(channellist, channels[i]->name);
   }
   if (channellist)
      gtk_combo_set_popdown_strings (GTK_COMBO (channelcombo), channellist);
   save_options();
}

static void show_channel_editor(GtkWidget *widget, gpointer data)
{
   GtkWidget *window, *vbox, *hbox, *channellist, *button;
   GtkTooltips *tooltip;
   char frequency[256];
   char *list[2];

   if (verbose) g_print("Showing channel editor\n");

   window = gtk_window_new(GTK_WINDOW_DIALOG);

   gtk_window_set_title (GTK_WINDOW(window), "Linux Video Studio TV - Channel Editor");
   gtk_container_set_border_width (GTK_CONTAINER (window), 15);

   tooltip = gtk_tooltips_new();
   vbox = gtk_vbox_new(FALSE, 20);

   list[0] = "Channel Name"; list[1] = "Frequency";
   channellist = gtk_clist_new_with_titles(2, list);
   gtk_clist_set_selection_mode(GTK_CLIST(channellist), GTK_SELECTION_SINGLE);
   if (channels)
   {
      int i;
      for(i=0;channels[i];i++)
      {
         sprintf(frequency, "%d", channels[i]->frequency);
         list[0] = channels[i]->name; list[1] = frequency;
         gtk_clist_append(GTK_CLIST(channellist), list);
      }
   }
   gtk_signal_connect(GTK_OBJECT(channellist), "select_row",
         GTK_SIGNAL_FUNC(row_selected), NULL);
   gtk_box_pack_start(GTK_BOX (vbox), channellist, FALSE, FALSE, 0);
   gtk_widget_show(channellist);
   gtk_widget_set_usize(channellist, -1, 150);

   entry_and_slider.adj = gtk_adjustment_new(GTK_TVPLUG(tv)->frequency,
      GTK_TVPLUG(tv)->frequency_min, GTK_TVPLUG(tv)->frequency_max,
      1, (GTK_TVPLUG(tv)->frequency_max-GTK_TVPLUG(tv)->frequency_min)/10, 0);
   gtk_signal_connect(entry_and_slider.adj, "value_changed",
      GTK_SIGNAL_FUNC(video_slider_changed), "frequency");
   entry_and_slider.scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (entry_and_slider.adj));
   gtk_scale_set_draw_value(GTK_SCALE(entry_and_slider.scrollbar), 0);
   gtk_box_pack_start(GTK_BOX (vbox), entry_and_slider.scrollbar, TRUE, TRUE, 10);
   gtk_widget_show(entry_and_slider.scrollbar);
   gtk_widget_set_usize(entry_and_slider.scrollbar, 150, -1);
   gtk_tooltips_set_tip(tooltip, entry_and_slider.scrollbar, "Channel Frequency", NULL);

   entry_and_slider.entry = gtk_entry_new();
   gtk_box_pack_start(GTK_BOX (vbox), entry_and_slider.entry, TRUE, FALSE, 0);
   gtk_widget_show(entry_and_slider.entry);

   hbox = gtk_hbox_new(TRUE, 20);
   button = gtk_button_new_with_label("Save");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(save_data), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, TRUE, TRUE, 10);
   gtk_widget_show(button);
   button = gtk_button_new_with_label("Delete");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(del_data), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, TRUE, TRUE, 10);
   gtk_widget_show(button);
   button = gtk_button_new_with_label("OK");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(save_list), GTK_CLIST(channellist));
   gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
      GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT (window));
   gtk_box_pack_start(GTK_BOX (hbox), button, TRUE, TRUE, 10);
   gtk_widget_show(button);
   gtk_box_pack_start(GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
   gtk_widget_show(hbox);

   gtk_container_add (GTK_CONTAINER (window), vbox);
   gtk_widget_show(vbox);

   gtk_widget_show(window);
}

static void channel_changed(GtkWidget *widget, gpointer data)
{
   int i;
   int frequency = -1;

   if (!channels)
   {
      g_print("WARNING: no channels set yet\n");
      return;
   }

   for (i=0;channels[i];i++)
   {
      if (strcmp(gtk_entry_get_text(GTK_ENTRY(widget)), channels[i]->name) == 0)
      {
         frequency = channels[i]->frequency;
         current_channel = i;
         break;
      }
   }

   if (frequency == -1)
   {
      g_print("WARNING: unknown channel\n");
      return;
   }

   if (verbose) g_print("Changing channel to %s (frequency: %d)\n",
      gtk_entry_get_text(GTK_ENTRY(widget)), frequency);

   gtk_tvplug_set(tv, "frequency", frequency);
}

static GtkWidget *get_video_channels_widget()
{
   GtkWidget *vbox, *button;
   GList *channellist = NULL;

   vbox = gtk_vbox_new(FALSE, 20);

   if (channels)
   {
      int i;
      for (i=0;channels[i];i++)
         channellist = g_list_append(channellist, channels[i]->name);
   }

   channelcombo = gtk_combo_new();
   if (channellist)
      gtk_combo_set_popdown_strings (GTK_COMBO (channelcombo), channellist);
   gtk_signal_connect(GTK_OBJECT(GTK_COMBO(channelcombo)->entry), "changed",
      GTK_SIGNAL_FUNC (channel_changed), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), channelcombo, TRUE, FALSE, 0);
   gtk_widget_show(channelcombo);

   button = gtk_button_new_with_label("Channel Editor");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC (show_channel_editor), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, FALSE, 0);
   gtk_widget_show(button);

   return vbox;
}

static void show_options_window()
{
   GtkWidget *window, *box, *vbox;

   if (verbose) g_print("Showing options window\n");

   window = gtk_window_new(GTK_WINDOW_DIALOG);

   gtk_window_set_title (GTK_WINDOW(window), "Linux Video Studio TV - Options");
   gtk_container_set_border_width (GTK_CONTAINER (window), 15);

   vbox = gtk_vbox_new(FALSE, 20);

   box = get_video_sliders_widget();
   gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, FALSE, 0);
   gtk_widget_show(box);

   box = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, FALSE, 0);
   gtk_widget_show(box);

   box = get_video_channels_widget();
   gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, FALSE, 0);
   gtk_widget_show(box);

   gtk_container_add (GTK_CONTAINER (window), vbox);
   gtk_widget_show(vbox);

   gtk_widget_show(window);
}

static void increase_channel(int num)
{
   if (!channels)
      return;

   current_channel += num;

   if (current_channel < 0)
   {
      current_channel = 0;
      return;
   }
   if (!channels[current_channel])
   {
      current_channel--;
      return;
   }

   if (verbose) g_print("Changing channel to %s (frequency: %d)\n",
      channels[current_channel]->name, channels[current_channel]->frequency);

   gtk_tvplug_set(tv, "frequency", channels[current_channel]->frequency);
}

static void tv_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
   switch (event->button)
   {
      case 1:
         toggle_fullscreen();
         break;
      case 3:
         show_options_window();
         break;
      case 4:
         increase_channel(-1);
         break;
      case 5:
         increase_channel(1);
         break;
   }
}

static int parse_command_line_options(int *argc, char **argv[], int *prt)
{
   int n;

   while((n=getopt(*argc,*argv,"htvdp:")) != EOF)
   {
      switch (n)
      {
         case 'p':
            *prt = atoi(optarg);
            break;
         case 'h':
            usage();
            break;
         case 't':
            gtk_tvplug_new(-1);
            exit(1);
            break;
         case 'v':
            version();
            break;
         case 'd':
            verbose = TRUE;
            break;
         case 'c':
            tv_config_file = optarg;
            break;
      }
   }

   return 1;
}

int main(int argc, char *argv[])
{
   int p=0, w=0, h=0;;

   gtk_init(&argc, &argv);
   if (!parse_command_line_options(&argc, &argv, &p)) return 1;

   load_options(&w, &h);
   if (p) port = p;

   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_signal_connect (GTK_OBJECT (window), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), NULL);
   gtk_signal_connect (GTK_OBJECT (window), "destroy",
      GTK_SIGNAL_FUNC (destroy), NULL);

   gtk_window_set_title (GTK_WINDOW (window), "Linux Video Studio TV");
   gtk_container_set_border_width (GTK_CONTAINER (window), 0);
   tv = gtk_tvplug_new(port);
   if (!tv)
   {
      g_print("ERROR: no suitable video4linux device found\n");
      g_print("Please supply a video4linux Xv-port manually (\'stv -p <num\')\n");
      return 1;
   }
   if (!port) port = GTK_TVPLUG(tv)->port;
   gtk_signal_connect(GTK_OBJECT(tv), "button_press_event",
      GTK_SIGNAL_FUNC(tv_clicked), NULL);
   gtk_container_add (GTK_CONTAINER (window), tv);
   gtk_widget_show(tv);
   if (w>0 && h>0) gtk_widget_set_usize(tv, w, h);
   printf("W: %d, H: %d\n", w,h);

   gtk_widget_show(window);
   gtk_main();

   return 0;
}
