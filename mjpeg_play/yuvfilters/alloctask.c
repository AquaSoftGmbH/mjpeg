#include <stdio.h>
#include <stdlib.h>
#include "yuvfilters.h"

YfTaskCore_t *
YfAllocateTask(const YfTaskClass_t *filter, size_t size, const YfTaskCore_t *h0)
{
  YfTaskCore_t *h = malloc(size);
  if (!h) {
    perror("malloc");
    return NULL;
  }
  memset(h, 0, size);
  if (h0)
    *h = *h0;
  h->method = filter;
  h->handle_outgoing = NULL;
  return h;
}

void
YfFreeTask(YfTaskCore_t *handle)
{
  free(handle);
}
