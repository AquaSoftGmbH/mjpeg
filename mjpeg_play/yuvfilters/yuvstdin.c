#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "yuvfilters.h"

DEFINE_STD_YFTASKCLASS(yuvstdin);

static const char *
do_usage()
{
  return "";
}

static YfTaskCore_t *
do_init(int argc, char **argv, const YfTaskCore_t *h0)
{
  int n, framebytes;
  char *p;
  YfTaskCore_t *h;
  int width, height, fpscode;
  int idbytes;
  char *buf;
  char idline[24];

  --argc; ++argv;
  n = read(0, idline, sizeof "YUV4MPEG 9990 9990 0\n" - 1);
  if (n <= 0) {
    if (!n)
      WERROR("(stdin): no data\n");
    else
      perror("(stdin)");
    return NULL;
  }
  idline[n] = '\0';
  p = strchr(idline, '\n');
  if (!p || sscanf(idline, "YUV4MPEG %d %d %d ",
		   &width, &height, &fpscode) != 3) {
    WERROR("(stdin): illeagal stream ID\n");
    return NULL;
  }
  p++;
  idbytes = p - idline;
  framebytes = FRAMEBYTES(width, height);
  h = YfAllocateTask(&yuvstdin, sizeof *h + framebytes, h0);
  if (!h)
    return NULL;
  h->width   = width;
  h->height  = height;
  h->fpscode = fpscode;
  buf = (char *)(h + 1);
  n -= idbytes;
  if (0 < n)
    memcpy(buf, p, n);
  if (n < sizeof ((YfFrame_t *)0)->id)
    n += read(0, buf + n, sizeof ((YfFrame_t *)0)->id - n);
  if (n != sizeof ((YfFrame_t *)0)->id) {
    perror("(stdin): no frame");
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
do_frame(YfTaskCore_t *handle, const YfTaskCore_t *h0, const YfFrame_t *frame)
{
  YfTaskCore_t *h = handle;
  int n, framebytes;
  int ret, warning;
  char *p;

  framebytes = FRAMEBYTES(h->width, h->height);
  ret = 0;
  warning = 0;
  n = sizeof ((YfFrame_t *)0)->id;
  p = (char *)(h + 1);
  for (;;) {
    while (n < framebytes) {
      int n0 = read(0, p + n, framebytes - n);
      if (n0 < 0) {
	perror("(stdin)");
	ret = 1;
	goto END;
      } else if (n0 == 0) {
	if (n) {
	  WERROR("(stdin): illeagal EOF in frame\n");
	  ret = 1;
	}
	goto END;
      }
      n += n0;
    }
    if (strncmp(p, "FRAME\n", sizeof ((YfFrame_t *)0)->id)) {
      if (warning++ < 10) {
	WERROR("(stdin): warning: illeagal frame ID\n");
	strncpy(p, "FRAME\n", sizeof ((YfFrame_t *)0)->id);
	continue;
      } else {
	WERROR("(stdin): illeagal frame ID, give up!\n");
      }
      ret = 1;
      break;
    }
    if ((ret = YfPutFrame(h, (YfFrame_t *)p)))
      break;
    n = 0;
  }
 END:
  return ret;
}
