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
  char idline[24];

  --argc; ++argv;
  if (!(h = YfAllocateTask(&yuvstdout, sizeof *h, h0)))
    return NULL;
  sprintf(idline, "YUV4MPEG %d %d %d\n", h0->width, h0->height, h0->fpscode);
  if (write(1, idline, strlen(idline)) <= 0) {
    perror("(stdout)");
    YfFreeTask(h);
    return NULL;
  }
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
