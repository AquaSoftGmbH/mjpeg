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

#ifdef HAVE_LIRC

#include <gtk/gtk.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>

#include "lirc.h"

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

extern GtkWidget *tv;
extern int verbose;
static GtkWidget *focussed_entry = NULL, *focussed_label = NULL;


static void entry_got_focus(GtkWidget *Widget, GdkEventFocus *event, gpointer data)
{
   focussed_entry = GTK_WIDGET(data);
}

static void options_window_unrealize(GtkWidget *widget, gpointer data)
{
   focussed_entry = NULL;
   focussed_label = NULL;
}

GtkWidget *get_rc_button_selection_notebook_page()
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

   gtk_signal_connect(GTK_OBJECT(vboxmain), "unrealize",
      GTK_SIGNAL_FUNC(options_window_unrealize), NULL);

   return vboxmain;
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

gint lirc_init()
{
   static int fd=0;
   struct sockaddr_un addr;

   if (fd>0) close(fd);

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
