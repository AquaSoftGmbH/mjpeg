/* -*- mode:C; tab-width:8; -*- */
/*
 *  Copyright (C) 2001 Kawamata/Hitoshi <hitoshi.kawamata@nifty.ne.jp>
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

#include <config.h>
#include <stdlib.h>
#include <yst.h>
#include <yuvfilter.h>

typedef struct {
  yst_stream_t *in, *out;
} priv_t;

static int
yuvthru_init(yst_stream_t *in, yst_stream_t *out, void **appdatap,
	       int argc, char **argv)
{
  priv_t *priv;

  if (!(priv = malloc(sizeof *priv)))
    return 1;
  priv->in  = in;
  priv->out = out;
  *appdatap = priv;
  return 0;
}

static int
yuvthru_fini(void *appdata)
{
  free(appdata);
  return 0;
}

static int
yuvthru_frame(void *appdata, yst_frame_t *frame_in)
{
  priv_t *priv = appdata;

  if (!(yst_stream_flags_get(priv->out) & YST_F_STREAM_HEADER_DONE)) {
    yst_extra_info_t *xsinfo = NULL;
    yst_stream_info_t sinfodata;

    yst_stream_info_get(priv->in,  &sinfodata, &xsinfo);
    yst_stream_info_set(priv->out, &sinfodata, &xsinfo);
  }
#if 1
  yst_frame_send(priv->out, frame_in);
#else
  {
    yst_extra_info_t *xfinfo = NULL;
    yst_frame_t *frame_out;
    yst_frame_info_t finfodata;
    unsigned char *yuv_in[3], *yuv_out[3];
    size_t size_in, size_out;

    yst_frame_info_get(       frame_in,  &finfodata, &xfinfo);
    frame_out = yst_frame_new(priv->out, &finfodata, &xfinfo);
    size_in  = yst_frame_data(frame_in,  yuv_in,  NULL);
    size_out = yst_frame_data(frame_out, yuv_out, NULL);
    memcpy(yuv_out[0], yuv_in[0], size_out);
    yst_frame_release(frame_in);
    yst_frame_send(priv->out, frame_out);
  }
#endif
  return 0;
}

const yuvfilter_t yuvthru = {
  yuvthru_init, yuvthru_fini, yuvthru_frame, };
