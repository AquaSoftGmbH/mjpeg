/* -*- mode:C -*- */

#ifndef YUVFILTERS_H
#define YUVFILTERS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WERROR(MSG) write(2, MSG, sizeof MSG - 1)

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
