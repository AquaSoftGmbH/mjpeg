/* 
 *  playdv.c
 *
 *     Copyright (C) Charles 'Buck' Krasic - April 2000
 *     Copyright (C) Erik Walthinsen - April 2000
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  decoder.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */

#define BENCHMARK_MODE 0
#if BENCHMARK_MODE
#define GTKDISPLAY 0
#else
#define GTKDISPLAY 1
#endif
#define Y_ONLY 0

#include <glib.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "bitstream.h"
#include "dct.h"
#include "idct_248.h"
#include "quant.h"
#include "weighting.h"
#include "vlc.h"
#include "parse.h"
#include "place.h"
#include "ycrcb_to_rgb32.h"

void convert_coeffs(dv_block_t *bl)
{
  int i;
  for(i=0;
      i<64;
      i++) {
    bl->coeffs248[i] = bl->coeffs[i];
  }
} // convert_coeffs

void convert_coeffs_prime(dv_block_t *bl)
{
  int i;
  for(i=0;
      i<64;
      i++) {
    bl->coeffs[i] = bl->coeffs248[i];
  }
} // convert_coeffs_prime

int main(int argc,char *argv[]) {
  static guint8 vsbuffer[80*5]      __attribute__ ((aligned (64))); 
  static dv_videosegment_t videoseg __attribute__ ((aligned (64)));
  FILE *f;
  dv_sample_t sampling;
  gboolean isPAL = 0;
  gboolean is61834 = 0;
  gboolean displayinit = FALSE;
  gint numDIFseq;
  dv_macroblock_t *mb;
  dv_block_t *bl;
  gint ds;
  gint b,m,v;
  gint lost_coeffs;
  guint dif;
  guint offset;
  size_t mb_offset;
  static gint frame_count;
  GtkWidget *window,*image;
  guint8 rgb_frame[720*576*4];

  if (argc >= 2)
    f = fopen(argv[1],"r");
  else
    f = stdin;

  weight_init();  
  dct_init();
  dv_dct_248_init();
  dv_construct_vlc_table();
  dv_parse_init();
  dv_place_init();
  dv_ycrcb_init();
  videoseg.bs = bitstream_init();

  lost_coeffs = 0;
  dif = 0;
  while (!feof(f)) {
    /* Rather read a whole frame at a time, as this gives the
       data-cache a lot longer time to stay warm during decode */
    // start by reading header block, to determine stream parameters
    offset = dif * 80;
    if (fseek(f,offset,SEEK_SET)) break;
    if (fread(vsbuffer,sizeof(vsbuffer),1,f)<1) break;
    isPAL = vsbuffer[3] & 0x80;
    is61834 = vsbuffer[3] & 0x01;
    if(isPAL && is61834)
      sampling = e_dv_sample_420;
    else
      sampling = e_dv_sample_411;
//    printf("video is %s-compliant %s\n",
//           is61834?"IEC61834":"SMPTE314M",isPAL?"PAL":"NTSC");
    numDIFseq = isPAL ? 12 : 10;

#if GTKDISPLAY
    if (!displayinit) {
      gtk_init(&argc,&argv);  
      gdk_rgb_init();
      gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
      gtk_widget_set_default_visual(gdk_rgb_get_visual());
      window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      image = gtk_drawing_area_new();
      gtk_container_add(GTK_CONTAINER(window),image);
      gtk_drawing_area_size(GTK_DRAWING_AREA(image),720,isPAL?576:480);
      gtk_widget_set_usize(GTK_WIDGET(image),720,isPAL?576:480);
      gtk_widget_show(image);
      gtk_widget_show(window);
      gdk_flush();
      while (gtk_events_pending())
        gtk_main_iteration();
      gdk_flush();
      displayinit = TRUE;
    }
#endif

    // each DV frame consists of a sequence of DIF segments 
    for (ds=0;
	 ds<numDIFseq;
	 ds++) { 
      // Each DIF segment conists of 150 dif blocks, 135 of which are video blocks
      dif += 6; // skip the first 6 dif blocks in a dif sequence 
      /* A video segment consists of 5 video blocks, where each video
         block contains one compressed macroblock.  DV bit allocation
         for the VLC stage can spill bits between blocks in the same
         video segment.  So parsing needs the whole segment to decode
         the VLC data */
      // Loop through video segments 
      for (v=0;v<27;v++) {
	// skip audio block - interleaved before every 3rd video segment
	if(!(v % 3)) dif++; 
        // stage 1: parse and VLC decode 5 macroblocks that make up a video segment
	offset = dif * 80;
	if(fseek(f,offset,SEEK_SET)) break;
	if(fread(vsbuffer,sizeof(vsbuffer),1,f)<1) break;
	bitstream_new_buffer(videoseg.bs, vsbuffer, 80*5); 
	videoseg.i = ds;
	videoseg.k = v;
        videoseg.isPAL = isPAL;
        lost_coeffs += dv_parse_video_segment(&videoseg);
        // stage 2: dequant/unweight/iDCT blocks, and place the macroblocks
        for (m=0,mb = videoseg.mb;
	     m<5;
	     m++,mb++) {
	  for (b=0,bl = mb->b;
	       b<6;
	       b++,bl++) {
	    if (bl->dct_mode == DV_DCT_248) { 
	      quant_248_inverse(bl->coeffs,mb->qno,bl->class_no);
	      weight_248_inverse(bl->coeffs);
	      convert_coeffs(bl);
	      dv_idct_248(bl->coeffs248);
	      convert_coeffs_prime(bl);
	    } else {
	      quant_88_inverse(bl->coeffs,mb->qno,bl->class_no);
	      weight_88_inverse(bl->coeffs);
	      idct_88(bl->coeffs);
	    } // else
	  } // for b
	  if(sampling == e_dv_sample_411) {
	    mb_offset = dv_place_411_macroblock(mb,4);
	    if((mb->j == 4) && (mb->k > 23)) 
	      dv_ycrcb_420_block(rgb_frame + mb_offset, mb->b);
	    else
	      dv_ycrcb_411_block(rgb_frame + mb_offset, mb->b);
	  } else {
	    mb_offset = dv_place_420_macroblock(mb,4);
	    dv_ycrcb_420_block(rgb_frame + mb_offset, mb->b);
	  }
        } // for mb
	dif+=5;
      } // for s
    } // ds
    //fprintf(stderr,"displaying frame (%d coeffs lost in parse)\n", lost_coeffs);
    frame_count++;
#if BENCHMARK_MODE
    if(frame_count >= 300) break;
#endif
#if GTKDISPLAY
	
    gdk_draw_rgb_32_image(image->window,image->style->fg_gc[image->state],
                        0,0,720,isPAL?576:480,GDK_RGB_DITHER_NORMAL,rgb_frame,720*4);
    gdk_flush();
    while (gtk_events_pending())
      gtk_main_iteration();
    gdk_flush();
#endif
  }
  exit(0);
}
