/* -*- mode:C -*- */
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

#ifndef __YUVFILTERS_H__
#define __YUVFILTERS_H__

#include <stddef.h>
#ifdef MJPEGTOOLS
# include <string.h>
# include <errno.h>
# include <mjpeg_logging.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MJPEGTOOLS
# define perror(MSG) mjpeg_error("%s: %s\n", MSG, strerror(errno))
# define WERROR(MSG) mjpeg_error(MSG)
# define WERRORL(MSG) mjpeg_error(MSG)
# define WWARN(MSG) mjpeg_warn(MSG)
#else
# define WERROR(MSG) write(2, MSG, sizeof MSG - 1)
# define WERRORL(MSG) write(2, MSG, strlen(MSG))
# define WWARN(MSG) WERROR("warning: " MSG)
#endif

typedef struct {
  char id[sizeof "FRAME\n" - 1];
  unsigned char data[0];
} YfFrame_t;

#define DATABYTES(W,H) (((W)*(H))+(((W)/2)*((H)/2)*2))
#define FRAMEBYTES(W,H) (sizeof ((YfFrame_t *)0)->id + DATABYTES(W,H))

struct YfTaskClass_tag;

typedef struct YfTaskCore_tag {
  /* private: filter may not touch */
  const struct YfTaskClass_tag *method;
  struct YfTaskCore_tag *handle_outgoing;
  /* protected: filter must set */
  int width, height, fpscode;
} YfTaskCore_t;

typedef struct YfTaskClass_tag {
  const char *(*usage)(void);
  YfTaskCore_t *(*init)(int argc, char **argv, const YfTaskCore_t *h0);
  void (*fini)(YfTaskCore_t *handle);
  int (*frame)(YfTaskCore_t *handle, const YfTaskCore_t *h0, const YfFrame_t *frame0);
} YfTaskClass_t;


#define DECLARE_YFTASKCLASS(name) \
extern const YfTaskClass_t name

#define DEFINE_YFTASKCLASS(sclass, prefix, name) \
sclass const char *prefix##_usage(void); \
sclass YfTaskCore_t *prefix##_init(int argc, char **argv, const YfTaskCore_t *h0); \
sclass void prefix##_fini(YfTaskCore_t *handle); \
sclass int prefix##_frame(YfTaskCore_t *handle, const YfTaskCore_t *h0, const YfFrame_t *frame0); \
const YfTaskClass_t name = { prefix##_usage, prefix##_init, prefix##_fini, prefix##_frame, }

#define DEFINE_STD_YFTASKCLASS(name) DEFINE_YFTASKCLASS(static,do,name)

extern int verbose;

extern YfTaskCore_t *YfAllocateTask(const YfTaskClass_t *filter, size_t size, const YfTaskCore_t *h0);
extern void YfFreeTask(YfTaskCore_t *handle);
extern YfFrame_t *YfInitFrame(YfFrame_t *frame, const YfTaskCore_t *h0);
extern int YfPutFrame(const YfTaskCore_t *handle, const YfFrame_t *frame);
extern YfTaskCore_t *YfAddNewTask(const YfTaskClass_t *filter,
			    int argc, char **argv, const YfTaskCore_t *h0);
#ifdef __cplusplus
}
#endif

#endif
