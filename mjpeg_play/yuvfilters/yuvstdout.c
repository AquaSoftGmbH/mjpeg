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
