/* -*- mode:C; tab-width:8; -*- */
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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef USE_DLFCN_H
# include <dlfcn.h>
#endif
#ifdef MJPEGTOOLS
# include <mjpeg_logging.h>
# define INFO(msg) mjpeg_info("%s", msg)
# define WARN(msg) mjpeg_warn("%s", msg)
# define ERROR(msg) mjpeg_error("%s", msg)
# define DEBUG(msg) mjpeg_debug("%s", msg)
# define INFO1(fmt,a1) mjpeg_info(fmt,a1)
# define ERROR1(fmt,a1) mjpeg_error(fmt,a1)
#else
# include <stdio.h>
# define INFO(msg) fprintf(stderr, "%s", msg)
# define WARN(msg) fprintf(stderr, "%s", msg)
# define ERROR(msg) fprintf(stderr, "%s", msg)
# define DEBUG(msg) fprintf(stderr, "%s", msg)
# define INFO1(fmt,a1) fprintf(stderr, fmt, a1)
# define ERROR1(fmt,a1) fprintf(stderr, fmt, a1)
#endif
#include <yst_private.h>
#include <yuvfilter.h>
#include "yuvfilter_static.h"

typedef struct task_tag {
  yst_stack_redirector_t redirector;
  struct task_tag *next;
#ifdef USE_DLFCN_H
  void *yuvfilter_so;
#endif
  const yuvfilter_t *yuvfilter;
  void *appdata;
  int argc;
  char **argv;
  yst_stream_t *in, *out;
} task_t;

static yst_status_t
redirect(yst_frame_t *frame, yst_stack_desc_t *desc)
{
  task_t *task = (task_t *)desc;
  return task->yuvfilter->frame(task->appdata, frame);
}

int
main(int argc, char **argv)
{
  char *yuvfilter_name;
  unsigned int format = YST_F_FMT_WRITE;
#ifdef USE_DLFCN_H
  int any_version = 0;
#endif
  task_t *task0 = NULL, **taskp, *task;
  yst_stream_t *in, *out;
  int status;
  yst_frame_t *frame;

  if (!(in = yst_stream_init(YST_F_MEDIA_FILE|YST_F_RW_READ, NULL, NULL))) {
    ERROR("cannot initialize input stream (memory exhausted?)\n");
    return 1;
  }

  if ((yuvfilter_name = strrchr(argv[0], '/')))
    yuvfilter_name++;
  else
    yuvfilter_name = argv[0];

  /* parse options */
  if (!strcmp(yuvfilter_name, "yuvfilter")) {
    yst_stream_info_t sinfodata;
    yst_extra_info_t *xsinfo = NULL;
    int print_help = 0, print_version = 0;
#ifdef MJPEGTOOLS
    int verbose = 1;
#endif
    const static char fmts[] =
#ifdef USE_Y4M_PLAIN_FIELD
		   ",plain"
#endif
#ifdef USE_Y4M_TAGGED_FIELD
		   ",tag"
#endif
#ifdef USE_Y4M_NAMED_FIELD
		   ",name"
#endif
#ifdef USE_VIDEO_RAW
		   ",raw"
#endif
      ;
    const static char opts_short[] =
#ifdef USE_DLFCN_H
      "a"
#endif
#ifdef MJPEGTOOLS
      "v"
#endif
      "hVp:f:";
    const static struct option opts_long[] = {
#ifdef USE_DLFCN_H
      { "any-verion", no_argument,       NULL, 'a', },
#endif
#ifdef MJPEGTOOLS
      { "verbose",    required_argument, NULL, 'v', },
#endif
      { "help",       no_argument,       NULL, 'h', },
      { "version",    no_argument,       NULL, 'V', },
      { "property",   required_argument, NULL, 'p', },
      { "format",     required_argument, NULL, 'f', },
      { NULL, 0, NULL, 0, },
    };

    yst_stream_info_get(in, &sinfodata, &xsinfo);
    for (;;) {
      switch (getopt_long(argc, argv, opts_short, opts_long, NULL)) {
#ifdef USE_DLFCN_H
      case 'a':
	any_version = 1;
	break;
#endif
#ifdef MJPEGTOOLS
      case 'v':
	verbose = atoi(optarg);
	break;
#endif
      case 'h':
	print_help = 1;
	break;
      case 'V':
	print_version = 1;
	break;
      case 'p':
	yst_stream_property_parse(optarg, &sinfodata, &xsinfo);
	break;
      case 'f':
	if      (!strcmp(optarg, "default")) format = YST_F_FMT_WRITE;
#ifdef USE_Y4M_PLAIN_FIELD
	else if (!strcmp(optarg, "plain"))   format = YST_F_FMT_Y_PLAIN;
#endif
#ifdef USE_Y4M_TAGGED_FIELD
	else if (!strcmp(optarg, "tag"))     format = YST_F_FMT_Y_TAG;
#endif
#ifdef USE_Y4M_NAMED_FIELD
	else if (!strcmp(optarg, "name"))    format = YST_F_FMT_Y_NAME;
#endif
#ifdef USE_VIDEO_RAW
	else if (!strcmp(optarg, "raw"))     format = YST_F_FMT_V_RAW;
#endif
	else {
	  ERROR("invalid format\n");
	  print_help = 1;
	}
	break;
      default:
	goto OPTS_END;
      }
    }
  OPTS_END:
#ifdef MJPEGTOOLS
    mjpeg_default_handler_verbosity(verbose);
#endif
    argc -= optind;
    argv += optind;
    optind = 0;
    if (print_version) {
      INFO("yuvfilter (" PACKAGE ") " VERSION "\n");
      INFO("Copyright (C) 2001 Kawamata/Hitoshi. "
	   "<hitoshi.kawamata@nifty.ne.jp>\n");
      return 0;
    }
    if (print_help || argc <= 0) {
      INFO("Usage: [yuvfilter [OPTION]...] "
	   "COMMAND [OPTION]... ['|' COMMAND [OPTION]...]...\n");
      INFO("Options:\n");
      INFO("  -h, --help            print this help\n");
      INFO("  -V, --version         print version\n");
#ifdef USE_DLFCN_H
      INFO("  -a, --any-version     search plugin without version suffix\n");
#endif
#ifdef MJPEGTOOLS
      INFO("  -v, --verbose LEVEL   set verbosity level {0,1,2} "
	   "(default: 1)\n");
#endif
      INFO("  -p, --property STRING set default property of input stream\n");
      INFO1("  -f, --format FORMAT   set output header format {default%s}\n",
	    fmts);
#ifdef WRITE_Y4M_NAMED_FIELD
      INFO("                        (default: name)\n");
#else
      INFO("                        (default: tag)\n");
#endif
      return 1;
    }
    yst_stream_info_set(in, &sinfodata, &xsinfo);
    yuvfilter_name = argv[0];
  }

  /* initialize tasks */
  out = NULL;
  taskp = &task0;
  do {
#ifdef USE_DLFCN_H
    char yuvfilter_so_name[128];
    const char *const *prefix;
    static const char *const prefixes[] = {
      PKGLIBDIR "/lib",
      "lib",
      NULL, };
#else
    const yuvfilter_desc_t *desc;
#endif

    if (!(task = (task_t *)malloc(sizeof *task))) {
      ERROR("memory exhausted\n");
      status = 1;
      goto FINI;
    }
    memset(task, 0, sizeof *task);
    *taskp = task;
    taskp = &task->next;
    task->redirector = redirect;
    task->argv = argv;
    while (*argv && strcmp(*argv, "|")) {
      task->argc++;
      argv++;
      argc--;
    }
    *argv++ = NULL;
    argc--;

    if (out == in)		/* not 1st */
      in->media.stack.desc = (yst_stack_desc_t *)task;
    if (!(out = yst_stream_init((0 < argc)?
				(YST_F_RW_WRITE|YST_F_MEDIA_STACK|format):
				(YST_F_RW_WRITE|YST_F_MEDIA_FILE|format),
				NULL, NULL))) {
      ERROR("cannot initialize output stream (memory exhausted?)\n");
      status = 1;
      goto FINI;
    }
    task->in  = in;
    task->out = out;

#ifdef USE_DLFCN_H
    for (prefix = prefixes; *prefix; prefix++) {
      strcat(strcat(strcpy(yuvfilter_so_name, *prefix), yuvfilter_name),
	     any_version? ".so": "-" VERSION ".so");
      if ((task->yuvfilter_so = dlopen(yuvfilter_so_name, RTLD_LAZY)))
	goto DLOPENED;
    }
    ERROR1("plugin %s not found\n", yuvfilter_name);
    status = 1;
    goto FINI;
  DLOPENED:
    task->yuvfilter = (yuvfilter_t *)dlsym(task->yuvfilter_so, yuvfilter_name);
#else
    for (desc = yuvfilter_descs; desc->name; desc++) {
      if (!strcmp(desc->name, yuvfilter_name)) {
	task->yuvfilter = desc->yuvfilter;
	break;
      }
    }
#endif
    if (!task->yuvfilter) {
      ERROR1("entry table of %s not found\n", yuvfilter_name);
      status = 1;
      goto FINI;
    }
    if ((status = task->yuvfilter->init(in, out, &task->appdata,
					task->argc, task->argv))) {
      goto FINI;
    }
    in = out;
    yuvfilter_name = argv[0];
  } while (0 < argc);

  /* main loop */
  while ((frame = yst_frame_receive(task0->in, NULL, NULL, NULL)))
    if ((status = task0->yuvfilter->frame(task0->appdata, frame)))
      goto FINI;
  if (task0->in->error && task0->in->error != YST_E_EOD)
    ERROR1("%s\n", yst_errstr(task0->in->error)); /* FIXME: other streams? */
  for (task = task0; task; task = task->next)
    task->yuvfilter->frame(task->appdata, NULL); /* notify EOD */

 FINI:
  if (task0 && task0->in)
    yst_stream_fini(task0->in);
  while (task0) {
    int status_fini;
    task = task0->next;
    if ((status_fini = task0->yuvfilter->fini(task0->appdata)) && !status)
      status = status_fini;
#ifdef USE_DLFCN_H
    dlclose(task0->yuvfilter_so);
#endif
    yst_stream_fini(task0->out);
    free(task0);
    task0 = task;
  }
  return status;
}
