/*
    wav_io: Utilities for dealing with WAV files

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/*
   wav_read_header: Read and analyze the header of a WAV file.

   The file must be positioned at start before the call

   The file is positioned at the start of the audio data after the call.

   This program is written so that it can be used on big and little endian
   machines and that it can be used on a pipe (no seeks)


   returns 0 on success, -1 on error
*/

static int hdr_len = 0;

static unsigned long getulong(unsigned char *data)
{
   /* Interprets data as a little endian number */

   return (data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24));
}

static unsigned long find_tag(FILE *fd, char *tag)
{
   unsigned char data[8];
   unsigned long m;
   int n;

   /* scan input stream for specified tag and return its length */
   /* returns -1 in case of error */

   while(1)
   {
      n = fread(data,1,8,fd);
      if(n!=8) goto error;

      hdr_len += 8;

      m = getulong(data+4);
      if(!strncasecmp(data,tag,4)) return m;

      for(n=0;n<m;n++)
         if(fgetc(fd)==EOF) goto error;

      hdr_len += m;
   }

error:
   fprintf(stderr,"EOF in WAV header when searching for tag \"%s\"\n",tag);
   return -1;
}

int wav_read_header(FILE *fd, int *rate, int *chans, int *bits,
                    int *format, unsigned long *bytes)
{
   unsigned long riff_len, fmt_len, m;
   unsigned char data[16];
   int n;

   n = fread(data,1,12,fd);
   if(n!=12) {
      fprintf(stderr,"EOF in WAV header\n");
      return -1;
   }

   if(strncasecmp(data,"RIFF",4)) {
      fprintf(stderr,"Input not a WAV file - starts not with \"RIFF\"\n");
      return -1;
   }

   riff_len = getulong(data+4);

   if(strncasecmp(data+8,"WAVE",4)) {
      fprintf(stderr,"Input not a WAV file - has no \"WAVE\" tag\n");
      return -1;
   }

   hdr_len = 12;

   fmt_len = find_tag(fd,"fmt ");
   if(fmt_len<0) return -1;
   if(fmt_len&1) fmt_len++;
   if(fmt_len<16) {
      fprintf(stderr,"WAV format len %ld too short\n",fmt_len);
      return -1;
   }

   n = fread(data,1,16,fd);
   if(n!=16) {
      fprintf(stderr,"EOF in WAV header\n");
      return -1;
   }

   m = getulong(data);        /* wFormatTag + nChannels */
   *format = m&0xffff;
   *chans  = (m>>16) & 0xffff;

   *rate = getulong(data+4);  /* nSamplesPerSec */

   m = getulong(data+8);      /* nAvgBytesPerSec */

   m = getulong(data+12);     /* nBlockAlign + wBitsPerSample */
   *bits = (m>>16) & 0xffff;

   /* skip over remaining bytes in format (if any) */

   for(n=16;n<fmt_len;n++) fgetc(fd);

   hdr_len += fmt_len;

   /* Get data len */

   *bytes = find_tag(fd,"data");
   if(*bytes<0) return -1;

   if(*bytes+hdr_len != riff_len+8) {
      fprintf(stderr,"Warning:\n");
      fprintf(stderr,"File length according data tag: %lu\n",*bytes+hdr_len);
      fprintf(stderr,"File length according RIFF tag: %lu\n",riff_len+8);
   }

   return 0;
}
