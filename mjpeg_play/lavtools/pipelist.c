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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pipelist.h"

extern int verbose;

int open_pipe_list (char *name, PipeList *pl)
{
   FILE *fd;
   char  line[1024];
   int   i, j, n;

   fd = fopen (name, "r");

   fgets (line, 1024, fd);
   if (strcmp (line, "LAV Pipe List\n") == 0) {

      /* 1. video norm */

      fgets (line, 1024, fd);
      if(line[0]!='N' && line[0]!='n' && line[0]!='P' && line[0]!='p')
      {
         fprintf(stderr,"Pipe list second line is not NTSC/PAL\n");
         exit (1);
      }
      if (verbose > 1)
        fprintf(stderr,"Pipe list norm is %s\n",line[0]=='N'||line[0]=='n'?"NTSC":"PAL");
      if(line[0]=='N'||line[0]=='n')
         pl->video_norm = 'n';
      else
         pl->video_norm = 'p';

      /* 2. input streams */

      fgets (line, 1024, fd);
      if (sscanf (line, "%d", &(pl->input_num)) != 1) {
         fprintf (stderr, "pipelist: # of input streams expected, \"%s\" found\n", line);
         return -1;
      }
      if (verbose > 1)
        fprintf (stderr, "Pipe list contains %d input streams\n", pl->input_num);

      pl->input_cmd = (char **) malloc (pl->input_num * sizeof (char *));

      for (i=0; i<pl->input_num; i++) {
         fgets(line,1024,fd);
         n = strlen(line);
         if(line[n-1]!='\n') {
            fprintf(stderr,"Input cmdline in pipe list too long\n");
            exit(1);
         }
         line[n-1] = 0; /* Get rid of \n at end */
         pl->input_cmd[i] = (char *) malloc (n);
         strncpy (pl->input_cmd[i], line, n);
      }
      
      /* 3. sequences */
      
      pl->frame_num = 0;
      pl->seq_num = 0;
      pl->seq_ptr = (PipeSequence **) malloc (32 * sizeof (PipeSequence *));
      while (fgets (line, 1024, fd)) {

         PipeSequence *seq = (PipeSequence *) malloc (sizeof (PipeSequence));
         
         /* 3.1. frames in sequence */

         if (sscanf (line, "%d", &(seq->frame_num)) != 1) {
            fprintf (stderr, "pipelist: # of frames in sequence expected, \"%s\" found\n", line);
            return -1;
         }
         if (seq->frame_num < 1) {
            fprintf (stderr, "Pipe list contains sequence of length < 1 frame\n");
            exit (1);
         }
         if (verbose > 1)
            fprintf (stderr, "Pipe list sequence %d contains %d frames\n", pl->seq_num, seq->frame_num);
         pl->frame_num += seq->frame_num;
         
         /* 3.2. input streams */
     
         n = !fgets (line, 1024, fd);
         if (sscanf (line, "%d", &(seq->input_num)) != 1) {
            fprintf (stderr, "pipelist: # of streams in sequence expected, \"%s\" found\n", line);
            return -1;
         }
         seq->input_ptr = (int *) malloc (seq->input_num * sizeof (int));
         seq->input_ofs = (unsigned long *) malloc (seq->input_num * sizeof (unsigned long));
         for (i=0; i<seq->input_num; i++) {
            if (!fgets (line, 1024, fd)) n++;
            j = sscanf (line, "%d %lud", &(seq->input_ptr[i]), &(seq->input_ofs[i]));
            if (j == 1) {
               /* if no offset is given, assume ofs = 0 */
               seq->input_ofs[i] = 0;
               j++;
            }
            if (j != 2) {
               fprintf (stderr, "pipelist: input stream index & offset expected, \"%s\" found\n", line);
               return -1;
            }
            if (seq->input_ptr[i] >= pl->input_num) {
               fprintf (stderr, "Sequence requests input stream that is not contained in pipe list\n");
               exit (1);
            }
         }

         /* 3.3. output cmd */

         fgets (line, 1024, fd);
         if (n > 0) {
            fprintf (stderr, "Error in pipe list: Unexpected end\n");
            fprintf (stderr, "\"%s\"\n", line);
            exit (1);
         }
         n = strlen(line);
         if(line[n-1]!='\n') {
            fprintf(stderr,"Output cmdline in pipe list too long\n");
            exit(1);
         }
         line[n-1] = 0; /* Get rid of \n at end */
         seq->output_cmd = (char *) malloc (n);
         strncpy (seq->output_cmd, line, n);
         
         pl->seq_ptr[pl->seq_num++] = seq;
         if ((pl->seq_num % 32) == 0)
            pl->seq_ptr = (PipeSequence **) realloc (pl->seq_ptr, sizeof (pl->seq_ptr) + 32 * sizeof (PipeSequence *));
      }
      return 0;
   }
   /* errno = EBADMSG; */
   return -1;
}

int write_pipe_list (char *name, PipeList *pl)
{
   FILE *fd;
   int   i, n;

   fd = fopen (name, "w");

   fprintf (fd, "LAV Pipe List\n");

   /* 1. video norm */

   fprintf (fd, "%s\n", (pl->video_norm == 'n') ? "NTSC" : "PAL");

   /* 2. input streams */

   fprintf (fd, "%d\n", pl->input_num);

   for (i=0; i<pl->input_num; i++)
      fprintf (fd, "%s\n", pl->input_cmd[i]);

   /* 3. sequences */

   for (i=0; i<pl->seq_num; i++) {
      
      PipeSequence *seq = pl->seq_ptr[i];

      /* 3.1. frames in sequence */
      
      fprintf (fd, "%d\n", seq->frame_num);

      /* 3.2. input streams */
      
      fprintf (fd, "%d\n", seq->input_num);
      for (n=0; n<seq->input_num; n++)
         fprintf (fd, "%d %lud\n", seq->input_ptr[n], seq->input_ofs[n]);

      /* 3.3. output cmd */
      
      fprintf (fd, "%s\n", seq->output_cmd);
   }
   return 0;

}
