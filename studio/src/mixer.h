/*
 * mixer.h, public header for mixer.c
 *
 * Copyright (C) 1997 Rasca Gmelch, Berlin
 * EMail: thron@gmx.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

int mixer_init (char *dev_file);	/* returns mixer_id */
int mixer_fini (int mixer_id);

const char *mixer_get_name (int mixer_id, int dev);
const char *mixer_get_label(int mixer_id, int dev);

int mixer_is_stereo (int mixer_id, int dev);
int mixer_get_vol_left (int mixer_id, int dev);
int mixer_get_vol_right(int mixer_id, int dev);
int mixer_set_vol_left (int mixer_id, int dev, int val); /* val:[0-100] */
int mixer_set_vol_right(int mixer_id, int dev, int val); /* val:[0-100] */
int mixer_is_rec_dev(int mixer_id, int dev);
int mixer_is_rec_on (int mixer_id, int dev);
int mixer_set_rec (int mixer_id, int dev, int val);		/* val:[0|1] */
int mixer_num_of_devs (int mixer_id);
int mixer_get_dev_by_name (int mixer_id, char *dev_name);
int mixer_exclusive_input (int mixer_id);
