/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* Only goal of this file is for some easy access to functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#ifdef HAVE_PNG
#include <png.h>
#endif
#ifdef HAVE_JPEG
#include <jpeglib.h>
#endif

#include "gdkpixbuf_write.h"
#include "gtkfunctions.h"

struct pixbuf_and_fs {
	GtkFileSelection *fs;
	GdkPixbuf *buf;
};

#ifdef HAVE_PNG
gboolean gdk_pixbuf_save_png(GdkPixbuf *pixbuf, char *file)
{
       png_structp png_ptr;
       png_infop info_ptr;
       guchar *ptr;
       guchar *pixels;
       int x, y, j;
       png_bytep row_ptr;
       png_bytep data;
       png_color_8 sig_bit;
       int w, h, rowstride;
       int has_alpha;
       int bpc;
       FILE *f;

       f = fopen(file, "wb");
       if (!f) return FALSE;

       data = NULL;
       
       bpc = gdk_pixbuf_get_bits_per_sample (pixbuf);
       w = gdk_pixbuf_get_width (pixbuf);
       h = gdk_pixbuf_get_height (pixbuf);
       rowstride = gdk_pixbuf_get_rowstride (pixbuf);
       has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
       pixels = gdk_pixbuf_get_pixels (pixbuf);

       png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
       g_return_val_if_fail (png_ptr != NULL, FALSE);

       info_ptr = png_create_info_struct (png_ptr);
       if (info_ptr == NULL) {
               png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
               fclose(f);
               return FALSE;
       }
       if (setjmp (png_ptr->jmpbuf)) {
               png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
               fclose(f);
               return FALSE;
       }
       png_init_io (png_ptr, f);
       if (has_alpha) {
               png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
                             PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
//               png_set_swap_alpha (png_ptr);
               png_set_bgr (png_ptr);
       } else {
               png_set_IHDR (png_ptr, info_ptr, w, h, bpc,
                             PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
               data = g_malloc (w * 3 * sizeof (char));

               if (data == NULL) {
                       /* Check error NULL, normally this would be broken,
                        * but libpng makes me want to code defensively.
                        */
                       png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
                       fclose(f);
                       return FALSE;
               }
       }
       sig_bit.red = bpc;
       sig_bit.green = bpc;
       sig_bit.blue = bpc;
       sig_bit.alpha = bpc;
       png_set_sBIT (png_ptr, info_ptr, &sig_bit);
       png_write_info (png_ptr, info_ptr);
       png_set_shift (png_ptr, &sig_bit);
       png_set_packing (png_ptr);

       ptr = pixels;
       for (y = 0; y < h; y++) {
               if (has_alpha)
                       row_ptr = (png_bytep)ptr;
               else {
                       for (j = 0, x = 0; x < w; x++)
                               memcpy (&(data[x*3]), &(ptr[x*3]), 3);

                       row_ptr = (png_bytep)data;
               }
               png_write_rows (png_ptr, &row_ptr, 1);
               ptr += rowstride;
       }

       if (data)
               g_free (data);

       png_write_end (png_ptr, info_ptr);
       png_destroy_write_struct (&png_ptr, (png_infopp) NULL);

       fclose(f);

       return TRUE;
}
#endif

#ifdef HAVE_JPEG
gboolean gdk_pixbuf_save_jpg (GdkPixbuf *pixbuf, char *file, int quality)
{
       struct jpeg_compress_struct cinfo;
       guchar *buf = NULL;
       guchar *ptr;
       guchar *pixels = NULL;
       JSAMPROW *jbuf;
       int y = 0;
       int i, j;
       int w, h = 0;
       int rowstride = 0;
       struct jpeg_error_mgr pub;
       FILE *f;

       f = fopen(file, "wb");
       if (!f) return FALSE;

       rowstride = gdk_pixbuf_get_rowstride (pixbuf);

       w = gdk_pixbuf_get_width (pixbuf);
       h = gdk_pixbuf_get_height (pixbuf);

       /* no image data? abort */
       pixels = gdk_pixbuf_get_pixels (pixbuf);
       g_return_val_if_fail (pixels != NULL, FALSE);

       /* allocate a small buffer to convert image data */
       buf = g_malloc (w * 3 * sizeof (guchar));
       if (!buf) {
               fclose(f);
	       return FALSE;
       }

       /* setup compress params */
       jpeg_create_compress (&cinfo);
       jpeg_stdio_dest (&cinfo, f);
       cinfo.image_width      = w;
       cinfo.image_height     = h;
       cinfo.input_components = 3; 
       cinfo.in_color_space   = JCS_RGB;

       /* set some error handling parameters */
       cinfo.err = jpeg_std_error (&pub);

       /* set up jepg compression parameters */
       jpeg_set_defaults (&cinfo);
       jpeg_set_quality (&cinfo, quality, TRUE);
       jpeg_start_compress (&cinfo, TRUE);
       /* get the start pointer */
       ptr = pixels;
       /* go one scanline at a time... and save */
       i = 0;
       while (cinfo.next_scanline < cinfo.image_height) {
               /* convert scanline from ARGB to RGB packed */
               for (j = 0; j < w; j++)
                       memcpy (&(buf[j*3]), &(ptr[i*rowstride + j*3]), 3);

               /* write scanline */
               jbuf = (JSAMPROW *)(&buf);
               jpeg_write_scanlines (&cinfo, jbuf, 1);
               i++;
               y++;

       }
       
       /* finish off */
       jpeg_finish_compress (&cinfo);   
       free (buf);
       fclose(f);
       return TRUE;
}
#endif

#if defined (HAVE_JPEG) || defined(HAVE_PNG)
static void gdk_pixbuf_file_selection_unrealize(GtkWidget *widget, gpointer data)
{
	struct pixbuf_and_fs *paf = (struct pixbuf_and_fs *) data;
	gdk_pixbuf_unref(paf->buf);
	free(paf);
}

static void gdk_pixbuf_file_name_chosen(GtkWidget *widget, gpointer data)
{
	struct pixbuf_and_fs *paf = (struct pixbuf_and_fs *) data;
	GdkPixbuf *buf = paf->buf;
	gboolean result = FALSE;
	char *file = gtk_file_selection_get_filename (GTK_FILE_SELECTION (paf->fs));
	gdk_pixbuf_ref(buf);

#ifdef HAVE_JPEG
	if (!strcmp(&file[strlen(file)-4], ".jpg"))
		result = gdk_pixbuf_save_jpg(buf, file, 100);
	else
#endif
#ifdef HAVE_PNG
	if (!strcmp(&file[strlen(file)-4], ".png"))
		result = gdk_pixbuf_save_png(buf, file);
	else
#endif
	{
		gtk_show_text_window(STUDIO_ERROR,
			"Unknown file extension",
#if defined(HAVE_PNG) && defined(HAVE_JPEG)
			"Please choose a *.png/*.jpg file"
#elif defined (HAVE_JPEG)
			"Please choose a *.jpg file"
#elif define (HAVE_PNG)
			"Please choose a *.png file"
#endif
			);
		gdk_pixbuf_save_to_file (buf);
		return;
	}

	if (!result)
		gtk_show_text_window(STUDIO_ERROR,
			"Encoding the image failed", NULL);

	gdk_pixbuf_unref(buf);
}

void gdk_pixbuf_save_to_file (GdkPixbuf *pixbuf)
{
	GtkWidget *filew;
	struct pixbuf_and_fs *paf = (struct pixbuf_and_fs *) malloc(sizeof(struct pixbuf_and_fs));

#if defined(HAVE_PNG) && defined(HAVE_JPEG)
	filew = gtk_file_selection_new ("Select Location (*.jpg/*.png)");
#elif defined (HAVE_JPEG)
	filew = gtk_file_selection_new ("Select Location (*.jpg)");
#elif define (HAVE_PNG)
	filew = gtk_file_selection_new ("Select Location (*.png)");
#endif
	paf->fs = GTK_FILE_SELECTION(filew);
	paf->buf = pixbuf;

	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		"clicked", (GtkSignalFunc) gdk_pixbuf_file_name_chosen, (gpointer)paf);
	gtk_signal_connect_object(GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		"clicked", gtk_widget_destroy, GTK_OBJECT(filew));
	gtk_signal_connect_object(GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),
		"clicked", gtk_widget_destroy, GTK_OBJECT(filew));
	gtk_signal_connect(GTK_OBJECT(filew), "unrealize",
		GTK_SIGNAL_FUNC(gdk_pixbuf_file_selection_unrealize), (gpointer)paf);

	gtk_widget_show(filew);
}
#endif
