#ifndef __STUDIO_PIPES_H__
#define __STUDIO_PIPES_H__

#define LAV2WAV 0       /* audio encoding, lavencode.c */
#define MP2ENC 1        /* audio encoding, lavencode.c */
#define LAV2YUV 2       /* video encoding, lavencode.c */
#define YUVSCALER 3     /* video encoding, lavencode.c */
#define MPEG2ENC 4      /* video encoding, lavencode.c */
#define MPLEX 5         /* mplex of video/audio, lavencode.c */
#define LAVPLAY 6       /* lavplay, lavplay_pipe.c */
#define LAVPLAY_E 7     /* lavplay for the editor, lavedit.c */
#define LAVPLAY_T 8     /* lavplay for the trimmer, lavedit_trimming.c */
#define LAVREC 9        /* lavrec, lavrec_pipe.c */
#define LAV2YUV_S 10    /* lav2yuv for scene recognition, lavrec_pipe.c */
#define YUVPLAY 11      /* for seeing the movie while encoding */
#define YUVPLAY_E 12    /* for the effects notebook page in lavedit_effects.c */
#define LAVPIPE 13      /* for the effects notebook page in lavedit_effects.c */
#define YUV2LAV 14      /* for the effects notebook page in lavedit_effects.c */
#define YUVDENOISE 15   /* video encoding, lavencode.c */
#define YUV2DIVX 16     /* for divx encoding, lavencode.c */
#define NUM 17          /* total number of pipes */
#define LAV2YUV_DATA 18 /* a special case for lav2yuv-data-processing */
#define LAVADDWAV 19    /* non-pipe command but needed for app_name/location() */
#define LAVTRANS 20     /* non-pipe command but needed for app_name/location() */

/* Here we define some nice names for the encoding */ 
#define STUDIO_ENC_FORMAT_MPEG 1 /* use mpeg2enc */
#define STUDIO_ENC_FORMAT_DIVX  2 /* use yuv2divx */
#define STUDIO_ENC_FORMAT_MJPEG 3 /* for yuv2lav  */

/* Shared functions for pipes.c */
void start_pipe_command(char *command[], int number);
void close_pipe(int number);
void write_pipe(int number, char *message);
int pipe_is_active(int number);
char *app_name(int number);
char *app_location(int number);
void init_pipes(void);

void save_app_locations(FILE *fp);
void load_app_locations(void);

#endif /* __STUDIO_PIPES_H__ */
