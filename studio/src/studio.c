/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
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
#include <gdk/gdkx.h>
#include <pthread.h>
#include <signal.h>

#include "gtktvplug.h"
#include "studio.h"
#include "pipes.h"
#include "gtkfunctions.h"

#include "linux_studio_logo.xpm"
#include "cam.xpm"
#include "tv.xpm"
#include "arrows.xpm"
#include "editing.xpm"

/* variables */
GtkWidget *window, *notebook;

/* forward declarations of functions */
void notebook_page_switched( GtkWidget *widget, gpointer data );
gint delete_event( GtkWidget *widget, GdkEvent *event, gpointer data );
void destroy( GtkWidget *widget, gpointer data );
void signal_handler(int signal_type);
void exit_func(GtkWidget *widget, gpointer data);
void open_about_menu(GtkWidget *widget, gpointer data);
void open_help_menu(GtkWidget *widget, gpointer data);
void usage(void);
void version(void);
void make_menus(GtkWidget *box);
void command_line_options( int argc, char *argv[] );

/* ================================================================= */

void notebook_page_switched( GtkWidget *widget, gpointer data )
{
	if (verbose) g_print("\nNow, you're on page %d\n", gtk_notebook_get_current_page(GTK_NOTEBOOK(widget)));
}

gint delete_event( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	g_print ("Thanks for using Linux Video Studio.\n");
	return(FALSE);
}

void destroy( GtkWidget *widget, gpointer data )
{
	int i;

	if (verbose) g_print ("Quitting...\n");

	/* this closes all apps if opened */
	for (i=0;i<NUM;i++)
	{
		close_pipe(i);
	}

	gtk_main_quit();
}

void signal_handler(int signal_type)
{
	int i;

	if (signal_type == SIGSEGV)
	{
		printf("*** WARNING *** - Got segmentation fault (SIGSEGV)\n");
		printf("\nPlease mail a bug report to rbultje@ronald.bitfreak.net or\n"
			"mjpeg-users@lists.sourceforge.net containing the situation\n"
			"in which this segfault occurred, gdb-output (+backtrace)\n"
			"and other possibly noteworthy information\n");
		printf("Trying to shut down lav-applications (if running): ");
	}
	else if (signal_type == SIGTERM)
	{
		printf("*** WARNING *** - Got termination signal (SIGTERM)\n");
		printf("Trying to shut down lav-applications (if running): ");
	}
	else if (signal_type == SIGINT)
	{
		printf("*** WARNING *** - Got interrupt signal (SIGINT)\n");
		printf("Trying to shut down lav-applications (if running): ");
	}

	for (i=0;i<NUM;i++)
		close_pipe(i);
	printf("succeeded\n");
	signal(signal_type, SIG_DFL);
}

void exit_func(GtkWidget *widget, gpointer data)
{
	if (! delete_event(widget, NULL, data))
		destroy(widget, data);
}

void open_about_menu(GtkWidget *widget, gpointer data)
{
	GtkWidget *options_window, *button, *vbox, *label, *pixmap_w;
	GdkPixmap *pixmap;
	GtkStyle *style;

	options_window = gtk_window_new(GTK_WINDOW_DIALOG);

	style = gtk_style_new();
	style->bg->red = 65535;
	style->bg->blue = 65535;
	style->bg->green = 65535;
	gtk_widget_push_style(style);
	gtk_widget_set_style(GTK_WIDGET(options_window), style);
	gtk_widget_pop_style();

	vbox = gtk_vbox_new (FALSE, 10);

	gtk_window_set_title (GTK_WINDOW(options_window), "Linux Video Studio - About");
	gtk_container_set_border_width (GTK_CONTAINER (options_window), 15);
	gtk_widget_show(options_window);

	pixmap = gdk_pixmap_create_from_xpm_d(options_window->window, NULL, NULL, linux_studio_logo_xpm);
	pixmap_w = gtk_pixmap_new(pixmap, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), pixmap_w, FALSE, FALSE, 0);
	gtk_widget_show(pixmap_w);

	label = gtk_label_new("Linux Video Studio " VERSION "\n\n" \
		"Created by:\nRonald Bultje <rbultje@ronald.bitfreak.net>\n" \
		"Bernhard Praschinger <praschinger@netway.at>\n\n" \
		"Images by Laurens Buhler <law@nixhelp.org>\n");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	button = gtk_button_new_with_label("  Close  ");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(options_window));
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_container_add (GTK_CONTAINER (options_window), vbox);
	gtk_widget_show(vbox);
}

void open_help_menu(GtkWidget *widget, gpointer data)
{
	GtkWidget *options_window, *button, *vbox, *label;

	options_window = gtk_window_new(GTK_WINDOW_DIALOG);
	vbox = gtk_vbox_new (FALSE, 10);

	gtk_window_set_title (GTK_WINDOW(options_window), "Linux Video Studio - Help");
	gtk_container_set_border_width (GTK_CONTAINER (options_window), 15);
	gtk_widget_show(options_window);

	label = gtk_label_new("Linux Video Studio " VERSION "\n\n" \
		"For the most recent FAQ, please visit:\n" \
		"http://ronald.bitfreak.net/help.shtml\n\n" \
		"For bug reports, questions, suggestions and\n" \
		"comments, please send an e-mail to:\n" \
		"<mjpeg-users@lists.sourceforge.net>\n\n" \
		"Or contact the developers individually:\n" \
		"Ronald Bultje <rbultje@ronald.bitfreak.net>\n" \
		"Bernhard Praschinger <praschinger@netway.at>");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	button = gtk_button_new_with_label("  Close  ");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(options_window));
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_container_add (GTK_CONTAINER (options_window), vbox);
	gtk_widget_show(vbox);
}

void usage()
{
	g_print("                  Linux Video Studio options\n");
	g_print("===============================================================\n");
	g_print("-h        : this help message\n");
	g_print("-v        : version information\n");
	g_print("-p <num>  : port to be used for Xvideo extension (0=autodetect)\n");
/*	g_print("-s <n>x<n>: specify preferred tv-plug size (<width>x<height>)\n");*/
	g_print("-t        : probe ports to select the port to use for Xvideo\n");
	g_print("-d        : debug/verbose\n");
	g_print("-c <name> : specify configuration name (default: studio)\n");
	g_print("===============================================================\n");
	g_print("               (c) Ronald Bultje, 2000-2001 (GPL)\n");
	exit(1);
}

void version()
{
	g_print("Linux Video Studio " VERSION "\n");
	exit(1);
}

void make_menus(GtkWidget *box)
{
	GtkWidget *menu, *menu_bar, *root_menu, *menu_items;

	/* FILE menu */
	menu = gtk_menu_new ();
	menu_items = gtk_menu_item_new_with_label ("Exit");
	gtk_menu_append (GTK_MENU (menu), menu_items);
	gtk_signal_connect_object (GTK_OBJECT (menu_items), "activate",
		GTK_SIGNAL_FUNC(exit_func), NULL);
	gtk_widget_show(menu_items);
	root_menu = gtk_menu_item_new_with_label ("File");
	gtk_widget_show (root_menu);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

	menu_bar = gtk_menu_bar_new ();
	gtk_box_pack_start (GTK_BOX (box), menu_bar, FALSE, FALSE, 0);
	gtk_widget_show (menu_bar);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), root_menu);

	/* OPTIONS menu */
	menu = gtk_menu_new ();
	menu_items = gtk_menu_item_new_with_label ("Options");
	gtk_menu_append (GTK_MENU (menu), menu_items);
	gtk_signal_connect_object (GTK_OBJECT (menu_items), "activate",
		GTK_SIGNAL_FUNC(open_options_window), NULL);
	gtk_widget_show(menu_items);
	root_menu = gtk_menu_item_new_with_label ("Options");
	gtk_widget_show (root_menu);

	menu_items = gtk_menu_item_new_with_label ("Encoding Options");
	gtk_menu_append (GTK_MENU (menu), menu_items);
	gtk_signal_connect_object (GTK_OBJECT (menu_items), "activate",
		GTK_SIGNAL_FUNC(open_encoptions_window), NULL);
	gtk_widget_show(menu_items);
	root_menu = gtk_menu_item_new_with_label ("Options");
	gtk_widget_show (root_menu);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), root_menu);

	/* HELP menu */
	menu = gtk_menu_new ();
	menu_items = gtk_menu_item_new_with_label ("About");
	gtk_menu_append (GTK_MENU (menu), menu_items);
	gtk_signal_connect_object (GTK_OBJECT (menu_items), "activate",
		GTK_SIGNAL_FUNC(open_about_menu), NULL);
	gtk_widget_show(menu_items);
	menu_items = gtk_menu_item_new_with_label ("Help");
	gtk_menu_append (GTK_MENU (menu), menu_items);
	gtk_signal_connect_object (GTK_OBJECT (menu_items), "activate",
		GTK_SIGNAL_FUNC(open_help_menu), NULL);
	gtk_widget_show(menu_items);

	root_menu = gtk_menu_item_new_with_label ("Help");
	gtk_widget_show (root_menu);
	//gtk_menu_item_right_justify(GTK_MENU_ITEM(root_menu)); //(for later)
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), root_menu);
}

void command_line_options( int argc, char *argv[] )
{
	int n, changed, tmp_port; //, sizechanged,w,h;
	char *optstr = NULL; //, *sizetmp;

	verbose = FALSE;
	//sizechanged = 0;
	changed = 0;
	tmp_port = 0;
	
	while( (n=getopt(argc,argv,"p:c:s:hvdt")) != EOF)
	{
		switch(n)
		{
			case 'h':
				usage();
				break;

			case 'v':
				version();
				break;
			case 'p':
				tmp_port = atoi(optarg);
				break;
			case 'c':
				if (strlen(optarg) != 0)
				{
					optstr = strdup(optarg);
					changed = 1;
				}
				break;
/*			case 's':
				if (strlen(optarg) != 0)
				{
					sizetmp = strdup(optarg);
					if (2 != sscanf(sizetmp,"%dx%d",&w,&h))
						g_print("Wrong values for width/height\n");
					else
						sizechanged = 1;
				}
				break;*/
			case 't':
				tmp_port = -1;
				break;
			case 'd':
				verbose = TRUE;
				break;
		}
	}
	if (changed == 1)
	{
		sprintf(studioconfigfile, "%s.conf", optstr);
		sprintf(editlist_filename, "%s_editlist.eli",optstr);
	}
	else
	{
		sprintf(studioconfigfile, "studio.conf");
		sprintf(editlist_filename, "studio_editlist.eli");
	}

	if (verbose && changed) g_print("Configuration Filename: %s\n", studioconfigfile);

	/* Load basic configuration */
	load_config();
	if (tmp_port != 0)
	{
		port = tmp_port;
		save_config();
	}
/*	if (sizechanged != 0)
	{
		tv_width = w;
		tv_height = h;
		save_config();
	}
	else if (tv_width == 0 && tv_height == 0)
	{
		tv_width = 192;
		tv_height = 144;
	}*/
}

int main( int argc, char *argv[] )
{
	GtkWidget *mainbox, *hbox, *hbox2, *label,*pixmap_w;
/*	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkStyle *style;*/

	command_line_options(argc, argv);

	g_thread_init(NULL);
	gtk_init(&argc, &argv);
	gdk_rgb_init ();

/*	capturing_init = FALSE;
	playing = FALSE;*/

	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy", GTK_SIGNAL_FUNC (destroy), NULL);

	gtk_window_set_title (GTK_WINDOW (window), "Linux Video Studio");
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);

	mainbox = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (window), mainbox);
	gtk_widget_show (mainbox);

	make_menus(mainbox);

	notebook = gtk_notebook_new ();

/*	style = gtk_widget_get_style(window);*/
	gtk_widget_realize(window);
	hbox = create_lavrec_layout(window);
	if (hbox != NULL)
	{
		hbox2 = gtk_hbox_new(FALSE, 10);
/*		pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
			&style->bg[GTK_STATE_NORMAL], cam_xpm);
		pixmap_w = gtk_pixmap_new(pixmap, mask);*/
		pixmap_w = gtk_widget_from_xpm_data(cam_xpm);
		gtk_widget_show(pixmap_w);
		gtk_box_pack_start(GTK_BOX(hbox2), pixmap_w, FALSE, FALSE, 0);
		label = gtk_label_new("Capture Video");
		gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook), hbox, hbox2);
		gtk_widget_show(hbox);
		gtk_widget_show(hbox2);
	}

	hbox = create_lavedit_layout(window);
	hbox2 = gtk_hbox_new(FALSE, 10);
/*	pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask, &style->bg[GTK_STATE_NORMAL], editing_xpm);
	pixmap_w = gtk_pixmap_new(pixmap, mask);*/
	pixmap_w = gtk_widget_from_xpm_data(editing_xpm);
	gtk_widget_show(pixmap_w);
	gtk_box_pack_start(GTK_BOX(hbox2), pixmap_w, FALSE, FALSE, 0);
	label = gtk_label_new("Edit Video");
	gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), hbox, hbox2);
	gtk_widget_show(hbox);
	gtk_widget_show(hbox2);

	hbox = create_lavplay_layout();
	if (hbox != NULL)
	{
		hbox2 = gtk_hbox_new(FALSE, 10);
/*		pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
			&style->bg[GTK_STATE_NORMAL], tv_xpm);
		pixmap_w = gtk_pixmap_new(pixmap, mask);*/
		pixmap_w = gtk_widget_from_xpm_data(tv_xpm);
		gtk_widget_show(pixmap_w);
		gtk_box_pack_start(GTK_BOX(hbox2), pixmap_w, FALSE, FALSE, 0);
		label = gtk_label_new("Playback Video");
		gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook), hbox, hbox2);
		gtk_widget_show(hbox);
		gtk_widget_show(hbox2);
	}

        /* New notebook entry for encoding */
	hbox = create_lavencode_layout(); 
	hbox2 = gtk_hbox_new(FALSE, 10);
/*	pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
		&style->bg[GTK_STATE_NORMAL], arrows_xpm);
	pixmap_w = gtk_pixmap_new(pixmap, mask);*/
	pixmap_w = gtk_widget_from_xpm_data(arrows_xpm);
	gtk_widget_show(pixmap_w);
	gtk_box_pack_start(GTK_BOX(hbox2), pixmap_w, FALSE, FALSE, 0);
	label = gtk_label_new("Encode Video");
	gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), hbox, hbox2);
	gtk_widget_show(hbox);
	gtk_widget_show(hbox2);

	hbox = gtk_hbox_new(FALSE,5);
	gtk_box_pack_start(GTK_BOX(hbox), notebook, FALSE, FALSE, 5);
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(mainbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	gtk_signal_connect(GTK_OBJECT(notebook), "switch_page", GTK_SIGNAL_FUNC(notebook_page_switched),NULL);

	gtk_widget_show(window);
	if (verbose) g_print("Linux Video Studio initialized correctly\n");

	init_pipes(); /* set all data to 0 */

	gdk_threads_enter();
	gtk_main ();
	gdk_threads_leave();

	return(0);
}
