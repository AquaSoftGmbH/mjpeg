#include "yuvfilters.h"

YfTaskCore_t *
YfAddNewTask(const YfTaskClass_t *filter,
	  int argc, char **argv, const YfTaskCore_t *h0)
{
  YfTaskCore_t *h;
  if (h0) {
    while (h0->handle_outgoing)
      h0 = h0->handle_outgoing;
    h = (*filter->init)(argc, argv, h0);
    if (!h)
      return NULL;
    while (h0->handle_outgoing)
      h0 = h0->handle_outgoing;
    *(YfTaskCore_t **)&h0->handle_outgoing = h;
  } else
    h = (*filter->init)(argc, argv, h0);
  return h;
}
