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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mpegconsts.h>
#include "yuvfilters.h"

DEFINE_STD_YFTASKCLASS(yuvstdout);

static const char *
do_usage()
{
  return "";
}

static YfTaskCore_t *
do_init(int argc, char **argv, const YfTaskCore_t *h0)
{
  YfTaskCore_t *h;
  y4m_stream_info_t si;

  --argc; ++argv;
  if (!(h = YfAllocateTask(&yuvstdout, sizeof *h, h0)))
    return NULL;
  y4m_init_stream_info(&si);
  y4m_si_set_width(&si, h0->width);
  y4m_si_set_height(&si, h0->height);
  y4m_si_set_framerate(&si, mpeg_framerate(h0->fpscode));
  y4m_si_set_interlace(&si, h0->interlace);
  y4m_si_set_sampleaspect(&si, h0->sampleaspect);
  if (y4m_write_stream_header(1, &si) != Y4M_OK) {
    YfFreeTask(h);
    h = NULL;
  }
  y4m_fini_stream_info(&si);
  return h;
}

static void
do_fini(YfTaskCore_t *handle)
{
  YfFreeTask(handle);
}

static int
do_frame(YfTaskCore_t *handle, const YfTaskCore_t *h0, const YfFrame_t *frame0)
{
  int n, ret;
  const char *p = (const char *)frame0;

  n = FRAMEBYTES(h0->width, h0->height);
  while ((ret = write(1, p, n)) < n) {
    if (ret < 0) {
      perror("(stdout)");
      return 1;
    }
    p += ret;
    n -= ret;
  }
  return 0;
}
