/* -*- mode:C -*- */

#ifndef TIMECODE_H
#define TIMECODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char h, m, s, f;
} MPEG_timecode_t;

extern int dropframetimecode;
extern int mpeg_timecode(MPEG_timecode_t *tc, int f, int fpscode);
/* mpeg_timecode() return -tc->f on first frame in the minute, tc->f on other. */

#ifdef __cplusplus
}
#endif

#endif
