/* PGM reader c/o "Rene K. Mueller" <kiwi@the-labs.com> */

#include <stdlib.h>
#include <stdio.h>
#include "RTjpeg.h"
#include "format.h"

void rgb_yuv(int w, int h, unsigned char *rgb, unsigned char *buf) {
  register int i,j; char *off = buf+w*h, *off2 = buf+w*h+w*h/4;
  for(j=0; j<h; j++) {
    for(i=0; i<w; i++) {
      int b = *rgb++;
      int g = *rgb++;
      int r = *rgb++;
      int y,cb,cr;
      y  =  (0.257 * r) + (0.504 * g) + (0.098 * b) + 16;
      cr =  (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
      cb = -(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;
      if(y<0) y = 0; if(y>255) y = 255;
      if(cb<0) cb = 0; if(cb>255) cb = 255;
      if(cr<0) cr = 0; if(cr>255) cr = 255;
      *buf++ = y;
      if(j&1&&i&1)
	*off++ = cr;
      if(j&1&&i&1)
	*off2++ = cb;
    }
  }
}

int main(int argc, char **argv)
{
  int w, h, tmp;
  FILE *f;
  unsigned char *in_rgb, *buf, *rgb;
  struct rtj_header hdr;
  char t[256];
  __s8 *strm;
  __u8 Q;
 
  if(argc!=3)
    {
      fprintf(stderr, "usage: pnm2rtj <filename> <Q factor (32..255)>\n\n");
      exit(-1);
    }
 
  f=fopen(argv[1], "r");
  Q=atoi(argv[2]);
  fgets(t, 256, f);
  do
    {
      fgets(t, 256, f);
    } while(t[0]=='#');
  sscanf(t, "%d %d", &w, &h);
  fgets(t, 256, f);

  /* init header */
  rtj_init_header(&hdr);

  /* compress and save tables in header */
  RTjpeg_init_compress(hdr.tbls, w, h, Q); 
 
  in_rgb=(unsigned char *)malloc(w*h*3);
  buf=(unsigned char *)malloc(w*h+(w*h)/2);
  strm=malloc(w*h+(w*h)/2);
  rgb=(unsigned char *)malloc(w*h*3);

  tmp=fread(in_rgb, 1, w*h*3, f);
  fclose(f);
  rgb_yuv(w,h,in_rgb,buf);
  
  fprintf(stderr, "Read %d bytes (%dx%d)\n", tmp, w, h);

  tmp=RTjpeg_compressYUV420(strm, buf);
 
  fprintf(stderr, "strmlen: %d (%f)\n", tmp, (w*h*3)/(float)tmp);
 
  /* write header */
  hdr.width = w;
  hdr.height = h;
  rtj_write_header( &hdr, stdout );
 
  /* write pic info  */
  fwrite( strm, 1, tmp, stdout );
 
  return 0;
}
