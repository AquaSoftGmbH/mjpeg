/* gap_enc_jpeg.h
 *
 *  GAP common encoder jpeg procedures
 *  with dependencies to libjeg .
 *
 */

#ifndef GAP_ENC_JPEG_H
#define GAP_ENC_JPEG_H

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* JPEGlib includes */
#include "jpeglib.h"
#include "jerror.h"

#define JPEG_DEFAULT_QUALITY 80
#define MAX_JPEGSIZE 300000

/* *************************************************
   ***    JPEG memory compression extensions     ***
   *************************************************
   These extensions define functions to compress a JPEG in memory.
   (see libJPEGs source doc for more info on dest-managers) */

/* Expanded data destination object for stdio output */                                                                                                         
typedef struct {                                                                  
  struct jpeg_destination_mgr pub; /* public fields */                                                                                                            
  JOCTET * buffer;              /* start of buffer */                           
} memjpeg_dest_mgr;                                                                                                                                           
/* That's the maximum size of the generated JPEGs.
   As video pictures don't have a higher resolution 
   than 720x576, I don't see any reason to make
   it larger than 256kb */
#define OUTPUT_BUF_SIZE  256*1024

#if 0
struct jpeg_destination_mgr {
  JOCTET * next_output_byte;	/* => next byte to write in buffer */
  size_t free_in_buffer;	/* # of byte spaces remaining in buffer */

  JMETHOD(void, init_destination, (j_compress_ptr cinfo));
  JMETHOD(boolean, empty_output_buffer, (j_compress_ptr cinfo));
  JMETHOD(void, term_destination, (j_compress_ptr cinfo));
};
#endif

METHODDEF(void)
init_destination (j_compress_ptr cinfo);

METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo);

METHODDEF(void)
term_destination (j_compress_ptr cinfo);

GLOBAL(void)
jpeg_memio_dest (j_compress_ptr cinfo, guchar *memjpeg, size_t **remaining);


guchar*
p_drawable_encode_jpeg(GDrawable *drawable, gint32 jpeg_interlaced, gint32 *JPEG_size, 
			       gint32 jpeg_quality, gint32 odd_even, gint32 use_YUV411, 
			       void *app0_buffer, gint32 app0_length);




#endif 		/* end GAP_ENC_JPEG_H */
