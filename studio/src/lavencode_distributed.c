/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * lavencode_distributed done by Bernhard Praschinger
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
 * Here is the layout for the distributed encoding done.
 * And the systems set where the process should run.
 * For the distribution RSH is used. 
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
void accept_option (gpointer data);
void create_machine(GtkWidget *hbox);
void create_audio (GtkWidget *hbox);
void create_video (GtkWidget *hbox);
void set_mname (GtkWidget *menu_item, gpointer data);
void add_mname (GtkWidget *wideget, gpointer data);
void remove_mname (GtkWidget *wideget, gpointer data);
void work_values(gpointer data);
void set_lav2wav (GtkWidget *menu_item, gpointer data);
void set_mp2enc (GtkWidget *menu_item, gpointer data);

/* Some Constant values */
#define NUMBER_MACHINE 10

/* Some Variables */
GtkWidget *enter_machname;
char remove_name[LONGOPT];

struct machine tempmach;
struct machine *point;
GList *temp_mnames;

/* =============================================================== */
/* Start of the code */

/* Here we may add the given name to the computer list */
void add_mname (GtkWidget *wideget, gpointer data)
{
int i, do_not_insert;
gchar *val;

do_not_insert = 0;

  val = gtk_entry_get_text (GTK_ENTRY(enter_machname));

  if ((strlen(val)>1) && (strlen(val)<LONGOPT) && (strcmp(val,"localhost")!=0))
  {
    temp_mnames= g_list_first(temp_mnames);

    for ( i = 0; i < g_list_length(temp_mnames); i++)
      {
        if (strcmp(val,g_list_nth_data(temp_mnames, i)) == 0)
          do_not_insert = 1;
      }

    if (do_not_insert == 0)
      temp_mnames = g_list_append (temp_mnames, g_strdup(val));

    temp_mnames= g_list_first(temp_mnames);
  }

  if (verbose)
    {
      for(i=0; i<g_list_length(temp_mnames); i++)
        printf("Maschine = %s \n", (char*) g_list_nth_data(temp_mnames, i));
    }

}

/* Now we definitly remove the name of the machine from the list. */
/* Could be I have to add the updating of the other Select boxes too */
void remove_mname (GtkWidget *wideget, gpointer data)
{
int i;

  if ( (strcmp((remove_name),"localhost") != 0) && (strlen(remove_name) > 0) )
  {
  temp_mnames= g_list_first(temp_mnames);
  
  for ( i = 0; i < (g_list_length(temp_mnames)); i++)
    {
      if (strcmp(remove_name ,(char*) g_list_nth_data(temp_mnames,i)) == 0)
        {
          temp_mnames = g_list_remove (temp_mnames, 
                                          g_list_nth_data (temp_mnames, i));
          break;
        }
    } 
  }

}

/* Here we set a Value with the machine name that might be removed from the 
 * list of aviable machines */
void set_mname (GtkWidget *menu_item, gpointer data)
{
  strcpy(remove_name, (char*)data);
}

/* Here we set the number according to the name given for lav2wav */
void set_lav2wav (GtkWidget *menu_item, gpointer data)
{
int i;
 
  for(i=0; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0 )
      {
        tempmach.lav2wav = i;
        break;
      }
 
  if (verbose)
    printf("Set machine number for lav2wav to %i, will be: %s \n",
                    tempmach.lav2wav, (char*) g_list_nth_data(temp_mnames,i));
}

/* Here we set the number according to the name given for mp2enc*/
void set_mp2enc (GtkWidget *menu_item, gpointer data)
{
int i;

  for(i=0; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0 )
      {
        tempmach.mp2enc = i;
        break;
      }
  
  if (verbose)
    printf("Set machine number to %i, will be: %s \n",
                    tempmach.mp2enc, (char*) g_list_nth_data(temp_mnames,i));
}


/* Here we create the layout for the machine data */
void create_machine(GtkWidget *hbox)
{
GtkWidget *label, *button_add, *table, *remove_mnames; 
GtkWidget *menu, *menu_item, *button_remove;
int tx, ty; /* table size x, y */
int i; 

tx=3;
ty=2;

  table = gtk_table_new (tx,ty, FALSE);

  tx=0;
  ty=0;

  label = gtk_label_new (" Machine name : ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  /* The input field for the machines */
  enter_machname = gtk_entry_new();

  gtk_entry_set_max_length (GTK_ENTRY(enter_machname), LONGOPT);
  gtk_table_attach_defaults (GTK_TABLE(table),enter_machname, tx,tx+1, ty,ty+1);

  gtk_widget_show (enter_machname);
  tx++;

  button_add = gtk_button_new_with_label (" ADD ");
  gtk_signal_connect(GTK_OBJECT(button_add), "clicked", 
                                           GTK_SIGNAL_FUNC (add_mname), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), button_add, tx, tx+1, ty, ty+1);
  gtk_widget_show(button_add);
  tx++;

  ty++;  /*to the next row */

  tx=1;  /* the first field is left empty */

  /* The remove field for the machines */
  remove_mnames = gtk_option_menu_new ();
  menu = gtk_menu_new();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      menu_item = gtk_menu_item_new_with_label 
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate", set_mname, 
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menu), menu_item);
      gtk_widget_show (menu_item);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (remove_mnames), menu);
  gtk_table_attach_defaults (GTK_TABLE(table),remove_mnames, tx,tx+1, ty,ty+1);
  gtk_widget_show(remove_mnames);

  tx++;
 
  button_remove = gtk_button_new_with_label (" Remove ");
  gtk_signal_connect(GTK_OBJECT(button_remove), "clicked",
                                       GTK_SIGNAL_FUNC (remove_mname), NULL);
  gtk_table_attach_defaults (GTK_TABLE (table),button_remove, tx,tx+1, ty,ty+1);
  gtk_widget_show(button_remove);
  tx++;

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

}

/* Here we create the layout for the audio encoding */
void create_audio (GtkWidget *hbox)
{
GtkWidget *label, *table, *mlav2wav, *menu, *menu_item;
int i,tx,ty; /* table size x, y*/

i=0;
tx=1;
ty=2;

  table = gtk_table_new (ty,ty, FALSE);
  
  tx=0;
  ty=0;
 
  label = gtk_label_new (" lav2wav ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;
 
  label = gtk_label_new ("| mp2enc ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  tx=0;
  ty++;

  /* The field for the lav2wav machine */
  mlav2wav = gtk_option_menu_new ();
  menu = gtk_menu_new();
  
  for (i = 0; i < g_list_length(temp_mnames); i++)
    { 
      menu_item = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate", set_lav2wav,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menu), menu_item);
      gtk_widget_show (menu_item);
    }

  gtk_widget_set_usize (mlav2wav, 100, -2 );
  gtk_option_menu_set_menu (GTK_OPTION_MENU (mlav2wav), menu);
  gtk_table_attach_defaults (GTK_TABLE(table),mlav2wav, tx,tx+1, ty,ty+1);
  gtk_widget_show(mlav2wav);

  tx++;

  /* The field for the mp2enc machine */
  mlav2wav = gtk_option_menu_new ();
  menu = gtk_menu_new();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      menu_item = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate", set_mp2enc,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menu), menu_item);
      gtk_widget_show (menu_item);
    } 
    
  gtk_widget_set_usize (mlav2wav, 100, -2 );
  gtk_option_menu_set_menu (GTK_OPTION_MENU (mlav2wav), menu);
  gtk_table_attach_defaults (GTK_TABLE(table),mlav2wav, tx,tx+1, ty,ty+1);
  gtk_widget_show(mlav2wav);


  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

}

/* Here we create the layout for the vide encoding part */
void create_video (GtkWidget *hbox)
{
GtkWidget *label, *table;
int tx,ty; /* table size x, y*/

tx=5;
ty=3;

  table = gtk_table_new (ty,ty, FALSE);
  
  tx=0;
  ty=0;

  tx=3;
  label = gtk_label_new ("| mpeg2enc");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  ty++;
  tx=0;

  label = gtk_label_new (" yuv2lav ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;
  
  label = gtk_label_new ("| yuvdenoise ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;
  
  label = gtk_label_new ("| yuvscaler ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;
  
  label = gtk_label_new ("| yuv2divx ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;
 
  ty++;
  tx=3;

  label = gtk_label_new ("| yuv2lav  ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  tx++;

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);
}

/* Here we copy the original values into temp. Values */
void work_values(gpointer data)
{
int i;

  if ((strcmp((char*)data,"MPEG1") == 0) || (enhanced_settings == 0))
    point = &machine4mpeg1;
  if ((strcmp((char*)data,"MPEG2") == 0) && (enhanced_settings == 1))
    point = &machine4mpeg2;
  if ((strcmp((char*)data,"VCD") == 0)   && (enhanced_settings == 1))
    point = &machine4vcd;
  if ((strcmp((char*)data,"SVCD") == 0)  && (enhanced_settings == 1))
    point = &machine4svcd;
  if ((strcmp((char*)data,"DivX") == 0)  && (enhanced_settings == 1))
    point = &machine4divx;
  if ((strcmp((char*)data,"MJPEG") == 0) && (enhanced_settings == 1))
    point = &machine4yuv2lav;
  else 
    point = &machine4mpeg1; /* fallback should never be used ;) */

  /* copy the values */
  tempmach.lav2wav   = (*point).lav2wav;
  tempmach.mp2enc    = (*point).mp2enc;
  tempmach.lav2yuv   = (*point).lav2yuv;
  tempmach.yuvdenoise= (*point).yuvdenoise;
  tempmach.yuvscaler = (*point).yuvscaler;
  tempmach.mpeg2enc  = (*point).mpeg2enc;
  tempmach.yuv2divx  = (*point).yuv2divx;
  tempmach.yuv2lav   = (*point).yuv2lav;
 
  g_list_free(temp_mnames); 
  temp_mnames = NULL;

  for (i = 0; i < g_list_length(machine_names); i++)
    temp_mnames = g_list_append(temp_mnames, g_list_nth_data(machine_names,i));

}

/* Here we save the chnaged options */
void accept_option (gpointer data)
{
int i;

  if ((strcmp((char*)data,"MPEG1") == 0) || (enhanced_settings == 0))
    point = &machine4mpeg1;
  if ((strcmp((char*)data,"MPEG2") == 0) && (enhanced_settings == 1))
    point = &machine4mpeg2;
  if ((strcmp((char*)data,"VCD") == 0)   && (enhanced_settings == 1))
    point = &machine4vcd;
  if ((strcmp((char*)data,"SVCD") == 0)  && (enhanced_settings == 1))
    point = &machine4svcd;
  if ((strcmp((char*)data,"DivX") == 0)  && (enhanced_settings == 1))
    point = &machine4divx;
  if ((strcmp((char*)data,"MJPEG") == 0) && (enhanced_settings == 1))
    point = &machine4yuv2lav;
  else 
    point = &machine4mpeg1; /* fallback should never be used ;) */

  /* Copy back the values */
  (*point).lav2wav   = tempmach.lav2wav;
  (*point).mp2enc    = tempmach.mp2enc;
  (*point).lav2yuv   = tempmach.lav2yuv;
  (*point).yuvdenoise= tempmach.yuvdenoise;
  (*point).yuvscaler = tempmach.yuvscaler;
  (*point).mpeg2enc  = tempmach.mpeg2enc;
  (*point).yuv2divx  = tempmach.yuv2divx;
  (*point).yuv2lav   = tempmach.yuv2lav;

  /* free the orignal g_list and copy back the temp values */
  g_list_free(machine_names); 
  machine_names = NULL;

  for (i = 0; i < g_list_length(temp_mnames); i++)
    machine_names = g_list_append(machine_names,g_list_nth_data(temp_mnames,i));
}

/* open a new window with all the options in it */
void open_distributed_window(GtkWidget *widget, gpointer data)
{
GtkWidget *distribute_window, *button;
GtkWidget *vbox, *hbox, *separator; 

  work_values(data);

  distribute_window = gtk_window_new(GTK_WINDOW_DIALOG);
  hbox = gtk_hbox_new (FALSE, 10);
 
  vbox = gtk_vbox_new (FALSE, 10);

  create_machine(hbox);

  separator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator); 

  create_audio (hbox);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox); 

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 5);
  gtk_widget_show (separator);

  hbox = gtk_hbox_new (FALSE, 10);
  create_video (hbox);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show(hbox); 

  /* Show the OK and cancel Button */
  hbox = gtk_hbox_new(TRUE, 20);

  button = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
                     GTK_SIGNAL_FUNC (accept_option), data);
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
}
