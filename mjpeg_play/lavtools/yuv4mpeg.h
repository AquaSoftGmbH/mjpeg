/*
 *  yuv4mpeg.h: reading to and writing from YUV4MPEG streams
 *              possibly part of lavtools in near future ;-)
 *
 *  Copyright (C) 2001 pHilipp Zabel  <pzabel@gmx.de>
 *
 *  based on code from mpeg2enc and lav2yuv
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __YUV4MPEG_H__
#define __YUV4MPEG_H__

size_t piperead(int fd, char *buf, size_t len);
size_t pipewrite(int fd, char *buf, size_t len);

int yuv_read_header (int fd_in, int *horizontal_size, int *vertical_size, int *frame_rate_code);
int yuv_read_frame(int fd_in, unsigned char *yuv[3], int width, int height);

void yuv_write_header (int fd, int width, int height, int frame_rate_code);
void yuv_write_frame  (int fd, unsigned char *yuv[3], int width, int height);

int yuv_fps2mpegcode (double fps);
double yuv_mpegcode2fps (unsigned int code);

#endif