/*
  *  yuvcorrect_functions.c
  *  Common functions between yuvcorrect and yuvcorrect_tune
  *  Copyright (C) 2002 Xavier Biquard <xbiquard@free.fr>
  * 
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *
  *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with this program; if not, write to the Free Software
  *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  */

// *************************************************************************************

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include "yuv4mpeg.h"
#include "yuvcorrect.h"

#define yuvcorrect_VERSION LAVPLAY_VERSION
// For pointer adress alignement
#define ALIGNEMENT 16		// 16 bytes alignement for mmx registers in SIMD instructions for Pentium

const float PI = 3.141592654;



// *************************************************************************************
int
yuvcorrect_y4m_read_frame (int fd, frame_t *frame, general_correction_t *gen_correct)
{
  // This function reads a frame from input stream. 
  // It is the same as the y4m_read_frame function (from y4mpeg.c) except that line switching
  // is done during frame read
  static int err = Y4M_OK;
  unsigned int line;
  uint8_t *buf;
  buf = frame->y;
   
  if ((err = y4m_read_frame_header (fd, &frame->info)) == Y4M_OK)
    {
      if (!gen_correct->line_switch)
	{
	  if ((err = y4m_read (fd, buf, frame->length)) != Y4M_OK)
	    {
	      mjpeg_info ("Couldn't read FRAME content: %s!",
			  y4m_strerr (err));
	      return (err);
	    }
	}
      else
	{
	  // line switching during frame read 
	  // Y COMPONENT
	  for (line = 0; line < frame->y_height; line += 2)
	    {
	      buf += frame->y_width;	// buf points to next line on output, store input line there
	      if ((err = y4m_read (fd, buf, frame->y_width)) != Y4M_OK)
		{
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
		     line, y4m_strerr (err));
		  return (err);
		}
	      buf -= frame->y_width;	// buf points to former line on output, store input line there
	      if ((err = y4m_read (fd, buf, frame->y_width)) != Y4M_OK)
		{
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
		     line + 1, y4m_strerr (err));
		  return (err);
		}
	      buf += (frame->y_width << 1);	// 2 lines were read and stored
	    }
	  // U and V component
	  for (line = 0; line < (frame->uv_height<<1); line += 2)
	    {
	      buf += frame->uv_width ;	// buf points to next line on output, store input line there
	      if ((err = y4m_read (fd, buf, frame->uv_width)) != Y4M_OK)
		{
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
		     line, y4m_strerr (err));
		  return (err);
		}
	      buf -= frame->uv_width;	// buf points to former line on output, store input line there
	      if ((err = y4m_read (fd, buf, frame->uv_width)) != Y4M_OK)
		{
		  mjpeg_info
		    ("Couldn't read FRAME content line %d : %s!",
		     line + 1, y4m_strerr (err));
		  return (err);
		}
	      buf += (frame->uv_width << 1);	// two line were read and stored
	    }
	}
    }
  else
    {
      if (err != Y4M_ERR_EOF)
	mjpeg_info ("Couldn't read FRAME header: %s!", y4m_strerr (err));
      else
	mjpeg_info ("End of stream!");
      return (err);
    }
  return Y4M_OK;
}
// *************************************************************************************


// *************************************************************************************
int
yuvcorrect_luminance_init (yuv_correction_t *yuv_correct)
{
  // This function initialises the luminance vector
  uint8_t *u_c_p;		//u_c_p = uint8_t pointer
  uint16_t i;

  // Filling in the luminance vector
  u_c_p = yuv_correct->luminance;
  for (i = 0; i < 256; i++)
    {
      if (i <= yuv_correct->InputYmin)
	*(u_c_p++) = yuv_correct->OutputYmin;
      else
	{
	  if (i >= yuv_correct->InputYmax)
	    *(u_c_p++) = yuv_correct->OutputYmax;
	  else
	    *(u_c_p++) = yuv_correct->OutputYmin +
	      floor (0.5 +
		     pow (
			  (float) (i - yuv_correct->InputYmin) /
			  (float) (yuv_correct->InputYmax - yuv_correct->InputYmin),
			  (float) 1 / yuv_correct->Gamma
			  ) 
		     * (yuv_correct->OutputYmax - yuv_correct->OutputYmin));


	}
//      mjpeg_debug ("Luminance[%u]=%u", i, yuv_correct->luminance[i]);
    }

  return (0);
}
// *************************************************************************************


// *************************************************************************************
int
yuvcorrect_chrominance_init (yuv_correction_t *yuv_correct)
{
  // This function initialises the UVchroma vector
  uint8_t *u_c_p;		//u_c_p = uint8_t pointer
  uint16_t u, v;		// do not use uint8_t, else you get infinite loop for u and v because 255+1=0 in unit8_t 
  float newU, newV, cosinus, sinus;

  mjpeg_debug ("chroma init");
  cosinus = cos (yuv_correct->UVrotation / 180.0 * PI);
  sinus = sin (yuv_correct->UVrotation / 180.0 * PI);
  // Filling in the chrominance vector
  u_c_p = yuv_correct->chrominance;
  for (u = 0; u <= 255; u++)
    {
      for (v = 0; v <= 255; v++)
	{
	  newU =
	    (((float) (u - yuv_correct->Urotcenter) * yuv_correct->Ufactor) * cosinus -
	     ((float) (v - yuv_correct->Vrotcenter) * yuv_correct->Vfactor) * sinus) + 128.0;
//          mjpeg_debug("u=%u, v=%u, newU=%f",u,v,newU);
	  newU = (float) floor (0.5 + newU);	// nearest integer in double format
	  if (newU < yuv_correct->UVmin)
	    newU = yuv_correct->UVmin;
	  if (newU > yuv_correct->UVmax)
	    newU = yuv_correct->UVmax;
	  newV =
	    (((float) (v - yuv_correct->Vrotcenter) * yuv_correct->Vfactor) * cosinus +
	     ((float) (u - yuv_correct->Urotcenter) * yuv_correct->Ufactor) * sinus) + 128.0;
//          mjpeg_debug("u=%u, v=%u, newV=%f",u,v,newV);
	  newV = (float) floor (0.5 + newV);	// nearest integer in double format
	  if (newV < yuv_correct->UVmin)
	    newV = yuv_correct->UVmin;
	  if (newV > yuv_correct->UVmax)
	    newV = yuv_correct->UVmax;
	  *(u_c_p++) = (uint8_t) newU;
	  *(u_c_p++) = (uint8_t) newV;
	}
    }
  mjpeg_debug ("end of chroma init");
  return (0);
}
// *************************************************************************************


// *************************************************************************************
int
yuvcorrect_luminance_treatment (frame_t *frame, yuv_correction_t *yuv_correct)
{
  // This function corrects the luminance of input
  uint8_t *u_c_p;
  uint32_t i;

  u_c_p = frame->y;
  // Luminance (Y component)
  for (i = 0; i < frame->nb_y; i++)
    *(u_c_p++) = yuv_correct->luminance[*(u_c_p)];

  return (0);
}
// *************************************************************************************


// *************************************************************************************
int
yuvcorrect_chrominance_treatment (frame_t *frame, yuv_correction_t *yuv_correct)
{
  // This function corrects the chrominance of input
  uint8_t *Uu_c_p, *Vu_c_p;
  uint32_t i, base;

//   mjpeg_debug("Start of yuvcorrect_chrominance_treatment(%p, %lu, %p)",input,size,UVchroma); 
  Uu_c_p = frame->u;
  Vu_c_p = frame->v;

  // Chroma
  for (i = 0; i < frame->nb_uv; i++)
    {
      base = ((((uint32_t)*Uu_c_p) << 8) + (*Vu_c_p)) << 1; // base = ((((uint32_t)*Uu_c_p) * 256) + (*Vu_c_p)) * 2
      *(Uu_c_p++) = yuv_correct->chrominance[base++];
      *(Vu_c_p++) = yuv_correct->chrominance[base];
    }
//   mjpeg_debug("End of yuvcorrect_chrominance_treatment");
  return (0);
}
// *************************************************************************************



// *************************************************************************************
int
bottom_field_storage (frame_t * frame, uint8_t oddeven, uint8_t * field1,
		      uint8_t * field2)
{
  int ligne;
  uint8_t *u_c_p;
  // This function stores the current bottom field into tabular field[1 or 2] 
  u_c_p = frame->y;
  u_c_p += frame->y_width; // first pixel of the bottom field
  if ( oddeven ) 
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field1, u_c_p, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field1 += frame->y_width;
	}
      u_c_p -= frame->uv_width;
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field1, u_c_p, frame->uv_width);
	  u_c_p += frame->y_width;
	  field1 += frame->uv_width;
	}
    }
  else
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field2, u_c_p, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field2 += frame->y_width;
	}
      u_c_p -= frame->uv_width;
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field2, u_c_p, frame->uv_width);
	  u_c_p += frame->y_width;
	  field2 += frame->uv_width;
	}
    }
  return (0);
}
// *************************************************************************************


// *************************************************************************************
int
top_field_storage (frame_t * frame, uint8_t oddeven, uint8_t * field1,
		      uint8_t * field2)
{
  int ligne;
  uint8_t *u_c_p;
  // This function stores the current bottom field into tabular field[1 or 2] 
  u_c_p = frame->y;
//  u_c_p += frame->y_width; // first pixel of the bottom field
  if ( oddeven ) 
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field1, u_c_p, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field1 += frame->y_width;
	}
//      u_c_p -= frame->uv_width;
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field1, u_c_p, frame->uv_width);
	  u_c_p += frame->y_width;
	  field1 += frame->uv_width;
	}
    }
  else
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field2, u_c_p, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field2 += frame->y_width;
	}
//      u_c_p -= frame->uv_width;
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (field2, u_c_p, frame->uv_width);
	  u_c_p += frame->y_width;
	  field2 += frame->uv_width;
	}
    }
  return (0);
}
// *************************************************************************************


// *************************************************************************************
int
bottom_field_replace (frame_t * frame, uint8_t oddeven, uint8_t * field1,
		      uint8_t * field2)
{
  int ligne;
  uint8_t *u_c_p;
  // This function replaces the current bottom field with tabular field[1 or 2] 
  u_c_p = frame->y;
  u_c_p += frame->y_width;

  if (oddeven)
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field2, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field2 += frame->y_width;
	}
      u_c_p -= frame->uv_width;
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field2, frame->uv_width);
	  u_c_p +=  frame->y_width;
	  field2 += frame->uv_width;
	}
    }
  else
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field1, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field1 += frame->y_width;
	}
      u_c_p -= frame->uv_width;
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field1, frame->uv_width);
	  u_c_p +=  frame->y_width;
	  field1 += frame->uv_width;
	}
    }
  return (0);
}
// *************************************************************************************


// *************************************************************************************
int
top_field_replace (frame_t * frame, uint8_t oddeven, uint8_t * field1,
		      uint8_t * field2)
{
  int ligne;
  uint8_t *u_c_p;
  // This function replaces the current bottom field with tabular field[1 or 2] 
  u_c_p = frame->y;
//  u_c_p += frame->y_width;

  if (oddeven)
    {
      // field2
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field2, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field2 += frame->y_width;
	}
//      u_c_p -= (frame->y_width >> 1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field2, frame->uv_width);
	  u_c_p +=  frame->y_width;
	  field2 += frame->uv_width;
	}
    }
  else
    {
      // field1
      // Y Component
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field1, frame->y_width);
	  u_c_p += (frame->y_width << 1);
	  field1 += frame->y_width;
	}
//      u_c_p -= (frame->y_width >> 1);
      // U and V COMPONENTS
      for (ligne = 0; ligne < frame->y_height; ligne += 2)
	{
	  memcpy (u_c_p, field1, frame->uv_width);
	  u_c_p +=  frame->y_width;
	  field1 += frame->uv_width;
	}
    }
  return (0);
}
// *************************************************************************************


// *************************************************************************************
void
yuvstat (frame_t *frame)
{
  uint8_t y, u, v;
  uint8_t *input;
//  int r, g, b;
  unsigned long int somme_y = 0, somme_u = 0, somme_v = 0, moy_y = 0, moy_u = 0, moy_v = 0;
  uint16_t histo_y[256], histo_u[256], histo_v[256];
//  uint16_t histo_r[256], histo_g[256], histo_b[256];
  unsigned long int i;
  unsigned long int decalage = frame->nb_uv;
  unsigned long int decalage_y_u = decalage << 2;
  unsigned long int decalage_y_v = decalage * 5;
  input = frame->y;
  for (i = 0; i < 256; i++)
    histo_y[i] = histo_u[i] = histo_v[i] = 0;
 //   histo_r[i] = histo_g[i] = histo_b[i] = 0;
  for (i = 0; i < decalage; i++)
    {
      y = input[i * 4];
      u = input[i + decalage_y_u];
      v = input[i + decalage_y_v];
      histo_y[y]++;
      histo_u[u]++;
      histo_v[v]++;
//     r = (int) y + (int) floor (1.375 * (float) (v - 128));
//      g = (int) y + (int) floor (-0.698 * (v - 128) - 0.336 * (u - 128));
//      b = (int) y + (int) floor (1.732 * (float) (u - 128));
//      if ((r < 0) || (r > 255))
//	{
//           mjpeg_debug("r out of range %03d, %03u, %03u, %03u",r,y,u,v);
//	  if (r < 0)
//	    r = 0;
//	  else
//	    r = 255;
//	}
//      if ((g < 0) || (g > 255))
//	{
//           mjpeg_debug("g out of range %03d, %03u, %03u, %03u",g,y,u,v);
//	  if (g < 0)
//	    g = 0;
//	  else
//	    g = 255;
//	}
//      if ((b < 0) || (b > 255))
//	{
//           mjpeg_debug("b out of range %03d, %03u, %03u, %03u",b,y,u,v);
//	  if (b < 0)
//	    b = 0;
//	  else
//	    b = 255;
//	}
//      histo_r[r]++;
//      histo_g[g]++;
//      histo_b[b]++;
    }
  mjpeg_info ("Histogramme\ni Y U V");
  for (i = 0; i < 256; i++)
    {
      mjpeg_info ("%03lu %05u %05u %05u", i, histo_y[i], histo_u[i],
		   histo_v[i]);
      somme_y += histo_y[i];
      somme_u += histo_u[i];
      somme_v += histo_v[i];
    }
  i = 0;
  while (moy_y < somme_y / 2)
    moy_y += histo_y[i++];
  moy_y = i;
  i = 0;
  while (moy_u < somme_u / 2)
    moy_u += histo_u[i++];
  moy_u = i;
  i = 0;
  while (moy_v < somme_v / 2)
    moy_v += histo_v[i++];
  moy_v = i;

  mjpeg_info ("moyY=%03lu moyU=%03lu moyV=%03lu", moy_y, moy_u, moy_v);


//  mjpeg_debug ("Histogramme\ni R G B");
//  for (i = 0; i < 256; i++)
//    mjpeg_debug ("%03lu %04u %04u %04u", i, histo_r[i], histo_g[i],
//		 histo_b[i]);

}
// *************************************************************************************


/* 
 * Local variables:
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
