/*
 *  jpegutils.c: Some Utility programs for dealing with
 *               JPEG encoded images
 *
 *  Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *  Copyright (C) 2001 pHilipp Zabel  <pzabel@gmx.de>
 *
 *  based on jdatasrc.c and jdatadst.c from the Independent
 *  JPEG Group's software by Thomas G. Lane
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
#include <setjmp.h>
#include <jpeglib.h>
#include <jerror.h>

#include "mjpeg_logging.h"

#include "lav_io.h"

 /*
 * jpeg_data:       buffer with input / output jpeg
 * len:             Length of jpeg buffer
 * itype:           0: Not interlaced
 *                  1: Interlaced, Odd first
 *                  2: Interlaced, even first
 * ctype            Chroma format for decompression.
 *                  Currently always 420 and hence ignored.
 * raw0             buffer with input / output raw Y channel
 * raw1             buffer with input / output raw U/Cb channel
 * raw2             buffer with input / output raw V/Cr channel
 * width            width of Y channel (width of U/V is width/2)
 * height           height of Y channel (height of U/V is height/2)
 */

int decode_jpeg_raw (unsigned char *jpeg_data, int len,
                     int itype, int ctype, int width, int height,
                     unsigned char *raw0, unsigned char *raw1,
                     unsigned char *raw2);

int encode_jpeg_raw (unsigned char *jpeg_data, int len, int quality,
                     int itype, int ctype, int width, int height,
                     unsigned char *raw0, unsigned char *raw1,
                     unsigned char *raw2);

static void jpeg_buffer_src  (j_decompress_ptr cinfo, unsigned char *buffer,
                       long num);
static void jpeg_buffer_dest (j_compress_ptr cinfo,   unsigned char *buffer,
                       long len);
static void jpeg_skip_ff (j_decompress_ptr cinfo);

/*******************************************************************
 *                                                                 *
 *    The following routines define a JPEG Source manager which    *
 *    just reads from a given buffer (instead of a file as in      *
 *    the jpeg library)                                            *
 *                                                                 *
 *******************************************************************/


/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

static void init_source (j_decompress_ptr cinfo)
{
   /* no work necessary here */
}


/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * Should never be called since all data should be allready provided.
 * Is nevertheless sometimes called - sets the input buffer to data
 * which is the JPEG EOI marker;
 *
 */

static char EOI_data[2] = { 0xFF, 0xD9 };

static boolean fill_input_buffer (j_decompress_ptr cinfo)
{
   cinfo->src->next_input_byte = EOI_data;
   cinfo->src->bytes_in_buffer = 2;
   return TRUE;
}


/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 */

static void skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
   if (num_bytes > 0) {
      if (num_bytes > (long) cinfo->src->bytes_in_buffer)
         num_bytes = (long) cinfo->src->bytes_in_buffer;
      cinfo->src->next_input_byte += (size_t) num_bytes;
      cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
   }
}


/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 */

static void term_source (j_decompress_ptr cinfo)
{
   /* no work necessary here */
}


/*
 * Prepare for input from a data buffer.
 */

static void
jpeg_buffer_src (j_decompress_ptr cinfo, unsigned char *buffer, long num)
{
   /* The source object and input buffer are made permanent so that a series
    * of JPEG images can be read from the same buffer by calling jpeg_buffer_src
    * only before the first one.  (If we discarded the buffer at the end of
    * one image, we'd likely lose the start of the next one.)
    * This makes it unsafe to use this manager and a different source
    * manager serially with the same JPEG object.  Caveat programmer.
    */
   if (cinfo->src == NULL) {    /* first time for this JPEG object? */
      cinfo->src = (struct jpeg_source_mgr *)
          (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                      sizeof (struct jpeg_source_mgr));
   }

   cinfo->src->init_source = init_source;
   cinfo->src->fill_input_buffer = fill_input_buffer;
   cinfo->src->skip_input_data = skip_input_data;
   cinfo->src->resync_to_restart = jpeg_resync_to_restart;	/* use default method */
   cinfo->src->term_source = term_source;
   cinfo->src->bytes_in_buffer = num;
   cinfo->src->next_input_byte = (JOCTET *) buffer;
}


/*
 * jpeg_skip_ff is not a part of the source manager but it is
 * particularly useful when reading several images from the same buffer:
 * It should be called to skip padding 0xff bytes beetween images.
 */

static void 
jpeg_skip_ff (j_decompress_ptr cinfo)
{
   while (cinfo->src->bytes_in_buffer > 1
          && cinfo->src->next_input_byte[0] == 0xff
          && cinfo->src->next_input_byte[1] == 0xff) {
      cinfo->src->bytes_in_buffer--;
      cinfo->src->next_input_byte++;
   }
}


/*******************************************************************
 *                                                                 *
 *    The following routines define a JPEG Destination manager     *
 *    which just reads from a given buffer (instead of a file      *
 *    as in the jpeg library)                                      *
 *                                                                 *
 *******************************************************************/


/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

static void init_destination (j_compress_ptr cinfo)
{
   /* No work necessary here */
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * Should never be called since all data should be written to the buffer.
 * If it gets called, the given jpeg buffer was too small.
 *
 */

static boolean empty_output_buffer (j_compress_ptr cinfo)
{
   //FIXME:
   mjpeg_error( "Given jpeg buffer was too small!\n");
   ERREXIT (cinfo, JERR_BUFFER_SIZE);	/* shouldn't be FILE_WRITE but BUFFER_OVERRUN! */
   return TRUE;
}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

static void term_destination (j_compress_ptr cinfo)
{
   /* no work necessary here */
}


/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

static void
jpeg_buffer_dest (j_compress_ptr cinfo, unsigned char *buf, long len)
{

   /* The destination object is made permanent so that multiple JPEG images
    * can be written to the same file without re-executing jpeg_stdio_dest.
    * This makes it dangerous to use this manager and a different destination
    * manager serially with the same JPEG object, because their private object
    * sizes may be different.  Caveat programmer.
    */
   if (cinfo->dest == NULL) {   /* first time for this JPEG object? */
      cinfo->dest = (struct jpeg_destination_mgr *)
          (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                      sizeof (struct
                                              jpeg_destination_mgr));
   }

   cinfo->dest->init_destination = init_destination;
   cinfo->dest->empty_output_buffer = empty_output_buffer;
   cinfo->dest->term_destination = term_destination;
   cinfo->dest->free_in_buffer = len;
   cinfo->dest->next_output_byte = (JOCTET *) buf;
}


/*******************************************************************
 *                                                                 *
 *    decode_jpeg_data: Decode a (possibly interlaced) JPEG frame  *
 *                                                                 *
 *******************************************************************/

/*
 * ERROR HANDLING:
 *
 *    We want in all cases to return to the user.
 *    The following kind of error handling is from the
 *    example.c file in the Independent JPEG Group's JPEG software
 */

struct my_error_mgr {
   struct jpeg_error_mgr pub;   /* "public" fields */
   jmp_buf setjmp_buffer;       /* for return to caller */
};

static void my_error_exit (j_common_ptr cinfo)
{
   /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
   struct my_error_mgr *myerr = (struct my_error_mgr *) cinfo->err;

   /* Always display the message. */
   /* We could postpone this until after returning, if we chose. */
   (*cinfo->err->output_message) (cinfo);

   /* Return control to the setjmp point */
   longjmp (myerr->setjmp_buffer, 1);
}

#define MAX_LUMA_WIDTH   4096
#define MAX_CHROMA_WIDTH 2048

static unsigned char buf0[16][MAX_LUMA_WIDTH];
static unsigned char buf1[8][MAX_CHROMA_WIDTH];
static unsigned char buf2[8][MAX_CHROMA_WIDTH];
static unsigned char chr1[8][MAX_CHROMA_WIDTH];
static unsigned char chr2[8][MAX_CHROMA_WIDTH];


/*
 * jpeg_data:       Buffer with jpeg data to decode
 * len:             Length of buffer
 * itype:           0: Not interlaced
 *                  1: Interlaced, Odd first
 *                  2: Interlaced, even first
 * ctype            Chroma format for decompression.
 *                  Currently always 420 and hence ignored.
 */

int decode_jpeg_raw (unsigned char *jpeg_data, int len,
                     int itype, int ctype, int width, int height,
                     unsigned char *raw0, unsigned char *raw1,
                     unsigned char *raw2)
{
   int numfields, hsf[3], vsf[3], field, yl, yc, x, y, i, xsl, xsc, xs, xd,
       hdown;

   JSAMPROW row0[16] = { buf0[0], buf0[1], buf0[2], buf0[3],
      buf0[4], buf0[5], buf0[6], buf0[7],
      buf0[8], buf0[9], buf0[10], buf0[11],
      buf0[12], buf0[13], buf0[14], buf0[15]
   };
   JSAMPROW row1[8] = { buf1[0], buf1[1], buf1[2], buf1[3],
      buf1[4], buf1[5], buf1[6], buf1[7]
   };
   JSAMPROW row2[8] = { buf2[0], buf2[1], buf2[2], buf2[3],
      buf2[4], buf2[5], buf2[6], buf2[7]
   };
   JSAMPARRAY scanarray[3] = { row0, row1, row2 };
   struct jpeg_decompress_struct dinfo;
   struct my_error_mgr jerr;

   /* We set up the normal JPEG error routines, then override error_exit. */
   dinfo.err = jpeg_std_error (&jerr.pub);
   jerr.pub.error_exit = my_error_exit;

   /* Establish the setjmp return context for my_error_exit to use. */
   if (setjmp (jerr.setjmp_buffer)) {
      /* If we get here, the JPEG code has signaled an error. */
      jpeg_destroy_decompress (&dinfo);
      return -1;
   }

   jpeg_create_decompress (&dinfo);

   jpeg_buffer_src (&dinfo, jpeg_data, len);

   /* Read header, make some checks and try to figure out what the
      user really wants */

   jpeg_read_header (&dinfo, TRUE);
   dinfo.raw_data_out = TRUE;
   dinfo.out_color_space = JCS_YCbCr;
   dinfo.dct_method = JDCT_IFAST;
   jpeg_start_decompress (&dinfo);

   if (dinfo.output_components != 3) {
      mjpeg_error( "Output components of JPEG image = %d, must be 3\n",
               dinfo.output_components);
      goto ERR_EXIT;
   }

   for (i = 0; i < 3; i++) {
      hsf[i] = dinfo.comp_info[i].h_samp_factor;
      vsf[i] = dinfo.comp_info[i].v_samp_factor;
   }

   if (hsf[0] != 2 || hsf[1] != 1 || hsf[2] != 1 ||
       (vsf[0] != 1 && vsf[0] != 2) || vsf[1] != 1 || vsf[2] != 1) {
      mjpeg_error( "Unsupported sampling factors!");
      goto ERR_EXIT;
   }

   /* Height match image height or be exact twice the image height */

   if (dinfo.output_height == height) {
      numfields = 1;
   } else if (2 * dinfo.output_height == height) {
      numfields = 2;
   } else {
      mjpeg_error(
               "Read JPEG: requested height = %d, height of image = %d\n",
               height, dinfo.output_height);
      goto ERR_EXIT;
   }

   /* Width is more flexible */

   if (dinfo.output_width > MAX_LUMA_WIDTH) {
      mjpeg_error( "Image width of %d exceeds max\n",
               dinfo.output_width);
      goto ERR_EXIT;
   }
   if (width < 2 * dinfo.output_width / 3) {
      /* Downsample 2:1 */

      hdown = 1;
      if (2 * width < dinfo.output_width)
         xsl = (dinfo.output_width - 2 * width) / 2;
      else
         xsl = 0;
   } else if (width == 2 * dinfo.output_width / 3) {
      /* special case of 3:2 downsampling */

      hdown = 2;
      xsl = 0;
   } else {
      /* No downsampling */

      hdown = 0;
      if (width < dinfo.output_width)
         xsl = (dinfo.output_width - width) / 2;
      else
         xsl = 0;
   }

   /* Make xsl even, calculate xsc */

   xsl = xsl & ~1;
   xsc = xsl / 2;

   yl = yc = 0;

   for (field = 0; field < numfields; field++) {
      if (field > 0) {
         jpeg_read_header (&dinfo, TRUE);
         dinfo.raw_data_out = TRUE;
         dinfo.out_color_space = JCS_YCbCr;
         dinfo.dct_method = JDCT_IFAST;
         jpeg_start_decompress (&dinfo);
      }

      if (numfields == 2) {
         switch (itype) {
         case LAV_INTER_ODD_FIRST:
            yl = yc = (1 - field);
            break;
         case LAV_INTER_EVEN_FIRST:
            yl = yc = field;
            break;
         default:
            mjpeg_error(
                     "Input is interlaced but no interlacing set\n");
            goto ERR_EXIT;
         }
      } else
         yl = yc = 0;

      while (dinfo.output_scanline < dinfo.output_height) {
         jpeg_read_raw_data (&dinfo, scanarray, 8 * vsf[0]);

         for (y = 0; y < 8 * vsf[0]; yl += numfields, y++) {
            xd = yl * width;
            xs = xsl;

            if (hdown == 0)
               for (x = 0; x < width; x++)
                  raw0[xd++] = row0[y][xs++];
            else if (hdown == 1)
               for (x = 0; x < width; x++, xs += 2)
                  raw0[xd++] = (row0[y][xs] + row0[y][xs + 1]) >> 1;
            else
               for (x = 0; x < width / 2; x++, xd += 2, xs += 3) {
                  raw0[xd] = (2 * row0[y][xs] + row0[y][xs + 1]) / 3;
                  raw0[xd + 1] =
                      (2 * row0[y][xs + 2] + row0[y][xs + 1]) / 3;
               }
         }

         /* Horizontal downsampling of chroma */

         for (y = 0; y < 8; y++) {
            xs = xsc;

            if (hdown == 0)
               for (x = 0; x < width / 2; x++, xs++) {
                  chr1[y][x] = row1[y][xs];
                  chr2[y][x] = row2[y][xs];
            } else if (hdown == 1)
               for (x = 0; x < width / 2; x++, xs += 2) {
                  chr1[y][x] = (row1[y][xs] + row1[y][xs + 1]) >> 1;
                  chr2[y][x] = (row2[y][xs] + row2[y][xs + 1]) >> 1;
            } else
               for (x = 0; x < width / 2; x += 2, xs += 3) {
                  chr1[y][x] = (2 * row1[y][xs] + row1[y][xs + 1]) / 3;
                  chr1[y][x + 1] =
                      (2 * row1[y][xs + 2] + row1[y][xs + 1]) / 3;
                  chr2[y][x] = (2 * row2[y][xs] + row2[y][xs + 1]) / 3;
                  chr2[y][x + 1] =
                      (2 * row2[y][xs + 2] + row2[y][xs + 1]) / 3;
               }
         }

         /* Vertical downsampling of chroma */

         if (vsf[0] == 1) {
            /* Really downsample */

            for (y = 0; y < 8; y += 2, yc += numfields) {
               xd = yc * width / 2;
               for (x = 0; x < width / 2; x++, xd++) {
                  raw1[xd] = (chr1[y][x] + chr1[y + 1][x]) >> 1;
                  raw2[xd] = (chr2[y][x] + chr2[y + 1][x]) >> 1;
               }
            }
         } else {
            /* Just copy */

            for (y = 0; y < 8; y++, yc += numfields) {
               xd = yc * width / 2;
               for (x = 0; x < width / 2; x++, xd++) {
                  raw1[xd] = chr1[y][x];
                  raw2[xd] = chr2[y][x];
               }
            }
         }
      }

      (void) jpeg_finish_decompress (&dinfo);
      if (field == 0 && numfields > 1)
         jpeg_skip_ff (&dinfo);
   }

   jpeg_destroy_decompress (&dinfo);
   return 0;

 ERR_EXIT:
   jpeg_destroy_decompress (&dinfo);
   return -1;
}


/*******************************************************************
 *                                                                 *
 *    encode_jpeg_data: Compress raw YCbCr data (output JPEG       *
 *                      may be interlaced                          *
 *                                                                 *
 *******************************************************************/
 
 /*
 * jpeg_data:       Buffer to hold output jpeg
 * len:             Length of buffer
 * itype:           0: Not interlaced
 *                  1: Interlaced, Odd first
 *                  2: Interlaced, even first
 * ctype            Chroma format for decompression.
 *                  Currently always 420 and hence ignored.
 */

int encode_jpeg_raw (unsigned char *jpeg_data, int len, int quality,
                     int itype, int ctype, int width, int height,
                     unsigned char *raw0, unsigned char *raw1,
                     unsigned char *raw2)
{
   int numfields, field, yl, yc, y, i;

   JSAMPROW row0[16] = { buf0[0], buf0[1], buf0[2], buf0[3],
      buf0[4], buf0[5], buf0[6], buf0[7],
      buf0[8], buf0[9], buf0[10], buf0[11],
      buf0[12], buf0[13], buf0[14], buf0[15]
   };
   JSAMPROW row1[8] = { buf1[0], buf1[1], buf1[2], buf1[3],
      buf1[4], buf1[5], buf1[6], buf1[7]
   };
   JSAMPROW row2[8] = { buf2[0], buf2[1], buf2[2], buf2[3],
      buf2[4], buf2[5], buf2[6], buf2[7]
   };
   JSAMPARRAY scanarray[3] = { row0, row1, row2 };

   struct jpeg_compress_struct cinfo;
   struct my_error_mgr jerr;

   /* We set up the normal JPEG error routines, then override error_exit. */
   cinfo.err = jpeg_std_error (&jerr.pub);
   jerr.pub.error_exit = my_error_exit;

   /* Establish the setjmp return context for my_error_exit to use. */
   if (setjmp (jerr.setjmp_buffer)) {
      /* If we get here, the JPEG code has signaled an error. */
      jpeg_destroy_compress (&cinfo);
      return -1;
   }

   jpeg_create_compress (&cinfo);

   jpeg_buffer_dest(&cinfo, jpeg_data, len);

   /* Set some jpeg header fields */

   cinfo.input_components = 3;
   jpeg_set_defaults (&cinfo);
   jpeg_set_quality  (&cinfo, quality, FALSE);

   cinfo.raw_data_in = TRUE;
   cinfo.in_color_space = JCS_YCbCr;
   cinfo.dct_method = JDCT_IFAST;

   cinfo.input_gamma = 1.0;

   cinfo.comp_info[0].h_samp_factor = 2;
   cinfo.comp_info[0].v_samp_factor = 1;	/*1||2 */
   cinfo.comp_info[1].h_samp_factor = 1;
   cinfo.comp_info[1].v_samp_factor = 1;
   cinfo.comp_info[2].h_samp_factor = 1;	/*1||2 */
   cinfo.comp_info[2].v_samp_factor = 1;


   if ((width>4096)||(height>4096)) {
      mjpeg_error( "Image dimensions (%dx%d) exceed lavtools' max (4096x4096)\n", width, height);
      goto ERR_EXIT;
   }
   if ((width%16)||(height%16)) {
      mjpeg_error( "Image dimensions (%dx%d) not multiples of 16\n", width, height);
      goto ERR_EXIT;
   }
   cinfo.image_width = width;
   switch (itype) {
   case 1:
   case 2: /* interlaced */
      numfields = 2;
      break;
   default:
      numfields = 1;
      if (height > 2048) {
         mjpeg_error( "Image height (%d) exceeds lavtools max for non-interlaced frames\n", height);
         goto ERR_EXIT;
      }
   }
   cinfo.image_height = height/numfields;

   yl = yc = 0;                 /* y luma, chroma */

   for (field = 0; field < numfields; field++) {

      jpeg_start_compress (&cinfo, FALSE);
      
      if (numfields == 2) {
         switch (itype) {
         case 2: /* even field first */
            yl = yc = (1 - field);
            break;
         case 1: /* odd field first */
            yl = yc = field;
            break;
         default:
            mjpeg_error(
                     "Input is interlaced but no interlacing set\n");
            goto ERR_EXIT;
         }
      } else
         yl = yc = 0;

      while (cinfo.next_scanline < cinfo.image_height) {

         for (y = 0; y < 8 * cinfo.comp_info[0].v_samp_factor;
              yl += numfields, y++) {
            row0[y] = &raw0[yl * width];
         }
         for (y = 0; y < 8; y++) {
            row1[y] = &raw1[yc * width / 2];
            row2[y] = &raw2[yc * width / 2];
            if (y%2) yc += numfields;
         }

         jpeg_write_raw_data (&cinfo, scanarray,
                              8 * cinfo.comp_info[0].v_samp_factor);

      }

      (void) jpeg_finish_compress (&cinfo);
   }
   
   /* FIXME */
   i = len - cinfo.dest->free_in_buffer;

   jpeg_destroy_compress (&cinfo);

   return i;   /* size of jpeg */

 ERR_EXIT:
   jpeg_destroy_compress (&cinfo);
   return -1;
}
