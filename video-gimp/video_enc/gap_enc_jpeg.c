/* gap_enc_jpeg.c
 *
 *  GAP common encoder jpeg procedures
 *  with dependencies to libjeg .
 *
 */
 
/* SYSTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_enc_jpeg.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

METHODDEF(void)
init_destination (j_compress_ptr cinfo)
{
  memjpeg_dest_mgr *dest = (memjpeg_dest_mgr *) cinfo->dest;

  // printf("GAP_MOVTAR: Memory JPEG's init_destination called !\n");
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo)
{
  memjpeg_dest_mgr * dest = (memjpeg_dest_mgr *) cinfo->dest;

  printf("EMERGENCY in gap_movtar: The jpeg memory compression\n is limited to 256 kb/frame !\n"
	 "Consult gz@lysator.liu.se to fix this problem !\n");
  ERREXIT(cinfo, JERR_FILE_WRITE);

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

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

METHODDEF(void)
term_destination (j_compress_ptr cinfo)
{
}

/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 * ATTENTION ! memjpeg has to have at least OUTPUT_BUF_SIZE reserved !
 */

GLOBAL(void)
jpeg_memio_dest (j_compress_ptr cinfo, guchar *memjpeg, size_t **remaining)
{
   memjpeg_dest_mgr *dest;

  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
   //  printf("GAP_MOVTAR: Running jpeg_memio_dest !\n");
  if (cinfo->dest == NULL) 
    {	/* first time for this JPEG object? */
      cinfo->dest = (struct jpeg_destination_mgr *)
	(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				    sizeof(memjpeg_dest_mgr));
    }

  dest = (memjpeg_dest_mgr *) cinfo->dest;
  dest->pub.init_destination = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination = term_destination;
  dest->buffer = memjpeg;
  *remaining = &(dest->pub.free_in_buffer);
}

/* p_drawable_encode_jpeg
   in: drawable: Describes the picture to be compressed in GIMP terms.
       jpeg_interlaced: TRUE: Generate two JPEGs (one for odd/even lines each) into one buffer.
       jpeg_quality: The quality of the generated JPEG (0-100, where 100 is best).
       odd_even (only valid for jpeg_interlaced = TRUE): TRUE: Code the odd lines first.
                                                         FALSE: Code the even lines first.
       app0_buffer: if != NULL, the content of the APP0-marker to write.
       app0_length: the length of the APP0-marker.						 
   out:JPEG_size: The size of the buffer that is returned.   
   returns: guchar *: A buffer, allocated by this routines, which contains
                      the compressed JPEG, NULL on error. */

guchar *p_drawable_encode_jpeg(GDrawable *drawable, gint32 jpeg_interlaced, gint32 *JPEG_size, 
			       gint32 jpeg_quality, gint32 odd_even, gint32 use_YUV411, 
			       void *app0_buffer, gint32 app0_length)
{
  GPixelRgn pixel_rgn;
  GDrawableType drawable_type;
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  
  guchar *temp, *t;
  guchar *data;
  guchar *src, *s;
  int has_alpha;
  int rowstride, yend;
  int i, j, y;
  guchar *JPEG_data;
  size_t *JPEG_buf_remain;
  size_t totalsize = 0;

  drawable_type = gimp_drawable_type (drawable->id);
  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width, drawable->height, FALSE, FALSE);

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);

  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress (&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* We use our own jpeg destination mgr */
  JPEG_data = (guchar *)g_malloc(OUTPUT_BUF_SIZE * sizeof(guchar));

  /* Install my memory destination manager (instead of stdio_dest) */
  jpeg_memio_dest (&cinfo, JPEG_data, &JPEG_buf_remain);

  if (gap_debug) fprintf(stderr, "GAP_AVI: encode_jpeg: Cleared the initilization !\n");

  /* Get the input image and a pointer to its data.
   */
  switch (drawable_type)
    {
    case RGB_IMAGE:
    case GRAY_IMAGE:
      /* # of color components per pixel */
      cinfo.input_components = drawable->bpp;
      has_alpha = 0;
      break;
    case RGBA_IMAGE:
    case GRAYA_IMAGE:
      gimp_message ("jpeg: image contains a-channel info which will be lost");
      /* # of color components per pixel (minus the GIMP alpha channel) */
      cinfo.input_components = drawable->bpp - 1;
      has_alpha = 1;
      break;
    case INDEXED_IMAGE:
      gimp_message ("jpeg: cannot operate on indexed color images");
      return FALSE;
      break;
    default:
      gimp_message ("jpeg: cannot operate on unknown image types");
      return FALSE;
      break;
    }

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  /* image width and height, in pixels */
  cinfo.image_width = drawable->width;
  cinfo.image_height = drawable->height;
  /* colorspace of input image */
  cinfo.in_color_space = (drawable_type == RGB_IMAGE ||
			  drawable_type == RGBA_IMAGE)
    ? JCS_RGB : JCS_GRAYSCALE;
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults (&cinfo);
  jpeg_set_quality (&cinfo, (int) (jpeg_quality), TRUE);

  /* Smoothing is not possible for nonstandard sampling rates */
  /*  cinfo.smoothing_factor = (int) (50); */
  /* I wonder if this is slower: */ 
  /* cinfo.optimize_coding = 1; */

  /* Are these the evil AVI destroyers ? */
  cinfo.write_Adobe_marker = FALSE;
  cinfo.write_JFIF_header = FALSE;

  /* That's the only allowed encoding in a movtar */
  if (!use_YUV411)
    {
      cinfo.comp_info[0].h_samp_factor = 2;
      cinfo.comp_info[0].v_samp_factor = 1;
      cinfo.comp_info[1].h_samp_factor = 1;
      cinfo.comp_info[1].v_samp_factor = 1;
      cinfo.comp_info[2].h_samp_factor = 1;
      cinfo.comp_info[2].v_samp_factor = 1;
    }
  else
    if (gap_debug) fprintf(stderr, "Using YUV 4:1:1 encoding !");

  cinfo.restart_interval = 0;
  cinfo.restart_in_rows = FALSE; 
  /* could be _ISLOW or _FLOAT, too, but this is fastest */
  cinfo.dct_method = JDCT_ISLOW; 
  if (gap_debug) fprintf(stderr, "GAP_AVI: encode_jpeg: Cleared parameter setting !\n");

  if (jpeg_interlaced)
    {
      cinfo.image_height = drawable->height/2;
      
      rowstride = drawable->bpp * drawable->width;
      temp = (guchar *) g_malloc (cinfo.image_width * cinfo.input_components);
      data = (guchar *) g_malloc (rowstride * gimp_tile_height ());
  
      for (y = (odd_even) ? 1 : 0; (odd_even) ? (y >= 0) : (y <= 1); (odd_even) ? (y--) : (y++))
	{
	  if (gap_debug) fprintf(stderr, "GAP_AVI: encode_jpeg: interlaced picture, now coding %s lines\n",
				  y ? "odd" : "even");
	  
	  jpeg_start_compress (&cinfo, TRUE);

	  /* Step 4.1: Write the app0 marker out */
	  if(app0_buffer) 
	    jpeg_write_marker(&cinfo,
			  JPEG_APP0,
			      app0_buffer,
			      app0_length);      
	  
	  while (cinfo.next_scanline < cinfo.image_height)
	    {
	      if (gap_debug) fprintf(stderr, "GAP_AVI: encode_jpeg: Line %d !", cinfo.next_scanline); 
	      gimp_pixel_rgn_get_rect (&pixel_rgn, data, 0, 2 * cinfo.next_scanline + y, 
				       cinfo.image_width, 1);
	      
	      /* Get rid of all the bad stuff (tm) (= get the right pixel order for JPEG encoding) */
	      t = temp;
	      s = data;
	      i = cinfo.image_width;
	      
	      while (i--)
		{
		  for (j = 0; j < cinfo.input_components; j++)
		    *t++ = *s++;
		  if (has_alpha)  /* ignore alpha channel */
		    s++;
		}
	      
	      src += 2*rowstride;
	      jpeg_write_scanlines (&cinfo, (JSAMPARRAY) &temp, 1);
	    }
      
	  jpeg_finish_compress(&cinfo);
	  totalsize += (OUTPUT_BUF_SIZE - *JPEG_buf_remain);
	  //JPEG_data = (guchar *)realloc(JPEG_data, sizeof(guchar)* *JPEG_size);
	}
      /* fprintf(stderr, "2 fields written.\n"); */
    }
  else
    {
      /* Step 4: Start compressor */

      /* TRUE ensures that we will write a complete interchange-JPEG file.
       * Pass TRUE unless you are very sure of what you're doing.
       */
      if (gap_debug) fprintf(stderr, "GAP_AVI: encode_jpeg: non-interlaced picture.\n"); 
      jpeg_start_compress (&cinfo, TRUE);

      /* Step 4.1: Write the app0 marker out */
      if(app0_buffer) 
	jpeg_write_marker(&cinfo,
			  JPEG_APP0,
			  app0_buffer,
			  app0_length);      
      
      /* Step 5: while (scan lines remain to be written) */
      /*           jpeg_write_scanlines(...); */
      
      /* Here we use the library's state variable cinfo.next_scanline as the
       * loop counter, so that we don't have to keep track ourselves.
       * To keep things simple, we pass one scanline per call; you can pass
       * more if you wish, though.
       */
      /* JSAMPLEs per row in image_buffer */
      rowstride = drawable->bpp * drawable->width;
      temp = (guchar *) g_malloc (cinfo.image_width * cinfo.input_components);
      data = (guchar *) g_malloc (rowstride * gimp_tile_height ());

      /* fault if cinfo.next_scanline isn't initially a multiple of
       * gimp_tile_height */
      src = NULL;

      while (cinfo.next_scanline < cinfo.image_height)
	{
	  if (gap_debug) fprintf(stderr, "GAP_AVI: encode_jpeg: Line %d !", cinfo.next_scanline); 
	  if ((cinfo.next_scanline % gimp_tile_height ()) == 0)
	    {
	      yend = cinfo.next_scanline + gimp_tile_height ();
	      yend = MIN (yend, cinfo.image_height);
	      gimp_pixel_rgn_get_rect (&pixel_rgn, data, 0, cinfo.next_scanline, cinfo.image_width,
				       (yend - cinfo.next_scanline));
	      src = data;
	    }
	  
	  t = temp;
	  s = src;
	  i = cinfo.image_width;
	  
	  while (i--)
	    {
	      for (j = 0; j < cinfo.input_components; j++)
		*t++ = *s++;
	      if (has_alpha)  /* ignore alpha channel */
		s++;
	    }
	  
	  src += rowstride;
	  jpeg_write_scanlines (&cinfo, (JSAMPARRAY) &temp, 1);
	}
      
      /* Step 6: Finish compression */
      jpeg_finish_compress (&cinfo);
      totalsize += (OUTPUT_BUF_SIZE - *JPEG_buf_remain);
      // JPEG_data = (guchar *)realloc(JPEG_data, sizeof(guchar)* *JPEG_size);
    }

  /* Step 7: release JPEG compression object */
  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress (&cinfo);

  /* free the temporary buffer */
  g_free (temp);
  g_free (data);

  *JPEG_size = totalsize;
  return JPEG_data;
}	

