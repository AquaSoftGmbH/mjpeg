/*
 *  fade.flt - reads a stream from stdin and fades in/out from/to
 *             black/white. Writes to stadout
 *
 *  Copyright (C) 2004, Michael Hanke
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>

#include "wave_io.h"

#define PFPS 25.0
#define NFPS 3000.0/100.1

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

static void usage () {

   fprintf (stderr, "usage:  fade_s.flt [params]\n"
                    "params: -s [num] - frame for starting fade in/out [0]\n"
                    "        -d [num] - duration of transition in frames (REQUIRED)\n"
                    "        -n       - video norm [p]\n"
                    "             p   - PAL (or SECAM)\n"
                    "             n   - NTSC\n"
                    "        -t [num] - type of transition\n"
	            "             1   - fade out to black\n"
	            "             2   - fade in from black\n"
	            "             3   - fade out to white\n"
 	            "             4   - fade in from white\n"
                    "        -v [num] - verbosity [0..2]\n"
                    "        -h       - print out this usage information\n");

}

int main (int argc, char *argv[])
{
   int verbose = 1;        /* verbosity */
   int type = 1;            /* type of transition */
   int start = 0;           /* start frame */
   int duration = 0;        /* duration of fading */
   int norm = 0;            /* video norm */
   int i, starts, durations, inout, sa, nsamp;
   float samples[2];
   double facc, fps, logbase, mx;
   wave_header_t *header;
   FILE *fpin, *fpout;

   while ((i = getopt(argc, argv, "v:s:d:n:t:h")) != -1) {
      switch (i) {
      case 'h':
         usage();
         exit(1);
         break;
      case 'v':
         verbose = atoi (optarg);
         if( verbose < 0 || verbose >2 ) {
            usage ();
            exit (1);
         }
         break;		  
      case 's':
         start = atoi (optarg);
         if (start < 0) {
	     fprintf(stderr, "Error: start < 0\n");
	     exit(1);
	 }
         break;
      case 'd':
         duration = atoi (optarg);
         if (duration <= 0) {
	     fprintf(stderr, "Error: duration <= 0\n");
	     exit(1);
	 }
         break;
      case 'n':
	 norm = (tolower(optarg[0]) == 'p');
	 break;
      case 't':
         type = atoi (optarg);
         if (type < 0 || type > 4) {
	     fprintf(stderr, "Error: type out of range\n");
	     exit(1);
	 }
         break;
      }
   }

   if (duration <= 0) {
      fprintf(stderr, "Error: duration <= 0\n");
      usage();
      exit(1);
   }
   fps = norm ? PFPS : NFPS;
   inout = type & 1;

   fpin = read_wave_header(&header, 0); /* stdin */
   if (fpin == NULL) exit(1);

   fpout = write_wave_header(header, 1); /* stdout */
   if (fpout == NULL) exit(1);

   starts = (start*header->SampleRate)/fps;
   durations = (duration*header->SampleRate)/fps; /* Hopefully, no overflow */
   mx = header->BitsPerSample == 8 ? 128.0 : 32768.0;
   logbase = -log(mx)/durations;

   for (sa = 0; sa < starts; sa++) {
      i = read_sample(header, fpin, samples);
      if (i != 0) {
	  fprintf(stderr, "Error: Unexpected end of input stream\n");
	  exit(1);
      }
      if (inout == 0) samples[0] = samples[1] = 0.0; /* silence */
      write_sample (header, fpout, samples);
      if (i != 0) {
	  fprintf(stderr, "Error: Write error\n");
	  exit(1);
      }
   }
   nsamp = starts;

   for (sa = 0; sa < durations; sa++) {
      i = read_sample(header, fpin, samples);
      if (i != 0) {
	  nsamp += sa;
	  close_wave_output(header, fpout, nsamp);
	  return 0; /* Maybe, a simple rounding error */
      }
      facc = exp(logbase*sa);
      if (inout == 0) facc = MIN(1.0/(facc*mx),1.0);
      samples[0] = facc*samples[0];
      samples[1] = facc*samples[1];
      write_sample (header, fpout, samples);
      if (i != 0) {
	  fprintf(stderr, "Error: Write error\n");
	  exit(1);
      }
   }
   nsamp += durations;

   for (;;nsamp++) {
      i = read_sample(header, fpin, samples);
      if (i != 0) {
	  close_wave_output(header, fpout, nsamp);
	  return 0;
      }
      if (inout) samples[0] = samples[1] = 0.0; /* silence */
      write_sample (header, fpout, samples);
      if (i != 0) {
	  fprintf(stderr, "Error: Write error\n");
	  exit(1);
      }
   }

   return 0;
}
