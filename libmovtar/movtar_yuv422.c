/* 
movtar_yuv422
=============

  Read a JPEG image and generate a ZR36060 compatible version of it.
  The main task is to convert the YUV-space from 4:1:1 or 4:4:4 to
  4:2:2. 
  Besides that, two images are generated if so necessary.
  The resolution is unchanged.
  (The resolution defines the number of JPEG fields per buffer)
  The result is written as a jpeg to stdout.
  See this more as example code than a really useful tool.

  Usage: movtar_index <options> -i filename 
  (see movtar_index -h for help (or have a look at the function "usage"))
  
  Copyright (C) 2000 Gernot Ziegler (gz@lysator.liu.se)

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
#include <jpeglib.h>

#define MAXPIXELS (1024*1024)  /* Maximum size of final image */

static unsigned char outBuffer[MAXPIXELS*3];

int main(int argc, char **argv)
{
   int img_height, img_width, field = 0, y, ystart;
   unsigned char *addr;
   struct jpeg_decompress_struct dinfo;
   struct jpeg_compress_struct newinfo;
   struct jpeg_error_mgr jerr;

   dinfo.err = jpeg_std_error(&jerr);
   jpeg_create_decompress(&dinfo);
   jpeg_stdio_src(&dinfo, stdin);

   jpeg_read_header(&dinfo, TRUE);
   dinfo.out_color_space = JCS_YCbCr;      
   jpeg_start_decompress(&dinfo);

   if(dinfo.output_components != 3)
     {
       fprintf(stderr,"Output components of JPEG image = %d, must be 3\n",dinfo.output_components);
       exit(1);
     }

   img_width  = dinfo.output_width;
   img_height = dinfo.output_height;
   fprintf(stderr, "Image dimensions are %dx%d\n", img_width, img_height);
   if(img_width*img_height>MAXPIXELS)
     {
       fprintf(stderr,"Img dimension %dx%d too big\n",img_width,img_height);
       exit(1);
     }

   for (y=0; y<img_height; y++)
     {
       addr = outBuffer+y*img_width*3;
       jpeg_read_scanlines(&dinfo, (JSAMPARRAY) &addr, 1);
     }
   
   (void) jpeg_finish_decompress(&dinfo);
   fprintf(stderr,"JPEG file successfully read.\n");

   /* Decompressing finished */
   jpeg_destroy_decompress(&dinfo);

   /* Starting the compression */
   newinfo.err = jpeg_std_error(&jerr);
   jpeg_create_compress(&newinfo);
   jpeg_stdio_dest(&newinfo, stdout);

   newinfo.in_color_space = JCS_YCbCr;      
   jpeg_set_defaults(&newinfo);

   newinfo.image_width = img_width;
   newinfo.input_components = 3;
   newinfo.input_gamma = 1.0;

   /* Set the output JPEG factors fitting the MJPEG codec of the Buz */
   /* These sets the sample ratio of the YUV output (here: 2:1:1) */
   newinfo.comp_info[0].h_samp_factor = 2; 
   newinfo.comp_info[0].v_samp_factor = 1; 
   newinfo.comp_info[1].h_samp_factor = 1; 
   newinfo.comp_info[1].v_samp_factor = 1; 
   newinfo.comp_info[2].h_samp_factor = 1; 
   newinfo.comp_info[2].v_samp_factor = 1; 


   if (img_width < 700)
   { newinfo.image_height = img_height;

     jpeg_start_compress(&newinfo, FALSE);

     for (y=0; y<img_height; y++)
       {
	 addr = outBuffer+y*img_width*3;
	 jpeg_write_scanlines(&newinfo, (JSAMPARRAY) &addr, 1);
       }
     
     jpeg_finish_compress(&newinfo);
     fprintf(stderr, "1 field written.\n");
   }
   else
     {
       newinfo.image_height = img_height/2;

       jpeg_start_compress(&newinfo, FALSE);

       ystart = field;
       for (y=0; y<img_height; y+=2)
	 {
	   addr = outBuffer+y*img_width*3;
	   jpeg_write_scanlines(&newinfo, (JSAMPARRAY) &addr, 1);
	 }
       
       jpeg_finish_compress(&newinfo);
   
       jpeg_start_compress(&newinfo, FALSE);
       
       for (y=1; y<img_height; y+=2)
	 {
	   addr = outBuffer+y*img_width*3;
	   jpeg_write_scanlines(&newinfo, (JSAMPARRAY) &addr, 1);
	 }

       jpeg_finish_compress(&newinfo);
       fprintf(stderr, "2 fields written.\n");
   
     }

   jpeg_destroy_compress(&newinfo);

   return 0;
}




