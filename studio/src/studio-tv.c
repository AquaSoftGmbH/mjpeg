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
#include <config.h>
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include "gtktvplug.h"
#include "gtkfunctions.h"
#include "parseconfig.h"
#if defined(HAVE_PNG) || defined(HAVE_JPEG)
#include "gdkpixbuf_write.h"
#endif
#ifdef OSS
#include "mixer.h"
#endif

#include "slider_hue.xpm"
#include "slider_contrast.xpm"
#include "slider_sat_colour.xpm"
#include "slider_brightness.xpm"
#include "slider_volume.xpm"
#include "stv_accept.xpm"
#include "stv_reject.xpm"
#include "stv_arrow_up.xpm"
#include "stv_arrow_down.xpm"

typedef struct {
   int  frequency;        /* channel frequency         */
   char name[256];        /* name given to the channel */
} Channel;

#ifdef HAVE_LIRC
enum{
   RC_0,
   RC_1,
   RC_2,
   RC_3,
   RC_4,
   RC_5,
   RC_6,
   RC_7,
   RC_8,
   RC_9,
   RC_MUTE,
   RC_CHAN_UP,
   RC_CHAN_DOWN,
   RC_FULLSCREEN,
   RC_QUIT,
   RC_SCREENSHOT,
   RC_VOLUME_UP,
   RC_VOLUME_DOWN,
   RC_PREVIOUS_CHAN,
   RC_NUM_KEYS
};
static char *rc_names[] = {
   "0  ",
   "1  ",
   "2  ",
   "3  ",
   "4  ",
   "5  ",
   "6  ",
   "7  ",
   "8  ",
   "9  ",
   "Audio mute  ",
   "Channel up  ",
   "Channel down  ",
   "Fullscreen  ",
   "Quit  ",
   "Screenshot  ",
   "Volume up  ",
   "Volume down  ",
   "Last channel  "
};
#endif

#define min(a,b) a>b?b:a

GtkWidget *window = NULL;
static gboolean verbose = FALSE;
static GtkWidget *tv = NULL, *channelcombo = NULL;
static GtkWidget *chanlist_scrollbar = NULL, *chanlist_entry = NULL;
static Channel **channels = NULL; /* make it NULL-terminated PLEASE! */
static gint current_channel=-1, previous_channel=-1, port=0;
static char *tv_config_file = "studio";
#ifdef OSS
static char *mixer_dev = NULL;
static GtkAdjustment *adj_audio = NULL;
static gint mixer_id, volume;
#endif
#ifdef HAVE_LIRC
static char *lirc_dev = NULL;
static char *remote_buttons[RC_NUM_KEYS];
static GtkWidget *rc_entry[RC_NUM_KEYS];
static GtkWidget *focussed_entry = NULL, *focussed_label = NULL;
#endif

/* Found this one in bluefish (http://bluefish.openoffice.nl/) */
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

static void load_options(int *width, int *height, int *x, int *y)
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
   if (width) *width = cfg_get_int("StudioTV", "default_width");
   if (height) *height = cfg_get_int("StudioTV", "default_height");
   if (x) *x = cfg_get_int("StudioTV", "default_x");
   if (y) *y = cfg_get_int("StudioTV", "default_y");

#ifdef OSS
   if (!mixer_dev) mixer_dev = cfg_get_str("StudioTV", "default_mixer_dev");
   if (!mixer_dev) mixer_dev = "/dev/mixer";
#endif

#ifdef HAVE_LIRC
   if (!lirc_dev) lirc_dev = cfg_get_str("StudioTV", "default_lirc_dev");
   if (!lirc_dev) lirc_dev = "/dev/lircd";
   for(num=0;num<RC_NUM_KEYS;num++)
   {
      sprintf(value_get, "remote_control_key_%d", num);
      remote_buttons[num] = cfg_get_str("StudioTV",value_get);
      if (!remote_buttons[num]) remote_buttons[num] = "none";
   }
#endif

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

   if (verbose) g_print("Configuration loaded from %s\n", filename);
}

static void save_options()
{
   char filename[256];
   FILE *fd;
   int i, numchans;
   int x,y,w,h;

   sprintf(filename, "%s/%s", getenv("HOME"), ".studio");
   chk_dir(filename);
   sprintf(filename, "%s/%s/%s.tv-conf",getenv("HOME"),".studio", tv_config_file);
   fd = fopen(filename, "w");
   if (!fd)
   {
      printf("WARNING: cannot open config file: %s\n", filename);
      return;
   }

   gdk_window_get_size(window->window, &w, &h);
   gdk_window_get_origin(window->window, &x, &y);
   fprintf(fd, "[StudioTV]\n");
   fprintf(fd, "default_port = %d\n", port);
   fprintf(fd, "default_width = %d\n", w);
   fprintf(fd, "default_height = %d\n", h);
   fprintf(fd, "default_x = %d\n", x);
   fprintf(fd, "default_y = %d\n", y);

#ifdef OSS
   fprintf(fd, "default_mixer_dev = %s\n", mixer_dev);
#endif

#ifdef HAVE_LIRC
   fprintf(fd, "default_lirc_dev = %s\n", lirc_dev);
   for(i=0;i<RC_NUM_KEYS;i++)
      fprintf(fd, "remote_control_key_%d = %s\n",
         i, remote_buttons[i]);
#endif

   if (channels)
   {
      numchans = 0;
      for (i=0;channels[i];i++)
      {
         fprintf(fd, "channel_frequency_%d = %d\n", i, channels[i]->frequency);
         fprintf(fd, "channel_name_%d = %s\n", i, channels[i]->name);
         numchans = i;
      }
      fprintf(fd, "num_chans = %d\n", numchans+1);
   }

   fclose(fd);

   if (verbose) g_print("Configuration saved to %s\n", filename);
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
   gtk_tvplug_set(tv, "mute", 1);
   gtk_main_quit();
}

static void version()
{
   g_print("Linux Video Studio TV version " VERSION "\n");
}

static void usage()
{
   g_print("Linux Video Studio TV - a simple yet functional TV application\n");
   g_print("Usage: \'stv [options]\' - where options are:\n");
   g_print("  -p num    : Xvideo port number to use (see -t, default: %d)\n", port);
   g_print("  -m device : Mixer device (default: %s)\n", mixer_dev);
#ifdef HAVE_LIRC
   g_print("  -l device : Linux Infrared Remote Control device (default: %s)\n", lirc_dev);
#endif
   g_print("  -g WxH+X+Y: Size/position of the window (default: like last session)\n");
   g_print("  -t        : probe Xvideo ports for video cards\n");
   g_print("  -c name   : config name (default: \'%s\')\n", tv_config_file);
   g_print("  -h        : this information\n");
   g_print("  -v        : version information\n");
   g_print("  -d        : debug/verbose mode\n");
   g_print("\n");
   g_print("(c) copyright Ronald Bultje, 2000-2001 - under the terms of the GPL\n");
}

static gint reset_size(gpointer data)
{
   GdkGeometry geom;

   geom.min_width = GTK_TVPLUG(tv)->width_best/16;
   geom.min_height = GTK_TVPLUG(tv)->height_best/16;
   geom.width_inc = GTK_TVPLUG(tv)->width_best/16;
   geom.height_inc = GTK_TVPLUG(tv)->height_best/16;
   geom.max_width = GTK_TVPLUG(tv)->width_best;
   geom.max_height = GTK_TVPLUG(tv)->height_best;
   gtk_window_set_geometry_hints(GTK_WINDOW(window), window, &geom,
      GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_RESIZE_INC);

   return FALSE;
}


static void goto_previous_channel()
{
   int old_cur = current_channel;
   
   if (previous_channel < 0) return;
   
   current_channel = previous_channel;
   previous_channel = old_cur;

   gtk_tvplug_set(tv, "frequency", channels[current_channel]->frequency);
}

static void create_screenshot()
{
#if defined(HAVE_PNG) || defined(HAVE_JPEG)
   GdkPixbuf *buf;

   if (!window) return;
   if (!GTK_WIDGET_VISIBLE(tv)) return;

   /* ref/get */
   buf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, 8, 
      min(tv->allocation.width, GTK_TVPLUG(tv)->width_best),
      min(tv->allocation.height, GTK_TVPLUG(tv)->height_best));
   if (!buf)
   {
      gtk_show_text_window(STUDIO_ERROR,
         "creating a pixbuf failed, are you out of memory?", NULL);
      return;
   }
   buf = gdk_pixbuf_get_from_drawable(buf, tv->window,
      gtk_widget_get_default_colormap(),
      tv->allocation.width>GTK_TVPLUG(tv)->width_best?(tv->allocation.width-GTK_TVPLUG(tv)->width_best)/2:0,
      tv->allocation.height>GTK_TVPLUG(tv)->height_best?(tv->allocation.height-GTK_TVPLUG(tv)->height_best)/2:0,
      0, 0,
      min(tv->allocation.width, GTK_TVPLUG(tv)->width_best),
      min(tv->allocation.height, GTK_TVPLUG(tv)->height_best));
   if (!buf)
   {
      gtk_show_text_window(STUDIO_ERROR,
         "Grabbing the screenshot failed", NULL);
      return;
   }

   gdk_pixbuf_save_to_file(buf);

#else
   gtk_show_text_window(STUDIO_ERROR,
      "Compiled without PNG/JPG support", "Screenshots not supported");
#endif
}

static void toggle_audio()
{
   static gboolean audio = TRUE;

   if (!window) return;
   if (!GTK_WIDGET_VISIBLE(tv)) return;

   audio = audio?FALSE:TRUE;

   if (audio)
   {
      if (verbose) g_print("Enabling audio\n");
      gtk_tvplug_set(tv, "mute", 0);
   }
   else
   {
      if (verbose) g_print("Disabling audio\n");
      gtk_tvplug_set(tv, "mute", 1);
   }
}

static void toggle_fullscreen()
{
   static gboolean full_screen = FALSE;
   static gint w=0,h=0,x=0,y=0;
   GdkGeometry geom;

   if (!window) return;
   if (!GTK_WIDGET_VISIBLE(tv)) return;

   full_screen = full_screen?FALSE:TRUE;

   if (full_screen)
   {
      /* go fullscreen */
      if (verbose) g_print("Going to full-screen mode (%dx%d)\n",
         gdk_screen_width(), gdk_screen_height());

      gtk_window_set_geometry_hints(GTK_WINDOW(window), window, NULL, 0);

      gdk_window_get_size(window->window, &w, &h);
      gdk_window_get_origin(window->window, &x, &y);

      gtk_widget_set_uposition(window, 0, 0);
      gtk_widget_set_usize(window, gdk_screen_width(), gdk_screen_height());
   }
   else
   {
      /* go back to windowed mode */
      if (verbose) g_print("Going to windowed mode (%dx%d)\n",
         w, h);

      geom.min_width = GTK_TVPLUG(tv)->width_best/16;
      geom.min_height = GTK_TVPLUG(tv)->height_best/16;
      geom.width_inc = GTK_TVPLUG(tv)->width_best/16;
      geom.height_inc = GTK_TVPLUG(tv)->height_best/16;
      geom.max_width = w; /* pfffffffffft */
      geom.max_height = h; /* pfffffffffft */
      gtk_window_set_geometry_hints(GTK_WINDOW(window), window, &geom,
         GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_RESIZE_INC);

      gtk_widget_set_uposition(window, x, y);
      gtk_widget_set_usize(window, w, h);
      gtk_timeout_add(100, (GtkFunction) reset_size, NULL); /* pffffffffffffft */
   }
}

static void video_slider_changed(GtkAdjustment *adj, char *what)
{
    gtk_tvplug_set(tv, what, adj->value);
}

#ifdef OSS
static void set_volume(int value)
{
   if (mixer_id > 0)
   {
      int num_devs = mixer_num_of_devs (mixer_id);
      int i;

      for (i=0;i<num_devs;i++)
      {
         if (!strcmp(mixer_get_label(mixer_id, i), "Vol  "))
         {
            mixer_set_vol_left (mixer_id, i, value);
            if (mixer_is_stereo (mixer_id, i))
               mixer_set_vol_right (mixer_id, i, value);
         }
      }
   }
   volume = value;

   if (adj_audio) gtk_adjustment_set_value(adj_audio, 100 - value);
}

static void audio_slider_changed(GtkAdjustment *adj, gpointer data)
{
   set_volume(100 - adj->value);
}
#endif

static GtkWidget *get_video_sliders_widget()
{
   GtkWidget *hbox, *vbox, *pixmap, *scrollbar;
   GtkTooltips *tooltip;
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
   GtkAdjustment *adj[4] = {
      GTK_TVPLUG(tv)->hue_adj,
      GTK_TVPLUG(tv)->contrast_adj,
      GTK_TVPLUG(tv)->brightness_adj,
      GTK_TVPLUG(tv)->saturation_adj
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
      gtk_signal_connect(GTK_OBJECT(adj[i]), "value_changed",
         GTK_SIGNAL_FUNC(video_slider_changed), names[i]);
      gtk_object_ref(GTK_OBJECT(adj[i]));
      scrollbar = gtk_vscale_new(adj[i]);
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

#ifdef OSS
   /* Audio volume slider */
   if (mixer_id > 0)
   {
      int num_devs = mixer_num_of_devs (mixer_id);
      int value = 0;
      for (i=0;i<num_devs;i++)
      {
         if (!strcmp(mixer_get_label(mixer_id, i), "Vol  "))
         {
            value = 100 - mixer_get_vol_left(mixer_id, i);
            break;
         }
      }

      vbox = gtk_vbox_new (FALSE, 0);
      adj_audio = GTK_ADJUSTMENT(gtk_adjustment_new(value,0,100,1,10,0));
      gtk_signal_connect(GTK_OBJECT(adj_audio), "value_changed",
         GTK_SIGNAL_FUNC(audio_slider_changed), NULL);
      scrollbar = gtk_vscale_new(adj_audio);
      gtk_scale_set_draw_value(GTK_SCALE(scrollbar), 0);
      gtk_box_pack_start(GTK_BOX (vbox), scrollbar, TRUE, TRUE, 10);
      gtk_widget_show(scrollbar);
      gtk_widget_set_usize(scrollbar, -1, 150);
      gtk_tooltips_set_tip(tooltip, scrollbar, "Audio Volume", NULL);
      pixmap = gtk_widget_from_xpm_data(slider_volume_xpm);
      gtk_widget_show(pixmap);
      gtk_box_pack_start(GTK_BOX(vbox), pixmap, FALSE, FALSE, 10);
      gtk_box_pack_start(GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
      gtk_widget_show(vbox);
   }
#endif

   return hbox;
}

static void row_selected(GtkCList *clist, gint row, gint column, GdkEvent *event, gpointer data)
{
   char *ldata;

   gtk_clist_get_text(clist, row, 0, &ldata);
   gtk_entry_set_text(GTK_ENTRY(chanlist_entry), ldata);
   gtk_clist_get_text(clist, row, 1, &ldata);
   gtk_adjustment_set_value(GTK_TVPLUG(tv)->frequency_adj, atoi(ldata));
   if (channels)
   {
      int i;
      for (i=0;channels[i];i++)
      {
         if (channels[i]->frequency == atoi(ldata))
         {
            previous_channel = current_channel;
            current_channel = i;
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
      gtk_show_text_window(STUDIO_WARNING, "Please supply a channel name", NULL);
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
   sprintf(buf, "%d", (int)GTK_TVPLUG(tv)->frequency_adj->value);
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
   gtk_show_text_window(STUDIO_WARNING, "Please select a channel to delete", NULL);
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

#ifdef HAVE_LIRC
   for (i=0;i<RC_NUM_KEYS;i++)
      remote_buttons[i] = g_strdup(gtk_entry_get_text(GTK_ENTRY(rc_entry[i])));
#endif

   save_options();
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

static void options_window_unrealize(GtkWidget *widget, gpointer data)
{
   int *bla = (int *) data;
   *bla = 0;

#ifdef HAVE_LIRC
   focussed_entry = NULL;
   focussed_label = NULL;
#endif
}

static void settings_window_unrealize(GtkWidget *widget, gpointer data)
{
   int *bla = (int *) data;
   *bla = 0;

#ifdef OSS
   adj_audio = NULL;
#endif
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
            gtk_show_text_window(STUDIO_WARNING, "Channel is already down there", NULL);
         return;
      }
   }

   /* if we get here, it's not good */
   gtk_show_text_window(STUDIO_WARNING, "Please select a channel to move", NULL);
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
            gtk_show_text_window(STUDIO_WARNING, "Channel is already on top", NULL);
         return;
      }
   }

   /* if we get here, it's not good */
   gtk_show_text_window(STUDIO_WARNING, "Please select a channel to move", NULL);
}

#ifdef HAVE_LIRC
static void entry_got_focus(GtkWidget *Widget, GdkEventFocus *event, gpointer data)
{
   focussed_entry = GTK_WIDGET(data);
}

static GtkWidget *get_rc_button_selection_notebook_page()
{
   GtkWidget *table, *scroll_window, *vboxmain, *hboxmain, *label, *vbox;
   int i;

   vboxmain = gtk_vbox_new(FALSE, 0);
   hboxmain = gtk_hbox_new(FALSE, 0);

   scroll_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
      GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

   vbox = gtk_vbox_new(FALSE, 10);

   table = gtk_table_new (2, RC_NUM_KEYS, FALSE);
   //gtk_table_set_col_spacings(GTK_TABLE(table), 5);
   for (i=0;i<RC_NUM_KEYS;i++)
   {
      label = gtk_label_new(rc_names[i]);
      gtk_misc_set_alignment(GTK_MISC(label), 1.0, GTK_MISC(label)->yalign);
      gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, i, i+1);
      gtk_widget_show(label);

      rc_entry[i] = gtk_entry_new();
      gtk_signal_connect(GTK_OBJECT(rc_entry[i]), "focus_in_event",
         GTK_SIGNAL_FUNC(entry_got_focus), rc_entry[i]);
      gtk_widget_set_usize(rc_entry[i], 100, -1);
      gtk_entry_set_text(GTK_ENTRY(rc_entry[i]), remote_buttons[i]);
      gtk_table_attach_defaults (GTK_TABLE (table), rc_entry[i], 1, 2, i, i+1);
      gtk_widget_show(rc_entry[i]);
   }
   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_window), table);
   gtk_widget_show(table);
   gtk_box_pack_start(GTK_BOX (vbox), scroll_window, TRUE, TRUE, 0);
   gtk_widget_show(scroll_window);
   focussed_label = gtk_label_new("Button_pressed: ");
   gtk_box_pack_start(GTK_BOX (vbox), focussed_label, FALSE, FALSE, 0);
   gtk_widget_show(focussed_label);

   gtk_box_pack_start(GTK_BOX (hboxmain), vbox, FALSE, FALSE, 10);
   gtk_widget_show(vbox);
   gtk_box_pack_start(GTK_BOX (vboxmain), hboxmain, TRUE, TRUE, 10);
   gtk_widget_show(hboxmain);

   return vboxmain;
}
#endif

static GtkWidget *get_channel_list_notebook_page(GtkWidget *channellist)
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
      GTK_SIGNAL_FUNC(video_slider_changed), "frequency");
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
   button = gtk_image_label_button("Add", stv_accept_xpm, 5, GTK_POS_RIGHT);
   gtk_tooltips_set_tip(tooltip, button, "Add Current Entry to Channel List", NULL);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(save_data), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);
   button = gtk_image_label_button("Del", stv_reject_xpm, 5, GTK_POS_RIGHT);
   gtk_tooltips_set_tip(tooltip, button, "Remove Current Entry from Channel List", NULL);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(del_data), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);
   button = gtk_image_label_button("Up", stv_arrow_up_xpm, 5, GTK_POS_RIGHT);
   gtk_tooltips_set_tip(tooltip, button, "Push Current Entry One Position Up", NULL);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(move_data_up), GTK_CLIST(channellist));
   gtk_box_pack_start(GTK_BOX (hbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);
   button = gtk_image_label_button("Down", stv_arrow_down_xpm, 5, GTK_POS_RIGHT);
   gtk_tooltips_set_tip(tooltip, button, "Pull Current Entry One Position Down", NULL);
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

static void show_channel_editor(GtkWidget *widget, gpointer data)
{
   GtkWidget *vbox, *hbox, *button, *pop_window, *channellist, *notebook;
   static int show = 0;
   char *list[2];

   if (show) return;
   if (verbose) g_print("Showing channel editor\n");
   show = 1;

   pop_window = gtk_window_new(GTK_WINDOW_DIALOG);

   gtk_window_set_title (GTK_WINDOW(pop_window), "Linux Video Studio TV - Settings");
   gtk_container_set_border_width (GTK_CONTAINER (pop_window), 5);

   vbox = gtk_vbox_new(FALSE, 10);
   notebook = gtk_notebook_new ();

   list[0] = "Channel Name"; list[1] = "Frequency";
   channellist = gtk_clist_new_with_titles(2, list);
   hbox = get_channel_list_notebook_page(channellist);
   gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
      hbox, gtk_label_new("Channel Editor"));
   gtk_widget_show(hbox);
#ifdef HAVE_LIRC
   hbox = get_rc_button_selection_notebook_page();
   gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
      hbox, gtk_label_new("Remote Control"));
   gtk_widget_show(hbox);
#endif
   gtk_box_pack_start(GTK_BOX (vbox), notebook, FALSE, FALSE, 0);
   gtk_widget_show(notebook);

   hbox = gtk_hbox_new(TRUE, 5);
   button = gtk_button_new_with_label("OK");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC(save_list), GTK_CLIST(channellist));
   gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
      GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT (pop_window));
   gtk_box_pack_start(GTK_BOX (hbox), button, TRUE, TRUE, 15);
   gtk_widget_show(button);
   button = gtk_button_new_with_label("Cancel");
   gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
      GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT (pop_window));
   gtk_box_pack_start(GTK_BOX (hbox), button, TRUE, TRUE, 15);
   gtk_widget_show(button);
   gtk_box_pack_start(GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
   gtk_widget_show(hbox);

   gtk_container_add (GTK_CONTAINER (pop_window), vbox);
   gtk_widget_show(vbox);

   gtk_signal_connect(GTK_OBJECT(pop_window), "unrealize",
      GTK_SIGNAL_FUNC(options_window_unrealize), (gpointer)(&show));

   gtk_window_set_policy(GTK_WINDOW(pop_window), 0, 0, 0); /* pffffffffft */
   gtk_window_set_transient_for(GTK_WINDOW(pop_window), GTK_WINDOW(window)); /* ? */
   gtk_widget_show(pop_window);
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
         previous_channel = current_channel;
         current_channel = i;
         break;
      }
   }

   if (frequency == -1)
   {
      if (verbose) g_print("WARNING: unknown channel\n");
      return;
   }

   if (verbose) g_print("Changing channel to %s (frequency: %d)\n",
      gtk_entry_get_text(GTK_ENTRY(widget)), frequency);

   gtk_tvplug_set(tv, "frequency", frequency);
}

static GtkWidget *get_video_channels_widget()
{
   GtkWidget *vbox;
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

   return vbox;
}

static GtkWidget *get_settings_button_widget()
{
   GtkWidget *button, *vbox;

   vbox = gtk_vbox_new(FALSE, 20);

   button = gtk_button_new_with_label("Settings");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
      GTK_SIGNAL_FUNC (show_channel_editor), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, FALSE, 0);
   gtk_widget_show(button);

   return vbox;
}

static void show_options_window()
{
   GtkWidget *box, *vbox, *pop_window;
   static int show = 0;

   if (show) return;
   if (verbose) g_print("Showing options window\n");
   show=1;

   pop_window = gtk_window_new(GTK_WINDOW_DIALOG);

   gtk_window_set_title (GTK_WINDOW(pop_window), "Linux Video Studio TV - Control");
   gtk_container_set_border_width (GTK_CONTAINER (pop_window), 15);

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

   box = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, FALSE, 0);
   gtk_widget_show(box);

   box = get_settings_button_widget();
   gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, FALSE, 0);
   gtk_widget_show(box);

   gtk_container_add (GTK_CONTAINER (pop_window), vbox);
   gtk_widget_show(vbox);

   gtk_signal_connect(GTK_OBJECT(pop_window), "unrealize",
      GTK_SIGNAL_FUNC(settings_window_unrealize), (gpointer)(&show));

   gtk_window_set_policy(GTK_WINDOW(pop_window), 0, 0, 0);
   gtk_window_set_transient_for(GTK_WINDOW(pop_window), GTK_WINDOW(window)); /* ? */
   gtk_widget_show(pop_window);
}

static void increase_channel(int num)
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
   }
}

static void goto_channel(int num)
{
   increase_channel(num-current_channel);
}

static gint remove_timeout(gpointer data)
{
   gint *num = (gint *) data;
   *num = -1;
   return FALSE;
}

static void set_timeout_goto_channel(int num)
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

static void tv_typed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
   int i=0;
   if (verbose)
   {
      g_print("Keyboard key press event (key ");
      if (event->keyval<0x100) g_print("\'%c\'", event->keyval);
      else g_print("0x%x", event->keyval);
      g_print(")\n");
   }
   switch (event->keyval)
   {
      case GDK_Right:
      case GDK_Down:
         increase_channel(1);
         break;
      case GDK_Left:
      case GDK_Up:
         increase_channel(-1);
         break;
      case GDK_Home:
         goto_channel(0);
         break;
      case GDK_End:
         if (channels) for(i=0;channels[i];i++)
         goto_channel(i-1);
         break;
      case GDK_Escape:
      case GDK_KP_Enter:
      case GDK_Return:
         toggle_fullscreen();
         break;
      case GDK_q:
         if (!delete_event(NULL, NULL, NULL)) destroy(NULL, NULL);
         break;
      case GDK_a:
         toggle_audio();
         break;
      case GDK_s:
         create_screenshot();
         break;
      case GDK_p:
         goto_previous_channel();
	 break;
      case GDK_KP_0:
      case GDK_KP_1:
      case GDK_KP_2:
      case GDK_KP_3:
      case GDK_KP_4:
      case GDK_KP_5:
      case GDK_KP_6:
      case GDK_KP_7:
      case GDK_KP_8:
      case GDK_KP_9:
         set_timeout_goto_channel(event->keyval - GDK_KP_0);
         break;
      case GDK_0:
      case GDK_1:
      case GDK_2:
      case GDK_3:
      case GDK_4:
      case GDK_5:
      case GDK_6:
      case GDK_7:
      case GDK_8:
      case GDK_9:
         set_timeout_goto_channel(event->keyval - GDK_0);
         break;
   }
}

static void tv_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
   if (verbose) g_print("Mouse button press event (button %d)\n", event->button);
   switch (event->button)
   {
      case 2:
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

#ifdef HAVE_LIRC
static void remote_button_action(gint button_num)
{
   switch(button_num)
   {
      case RC_0:
      case RC_1:
      case RC_2:
      case RC_3:
      case RC_4:
      case RC_5:
      case RC_6:
      case RC_7:
      case RC_8:
      case RC_9:
         set_timeout_goto_channel(button_num - RC_0);
         break;
      case RC_MUTE:
         toggle_audio();
         break;
      case RC_CHAN_UP:
         increase_channel(1);
         break;
      case RC_CHAN_DOWN:
         increase_channel(-1);
         break;
      case RC_FULLSCREEN:
         toggle_fullscreen();
         break;
      case RC_SCREENSHOT:
         create_screenshot();
         break;
      case RC_QUIT:
         if (!delete_event(NULL, NULL, NULL)) destroy(NULL, NULL);
         break;
#ifdef OSS
      case RC_VOLUME_UP:
         set_volume(volume + 2);
         break;
      case RC_VOLUME_DOWN:
         set_volume(volume - 2);
         break;
#endif
      case RC_PREVIOUS_CHAN:
         goto_previous_channel();
         break;
   }
}

static gint reset_label(gpointer data)
{
   gint *bla = (int *) data;
   *bla = -1;
   if (focussed_label) gtk_label_set_text(GTK_LABEL(focussed_label), "Button pressed: ");
   return FALSE;
}

static void remote_button_pressed(gpointer data, gint source,
   GdkInputCondition condition)
{
   char input[256], button_name[256], buff[256];
   int n;
   int button_num, time;
   static gint timeout_id = -1;

   n = read(source, input, 255);
   if (n < 0)
   {
      printf("**ERROR: error reading the remote control button: %s\n",
         (char *)sys_errlist[errno]);
      return;
   }
   if (n == 0)
   {
      return;
   }
   input[n] = '\0';
   while (n>0)
   {
      if (input[n] == '\n')
         input[n] = '\0';
      n--;
   }

   if (sscanf(input, "%16x %2x %s ", &button_num, &time, button_name)!=3) return;
   if (time == 0 && focussed_entry)
   {
      gtk_entry_set_text(GTK_ENTRY(focussed_entry), button_name);
   }
   if (time == 0 && focussed_label)
   {
      sprintf(buff, "Button pressed: %s", button_name);
      gtk_label_set_text(GTK_LABEL(focussed_label), buff);
      if (timeout_id >= 0) gtk_timeout_remove(timeout_id);
      timeout_id = gtk_timeout_add(1000, (GtkFunction) reset_label, (gpointer)(&timeout_id));
   }
   if (time>4 || time==0)
   {
      if (verbose) printf("Remote control button was pressed: \'%s\' (%d)\n",
         button_name, button_num);
      for(n=0;n<RC_NUM_KEYS;n++)
      {
         if (!strcmp(button_name, remote_buttons[n]))
            remote_button_action(n);
      }
   }
}

static gint lirc_init()
{
   int fd;
   struct sockaddr_un addr;

   addr.sun_family = AF_UNIX;
   if (!lirc_dev) lirc_dev = "/dev/lircd";
   strcpy(addr.sun_path, lirc_dev);

   fd = socket(AF_UNIX,SOCK_STREAM,0);
   if (fd == -1)
   {
      printf("**ERROR: LIRC socket init failed: %s\n",
         (char *)sys_errlist[errno]);
      return 0;
   }
   if(connect(fd, &addr, sizeof(addr)) == -1)
   {
      printf("**ERROR: LIRC connect init failed: %s\n",
         (char *)sys_errlist[errno]);
      return 0;
   }
   gdk_input_add (fd, GDK_INPUT_READ, remote_button_pressed, NULL);

   return 1;
}
#endif

static void focus_in_event(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
   GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
}

static void focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
   GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
}

static int parse_command_line_options(int *argc, char **argv[],
	int *prt, int *width, int *height, int *x_off, int *y_off)
{
   int n;
   int w,h,x,y,i;
   char c1, c2;

   while((n=getopt(*argc,*argv,"htvdp:c:g:l:m:")) != EOF)
   {
      switch (n)
      {
         case 'p':
            *prt = atoi(optarg);
            break;
         case 'h':
            usage();
            exit(1);
            break;
         case 't':
            gtk_tvplug_new(-1);
            exit(1);
            break;
         case 'v':
            version();
            exit(1);
            break;
         case 'd':
            verbose = TRUE;
            break;
         case 'c':
            tv_config_file = optarg;
            break;
         case 'g':
            i = sscanf(optarg, "%dx%d%c%d%c%d", &w, &h, &c1, &x, &c2, &y);
            if (i>=1) *width = w;
            if (i>=2) *height = h;
            if (i>=4 && (c1=='-'||c1=='+')) *x_off = (c1=='-'?-x:x);
            if (i>=6 && (c2=='-'||c2=='+')) *y_off = (c2=='-'?-y:y);
            break;
#ifdef HAVE_LIRC
         case 'l':
            lirc_dev = optarg;
            break;
#endif
         case 'm':
            mixer_dev = optarg;
            break;
      }
   }

   return 1;
}

int main(int argc, char *argv[])
{
   int p=0, w=0, h=0, x=-1000000, y=-1000000;
   GdkGeometry geom;

   gtk_init(&argc, &argv);
   if (!parse_command_line_options(&argc, &argv, &p, &w, &h, &x, &y)) return 1;
   load_options(w?NULL:&w, h?NULL:&h, x!=-1000000?NULL:&x, y!=-1000000?NULL:&y);
   if (p) port = p;

   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_signal_connect (GTK_OBJECT (window), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), NULL);
   gtk_signal_connect (GTK_OBJECT (window), "destroy",
      GTK_SIGNAL_FUNC (destroy), NULL);
   gtk_window_set_policy(GTK_WINDOW(window), 1, 1, 0);

   gtk_window_set_title (GTK_WINDOW (window), "Linux Video Studio TV");
   gtk_container_set_border_width (GTK_CONTAINER (window), 0);
   tv = gtk_tvplug_new(port);
   if (!tv)
   {
      g_print("ERROR: no suitable video4linux device found\n");
      g_print("Please supply a video4linux Xv-port manually (\'stv -p <num\')\n");
      return 1;
   }
   if (channels)
   {
      int i;
      for(i=0;channels[i];i++)
         if (channels[i]->frequency == (int)GTK_TVPLUG(tv)->frequency_adj->value)
         {
            current_channel = i;
            break;
         }
   }
   if (!port) port = GTK_TVPLUG(tv)->port;
   GTK_WIDGET_SET_FLAGS(tv, GTK_CAN_FOCUS); /* key press events */
   gtk_signal_connect(GTK_OBJECT(window), "button_press_event",
      GTK_SIGNAL_FUNC(tv_clicked), NULL);
   gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
      GTK_SIGNAL_FUNC(tv_typed), NULL);
   gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
      GTK_SIGNAL_FUNC(focus_in_event), NULL);
   gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
      GTK_SIGNAL_FUNC(focus_out_event), NULL);
   gtk_container_add (GTK_CONTAINER (window), tv);
   gtk_widget_show(tv);
   set_background_color(tv, 0, 0, 0);
   set_background_color(window, 0, 0, 0);
   geom.min_width = GTK_TVPLUG(tv)->width_best/16;
   geom.min_height = GTK_TVPLUG(tv)->height_best/16;
   geom.max_width = GTK_TVPLUG(tv)->width_best;
   geom.max_height = GTK_TVPLUG(tv)->height_best;
   geom.width_inc = GTK_TVPLUG(tv)->width_best/16;
   geom.height_inc = GTK_TVPLUG(tv)->height_best/16;
   gtk_window_set_geometry_hints(GTK_WINDOW(window), window, &geom,
      GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_RESIZE_INC);
   gtk_tvplug_set(tv, "mute", 0);
   if (w>0 && h>0) gtk_widget_set_usize(window, w, h);
   gtk_widget_show(window);
   if (x!=-1000000 && y!=-1000000) gtk_widget_set_uposition(window, x, y);

#ifdef HAVE_LIRC
   lirc_init();
#endif
#ifdef OSS
   mixer_id = mixer_init (mixer_dev);
   if (mixer_id<=0)
      g_print("**ERROR: opening mixer device (%s): %s\n",
         mixer_dev, sys_errlist[errno]);
   else
   {
      int num_devs = mixer_num_of_devs (mixer_id);
      int i;
      for (i=0;i<num_devs;i++)
      {
         if (!strcmp(mixer_get_label(mixer_id, i), "Vol  "))
         {
            volume = mixer_get_vol_left(mixer_id, i);
            break;
         }
      }
   }

#endif

   gtk_main();

   return 0;
}
