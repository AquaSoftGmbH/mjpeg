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

#ifndef __HAVE_CHANNELS_H_
#define __HAVE_CHANNELS_H_

typedef struct {
   int  frequency;        /* channel frequency         */
   char name[256];        /* name given to the channel */
} Channel;

Channel **channels /*= NULL*/; /* make it NULL-terminated PLEASE! */
gint current_channel /*=-1*/, previous_channel /*=-1*/;
GtkWidget *channelcombo /*=NULL*/;


GtkWidget *get_channel_list_notebook_page(GtkWidget *channellist);

void goto_previous_channel(void);
void increase_channel(int num);
void goto_channel(int num);
void set_timeout_goto_channel(int num);

#endif /* __HAVE_CHANNELS_H_ */
