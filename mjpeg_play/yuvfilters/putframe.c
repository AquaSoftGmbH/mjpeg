#include "yuvfilters.h"

int
YfPutFrame(const YfTaskCore_t *handle, const YfFrame_t *frame)
{
  return (*handle->handle_outgoing->method->frame)(handle->handle_outgoing,
						   handle, frame);
}
