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
 * For the distribution RSH is used. How it works look in lavencode.c 
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
void add_mname (GtkWidget *widget, gpointer data);
void remove_mname (GtkWidget *widget, gpointer data);
void add_item_to_menu(gchar *val);
void work_values(gpointer data);
void set_lav2wav   (GtkWidget *menu_item, gpointer data);
void set_mp2enc    (GtkWidget *menu_item, gpointer data);
void set_mpeg2enc  (GtkWidget *menu_item, gpointer data);
void set_yuv2lav   (GtkWidget *menu_itme, gpointer data);
void set_lav2yuv   (GtkWidget *menu_itme, gpointer data);
void set_yuvdenoise(GtkWidget *menu_itme, gpointer data);
void set_yuvscaler (GtkWidget *menu_itme, gpointer data);
void set_yuv2divx  (GtkWidget *menu_itme, gpointer data);
void set_up_defaults(void);
void create_menu4mpeg2enc  (GtkWidget *table, int *tx, int *ty);
void create_menu4yuv2lav   (GtkWidget *table, int *tx, int *ty);
void create_menu4lav2yuv    (GtkWidget *table, int *tx, int *ty);
void create_menu4yuvdenoise (GtkWidget *table, int *tx, int *ty);
void create_menu4yuvscaler  (GtkWidget *table, int *tx, int *ty);
void create_menu4yuv2divx   (GtkWidget *table, int *tx, int *ty);
void check_numbers (int number);
void removename      (void);
void menu_lav2wav    (void);
void menu_mp2enc     (void);
void menu_mpeg2enc   (void);
void menu_yuv2lav    (void);
void menu_lav2yuv    (void);
void menu_yuvdenoise (void);
void menu_yuvscaler  (void);
void menu_yuv2divx   (void);

/* Some Constant values */
#define NUMBER_MACHINE 10

/* Some Variables */
GtkWidget *enter_machname, *mlav2wav, *mmp2enc, *mlav2yuv, *mmp2enc,*mmpeg2enc;
GtkWidget *menulav2wav, *item_lav2wav, *menump2enc, *item_mp2enc,*menumpeg2enc;
GtkWidget *item_mpeg2enc, *myuv2lav, *menuyuv2lav, *item_yuv2lav;
GtkWidget *remove_mnames, *menu, *menu_item;
GtkWidget *mlav2yuv, *menulav2yuv, *item_lav2yuv;
GtkWidget *myuvdenoise, *menuyuvdenoise, *item_yuvdenoise;
GtkWidget *myuvscaler, *menuyuvscaler, *item_yuvscaler;
GtkWidget *myuv2divx, *menuyuv2divx, *item_yuv2divx;
char remove_name[LONGOPT];

struct machine tempmach;
struct machine *point;
GList *temp_mnames;

/* =============================================================== */
/* Start of the code */

/* We add the items to the "menus" and show them */
void add_item_to_menu(gchar *val)
{
  removename();
  menu_lav2wav();
  menu_mp2enc();
  menu_mpeg2enc();
  menu_yuv2lav();
  menu_lav2yuv();
  menu_yuvdenoise();
  menu_yuvscaler();
  menu_yuv2divx();

  set_up_defaults();
}

/* Here we may add the given name to the computer list */
void add_mname (GtkWidget *widget, gpointer data)
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
      {
        temp_mnames = g_list_append (temp_mnames, g_strdup(val));
        temp_mnames= g_list_first(temp_mnames);

        /* Here we can add the values to the menu */
        add_item_to_menu(g_strdup(val));
      }
  }

  if (verbose)
    {
      for(i=0; i<g_list_length(temp_mnames); i++)
        printf("Maschine = %s \n", (char*) g_list_nth_data(temp_mnames, i));
    }
}

/* Here we check if the machine number hast to be changed after a machine
 * was removed from the list */
void check_numbers (int number)
{
  if (number == tempmach.lav2wav )
    tempmach.lav2wav = 0;
  else if  ( number < tempmach.lav2wav)
    tempmach.lav2wav = tempmach.lav2wav -1;

  if (number == tempmach.mp2enc )
    tempmach.mp2enc = 0;
  else if  ( number < tempmach.mp2enc)
    tempmach.mp2enc = tempmach.mp2enc -1;

  if (number == tempmach.lav2yuv )
    tempmach.lav2yuv = 0;
  else if  ( number < tempmach.lav2yuv)
    tempmach.lav2yuv = tempmach.lav2yuv -1;

  if (number == tempmach.yuvdenoise )
    tempmach.yuvdenoise = 0;
  else if  ( number < tempmach.yuvdenoise)
    tempmach.yuvdenoise = tempmach.yuvdenoise -1;

  if (number == tempmach.yuvscaler )
    tempmach.yuvscaler = 0;
  else if  ( number < tempmach.yuvscaler)
    tempmach.yuvscaler = tempmach.yuvscaler -1;

  if (number == tempmach.mpeg2enc )
    tempmach.mpeg2enc = 0;
  else if  ( number < tempmach.mpeg2enc)
    tempmach.mpeg2enc = tempmach.mpeg2enc -1;

  if (number == tempmach.yuv2divx )
    tempmach.yuv2divx = 0;
  else if  ( number < tempmach.yuv2divx)
    tempmach.yuv2divx = tempmach.yuv2divx -1;

  if (number == tempmach.yuv2lav )
    tempmach.yuv2lav = 0;
  else if  ( number < tempmach.yuv2lav)
    tempmach.yuv2lav = tempmach.yuv2lav -1;
}

/* Now we definitly remove the name of the machine from the list. */
void remove_mname (GtkWidget *widget, gpointer data)
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

          check_numbers(i);
          removename();
          menu_lav2wav();
          menu_mp2enc();
          menu_mpeg2enc();
          menu_yuv2lav();
          menu_lav2yuv();
          menu_yuvdenoise();
          menu_yuvscaler();
          menu_yuv2divx();

          set_up_defaults();
          break;
        }
    } 
  }
}

/* Here we create the menu entries for the field where we can select the 
 * machine we want to remove */
void removename()
{
int i;

  menu = gtk_menu_new();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      menu_item = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate", set_mname,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menu), menu_item);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (remove_mnames), menu);
  gtk_widget_show_all(menu);
}

/* Here we create the menu entries for lav2wav */
void menu_lav2wav()
{
int i;

  menulav2wav = gtk_menu_new();
  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_lav2wav = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (item_lav2wav), "activate", set_lav2wav,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menulav2wav), item_lav2wav);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (mlav2wav), menulav2wav);
  gtk_widget_show_all(menulav2wav);
}

/* Here we create or recreate the menu entries for mp2enc */
void menu_mp2enc()
{
int i;

  menump2enc = gtk_menu_new();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_mp2enc = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (item_mp2enc), "activate", set_mp2enc,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menump2enc), item_mp2enc);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (mmp2enc), menump2enc);
  gtk_widget_show_all(menump2enc);
}

/* Here we create or recreate the menu entries for mpeg2enc */
void menu_mpeg2enc()
{
int i;

  menumpeg2enc = gtk_menu_new();
  
  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_mpeg2enc = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (item_mpeg2enc), "activate", set_mpeg2enc,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menumpeg2enc), item_mpeg2enc);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (mmpeg2enc), menumpeg2enc);
  gtk_widget_show_all(menumpeg2enc);
}

/* Here we create or recreate the menu entries for yuv2lav */
void menu_yuv2lav()
{
int i;

  menuyuv2lav = gtk_menu_new();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_yuv2lav = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (item_yuv2lav), "activate", set_yuv2lav,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menuyuv2lav), item_yuv2lav);
    }
  gtk_option_menu_set_menu (GTK_OPTION_MENU (myuv2lav), menuyuv2lav);
  gtk_widget_show_all(menuyuv2lav);
}

/* Here we create or recreate the menu entries for yuvdenoise */
void menu_yuvdenoise()
{
int i;

  menuyuvdenoise = gtk_menu_new ();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_yuvdenoise = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT(item_yuvdenoise),"activate",set_yuvdenoise,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menuyuvdenoise), item_yuvdenoise);
    }
  gtk_option_menu_set_menu (GTK_OPTION_MENU (myuvdenoise), menuyuvdenoise);
  gtk_widget_show_all(menuyuvdenoise);
}

/* Here we create or recreate the menu entries for yuvscaler */
void menu_yuvscaler()
{
int i;

  menuyuvscaler = gtk_menu_new();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_yuvscaler = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT(item_yuvscaler), "activate", set_yuvscaler,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menuyuvscaler), item_yuvscaler);
    }
  gtk_option_menu_set_menu (GTK_OPTION_MENU (myuvscaler), menuyuvscaler);
  gtk_widget_show_all(menuyuvscaler);
}

/* Here we create or recreate the menu entries for yuv2divx */
void menu_yuv2divx()
{
int i;

  menuyuv2divx = gtk_menu_new();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_yuv2divx = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (item_yuv2divx), "activate", set_yuv2divx,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menuyuv2divx), item_yuv2divx);
    }
  gtk_option_menu_set_menu (GTK_OPTION_MENU (myuv2divx), menuyuv2divx);
  gtk_widget_show_all(menuyuv2divx);
}


/* Here we create or recreate the menu entries for lav2yuv */
void menu_lav2yuv()
{
int i;

  menulav2yuv = gtk_menu_new ();

  for (i = 0; i < g_list_length(temp_mnames); i++)
    {
      item_lav2yuv = gtk_menu_item_new_with_label
                         ((char*) g_list_nth_data(temp_mnames, i));
      gtk_signal_connect (GTK_OBJECT (item_lav2yuv), "activate", set_lav2yuv,
                         ((char*) g_list_nth_data(temp_mnames, i)) );
      gtk_menu_append (GTK_MENU (menulav2yuv), item_lav2yuv);
    }
  gtk_option_menu_set_menu (GTK_OPTION_MENU (mlav2yuv), menulav2yuv);
  gtk_widget_show_all(menulav2yuv);


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

  printf("\n name :%s ,nummer %i \n",(char*)data, g_list_length(temp_mnames));
 
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

/* Here we set the number according to the name given for mp2enc */
void set_mp2enc (GtkWidget *menu_item, gpointer data)
{
int i;

  for (i=0 ; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0 )
      {
        tempmach.mp2enc = i;
        break;
      }
  
  if (verbose)
    printf("Set machine number for mp2enc to %i, will be: %s \n",
                    tempmach.mp2enc, (char*) g_list_nth_data(temp_mnames,i));
}

/* Same as aboth, we set the number for the mpeg2enc machine */
void set_mpeg2enc (GtkWidget *menu_item, gpointer data)
{
int i;

  for ( i=0 ; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0)
      {
        tempmach.mpeg2enc = i;
        break;
      }

  if (verbose)
    printf("Set machine number for mpeg2enc to %i, will be: %s \n",
                 tempmach.mpeg2enc, (char*) g_list_nth_data(temp_mnames,i));
}

/* See aboth, but here we set the number for the yuv2lav machine */
void set_yuv2lav (GtkWidget *menu_item, gpointer data)
{
int i;

  for ( i=0 ; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0)
      {
        tempmach.yuv2lav = i;
        break;
      }

  if (verbose)
    printf("Set machine number for yuv2lav to %i, will be: %s \n",
                 tempmach.yuv2lav, (char*) g_list_nth_data(temp_mnames,i));
}

/* See aboth, but here we set the number for the lav2yuv machine */
void set_lav2yuv (GtkWidget *menu_item, gpointer data)
{
int i;

  for ( i=0 ; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0)
      {
        tempmach.lav2yuv = i;
        break;
      }

  if (verbose)
    printf("Set machine number for lav2yuv to %i, will be: %s \n",
                 tempmach.yuv2lav, (char*) g_list_nth_data(temp_mnames,i));
}

/* See aboth, but here we set the number for the yuvdenoise machine */
void set_yuvdenoise (GtkWidget *menu_item, gpointer data)
{
int i;

  for ( i=0 ; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0)
      {
        tempmach.yuvdenoise = i;
        break;
      }

  if (verbose)
    printf("Set machine number for yuvdenoise to %i, will be: %s \n",
                 tempmach.yuvdenoise, (char*) g_list_nth_data(temp_mnames,i));
}

/* See aboth, but here we set the number for the yuvscaler machine */
void set_yuvscaler (GtkWidget *menu_item, gpointer data)
{
int i;

  for ( i=0 ; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0)
      {
        tempmach.yuvscaler = i;
        break;
      }

  if (verbose)
    printf("Set machine number for yuvscaler to %i, will be: %s \n",
                 tempmach.yuvscaler, (char*) g_list_nth_data(temp_mnames,i));
}

/* See aboth, but here we set the number for the yuv2divx machine */
void set_yuv2divx (GtkWidget *menu_item, gpointer data)
{
int i;

  for ( i=0 ; i < g_list_length(temp_mnames); i++)
    if ((strcmp(g_list_nth_data(temp_mnames,i), (char*)data)) == 0)
      {
        tempmach.yuv2divx = i;
        break;
      }

  if (verbose)
    printf("Set machine number for yuv2divx to %i, will be: %s \n",
                 tempmach.yuv2divx, (char*) g_list_nth_data(temp_mnames,i));
}

/* Here we create the layout for the machine data */
void create_machine(GtkWidget *hbox)
{
GtkWidget *label, *button_add, *table, *button_remove; 
int tx, ty; /* table size x, y */

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

  removename();

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
GtkWidget *label, *table;
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

  menu_lav2wav();

  gtk_widget_set_usize (mlav2wav, 100, -2 );
  gtk_table_attach_defaults (GTK_TABLE(table),mlav2wav, tx,tx+1, ty,ty+1);
  gtk_widget_show(mlav2wav);

  tx++;

  /* The field for the mp2enc machine */
  mmp2enc = gtk_option_menu_new ();
  
  menu_mp2enc();

  gtk_widget_set_usize (mmp2enc, 100, -2 );
  gtk_table_attach_defaults (GTK_TABLE(table),mmp2enc, tx,tx+1, ty,ty+1);
  gtk_widget_show(mmp2enc);

  gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

}

/* Here we create the menu for mpeg2enc */
void create_menu4mpeg2enc(GtkWidget *table, int *tx, int *ty)
{
  mmpeg2enc = gtk_option_menu_new ();

  menu_mpeg2enc();

  gtk_widget_set_usize (mmpeg2enc, 100, 25 );
  gtk_table_attach_defaults (GTK_TABLE(table),mmpeg2enc, *tx,*tx+1, *ty,*ty+1);
  gtk_widget_show(mmpeg2enc);
}

/* Here we create the menu for yuv2lav */
void create_menu4yuv2lav (GtkWidget *table, int *tx, int *ty)
{
  myuv2lav = gtk_option_menu_new ();

  menu_yuv2lav();

  gtk_widget_set_usize (myuv2lav, 100, 25 );
  gtk_table_attach_defaults (GTK_TABLE(table),myuv2lav, *tx,*tx+1, *ty,*ty+1);
  gtk_widget_show(myuv2lav);
}

/* Here we create the menu for lav2yuv */
void create_menu4lav2yuv (GtkWidget *table, int *tx, int *ty)
{
  mlav2yuv = gtk_option_menu_new ();

  menu_lav2yuv();

  gtk_widget_set_usize (mlav2yuv, 100, 25);
  gtk_table_attach_defaults (GTK_TABLE(table),mlav2yuv, *tx,*tx+1, *ty,*ty+1);
  gtk_widget_show (mlav2yuv);
}

/* Here we create the menu for yuvdenoise */
void create_menu4yuvdenoise (GtkWidget *table, int *tx, int *ty)
{ 
  myuvdenoise = gtk_option_menu_new ();
  
  menu_yuvdenoise();
  
  gtk_widget_set_usize (myuvdenoise, 100, 25);
  gtk_table_attach_defaults(GTK_TABLE(table),myuvdenoise, *tx,*tx+1, *ty,*ty+1);
  gtk_widget_show (myuvdenoise);
}

/* Here we create the menu for yuvscaler */
void create_menu4yuvscaler (GtkWidget *table, int *tx, int *ty)
{ 
  myuvscaler = gtk_option_menu_new ();
  
  menu_yuvscaler();
  
  gtk_widget_set_usize (myuvscaler, 100, 25);
  gtk_table_attach_defaults (GTK_TABLE(table),myuvscaler, *tx,*tx+1, *ty,*ty+1);
  gtk_widget_show (myuvscaler);
}

/* Here we create the menu for yuv2divx */
void create_menu4yuv2divx (GtkWidget *table, int *tx, int *ty)
{ 
  myuv2divx = gtk_option_menu_new ();
  
  menu_yuv2divx();
  
  gtk_widget_set_usize (myuv2divx, 100, 25);
  gtk_table_attach_defaults (GTK_TABLE(table),myuv2divx, *tx,*tx+1, *ty,*ty+1);
  gtk_widget_show (myuv2divx);
}


/* Here we create the layout for the vide encoding part */
void create_video (GtkWidget *hbox)
{
GtkWidget *label, *table;
int i, tx,ty; /* table size x, y*/

tx=4;
ty=6;

  table = gtk_table_new (ty,ty, FALSE);

  for (i = 0; i < tx; i++)  
    gtk_table_set_col_spacing (GTK_TABLE (table), i, 15);

  tx=0;
  ty=0;

  tx=3;
  label = gtk_label_new ("| mpeg2enc");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  create_menu4mpeg2enc(table, &tx, &ty);
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

  tx=0; /* return to the first field (or first colum) */
  ty++; /* of the next row */
  
  create_menu4lav2yuv(table, &tx, &ty); 
  tx++;

  create_menu4yuvdenoise (table, &tx, &ty);
  tx++;

  create_menu4yuvscaler (table, &tx, &ty);
  tx++;

  create_menu4yuv2divx (table, &tx, &ty);
 
  ty++;
  tx=3;

  label = gtk_label_new ("| yuv2lav  ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, tx, tx+1, ty, ty+1);
  gtk_widget_show(label);
  ty++;

  create_menu4yuv2lav(table, &tx, &ty); 

  gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, FALSE, 0);
  gtk_widget_show (table);
}

/* Here we copy the original values into temp. Values */
void work_values(gpointer data)
{
int i;

  printf("\n Wert von enh:%i und data: \"%s\"\n",enhanced_settings,(char*)data);

  point = &machine4mpeg1; /* fallback should never be used ;) */

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

  if (verbose)
    {
      printf("Machine for lav2wav %i,  for mp2enc %i,  for lav2yuv %i, \
      for yuvdenoise %i \n", tempmach.lav2wav, tempmach.mp2enc,
          tempmach.lav2yuv, tempmach.yuvdenoise);
      printf("Machine for yuvscaler %i,  for mpeg2enc %i,  for yuv2divx %i, \
      for yuv2lav %i \n", tempmach.yuvscaler, tempmach.mpeg2enc,
          tempmach.yuv2divx, tempmach.yuv2lav);
    }

}

/* Here we set up the fields with the default values or previous save one */
void set_up_defaults()
{
  gtk_option_menu_set_history (GTK_OPTION_MENU (mlav2wav), tempmach.lav2wav);
  gtk_option_menu_set_history (GTK_OPTION_MENU (mmp2enc), tempmach.mp2enc);
  gtk_option_menu_set_history (GTK_OPTION_MENU (mmpeg2enc), tempmach.mpeg2enc);
  gtk_option_menu_set_history (GTK_OPTION_MENU (myuv2lav), tempmach.yuv2lav);
  gtk_option_menu_set_history (GTK_OPTION_MENU (mlav2yuv), tempmach.lav2yuv);
  gtk_option_menu_set_history(GTK_OPTION_MENU(myuvdenoise),tempmach.yuvdenoise);
  gtk_option_menu_set_history (GTK_OPTION_MENU (myuvscaler),tempmach.yuvscaler);
  gtk_option_menu_set_history (GTK_OPTION_MENU (myuv2divx), tempmach.yuv2divx);
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

  set_up_defaults();
}
