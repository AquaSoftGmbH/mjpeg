/*
 *  pipelist.[ch] - provides two functions to read / write pipe
 *                  list files, the "recipes" for lavpipe
 *
 *  Copyright (C) 2001, pHilipp Zabel <pzabel@gmx.de>
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
#ifndef __PIPELIST_H__
#define __PIPELIST_H__

typedef struct {
   int            frame_num;
   int            input_num;
   int           *input_ptr;
   unsigned long *input_ofs;
   char          *output_cmd;
} PipeSequence;

typedef struct {
   char           video_norm;
   int            frame_num;
   int            input_num;
   char         **input_cmd;
   int            seq_num;
   PipeSequence **seq_ptr;
} PipeList;

int open_pipe_list (char *name, PipeList *pl);
int write_pipe_list (char *name, PipeList *pl);

#endif

