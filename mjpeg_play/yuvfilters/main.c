#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "yuvfilters.h"

#ifdef FILTER
#  define MODULE FILTER
#else
# ifdef READER
#  define MODULE READER
# else
#  define MODULE WRITER
# endif
#endif
#ifndef READER
#define READER yuvstdin
#endif
#ifndef WRITER
#define WRITER yuvstdout
#endif

DECLARE_YFTASKCLASS(READER);
DECLARE_YFTASKCLASS(WRITER);
#ifdef FILTER
DECLARE_YFTASKCLASS(FILTER);
#endif

int verbose = 1;

static void
usage(char **argv)
{
  char buf[1024];

  sprintf(buf, "Usage: %s %s\n", argv[0], (*MODULE.usage)());
  write(2, buf, strlen(buf));
}

int
main(int argc, char **argv)
{
  YfTaskCore_t *h, *hreader;
  int ret;
  char *p;

  if (1 < argc && (!strcmp(argv[1], "-?") ||
		   !strcmp(argv[1], "-h") ||
		   !strcmp(argv[1], "--help"))) {
    usage(argv);
    return 0;
  }
  if ((p = getenv("MJPEG_VERBOSITY")))
    verbose = atoi(p);

  ret = 1;
#ifndef FILTER
  if (!(hreader = YfAddNewTask(&READER, argc, argv, NULL)))
    goto RETURN;
#else
  if (!(hreader = YfAddNewTask(&READER, argc, argv, NULL)))
    goto RETURN;
  if (!YfAddNewTask(&FILTER, argc, argv, hreader))
    goto FINI;
#endif
  if (!YfAddNewTask(&WRITER, argc, argv, hreader))
    goto FINI;

  ret = (*READER.frame)(hreader, NULL, NULL);

 FINI:
  for (h = hreader; h; h = hreader) {
    hreader = h->handle_outgoing;
    (*h->method->fini)(h);
  }
 RETURN:
  return ret;
}
