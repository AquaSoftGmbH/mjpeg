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

#ifndef __HAVE_LIRC_H_
#define __HAVE_LIRC_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

char *lirc_dev /*= NULL*/;
char *remote_buttons[RC_NUM_KEYS];
GtkWidget *rc_entry[RC_NUM_KEYS];

gint lirc_init(void);
GtkWidget *get_rc_button_selection_notebook_page(void);
void remote_button_action(gint button_num);

#endif

#endif /* __HAVE_LIRC_H_ */
