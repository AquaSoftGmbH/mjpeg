# Process this file with Automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/utils

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(ALTIVEC_CFLAGS)

noinst_LIBRARIES = libaltivec.a

libaltivec_a_SOURCES = \
  detect.c \
  motion.c \
  quantize.c \
  benchmark.c \
  amber.c \
  sad_00.c \
  sad_01.c \
  sad_10.c \
  sad_11.c \
  bsad.c \
  sub_mean_reduction.c \
  sub_mean_reduction_ppc.S \
  sumsq.c \
  sumsq_sub22.c \
  bsumsq.c \
  bsumsq_sub22.c \
  build_sub44_mests.c \
  build_sub22_mests.c \
  find_best_one_pel.c \
  fdct_idct.c \
  idct.c \
  quant_non_intra.c \
  quant_weight_coeff_sum.c \
  iquant_non_intra_m1.c \
  variance.c \
  sub_pred.c \
  add_pred.c \
  pred_comp.c \
  subsample_image.c \
  altivec_conf.h \
  altivec_motion.h \
  altivec_predict.h \
  altivec_quantize.h \
  altivec_transform.h \
  benchmark.h \
  amber.h \
  vectorize.h \
  verify.h

EXTRA_DIST = .cvsignore

MAINTAINERCLEANFILES = Makefile.in

