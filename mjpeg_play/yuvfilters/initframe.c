#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yuvfilters.h"

YfFrame_t *
YfInitFrame(YfFrame_t *frame, const YfTaskCore_t *h0)
{
  if (!frame) {
    if (!(frame = malloc(FRAMEBYTES(h0->width, h0->height)))) {
      perror("malloc");
      return NULL;
    }
  }
  strncpy(frame->id, "FRAME\n", sizeof frame->id);
  return frame;
}
