#ifndef __CONFIG_FOR_STUDIO__
#define __CONFIG_FOR_STUDIO__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int verbose;

char video_format;
char input_source;
int hordcm;
int verdcm;
int geom_x;
int geom_y;
int geom_width;
int geom_height;
int quality;
int record_time;
int single_frame;
int time_lapse;
int wait_for_start;
int audio_size;
int audio_rate;
int stereo;
int audio_mute;
int audio_recsrc;
int sync_corr;
int MJPG_nbufs;
int MJPG_bufsize;
/* used for the encoding configuratione */
#define FILELEN 100
char enc_inputfile [FILELEN];
char enc_outputfile[FILELEN];
char enc_audiofile [FILELEN];
char enc_videofile [FILELEN];
int  use_yuvplay_pipe;                          /* Encoding Preview on/off */
int  encoding_syntax_style;	/* Used to set the syntax for the encoding */
int  fourpelmotion;             /* Common Quality setting */
int  twopelmotion;              /* Common Quality setting */
int  use_bicubic;               /* Use bicubic algorithmus for the scaling */

/* Structure that hat holds the encoding options */
#define SHORTOPT 2
#define LONGOPT 25
struct encodingoptions{ char notblacksize[LONGOPT];  /* lav2yuv options */
                        char input_use[LONGOPT];     /* yuvscaler options */
                        char output_size[LONGOPT];
                        char mode_keyword[LONGOPT];
                        char ininterlace_type[LONGOPT];
                        int  addoutputnorm;
                        int  outputformat;           /* lav2yuv options */
                        int  droplsb;
                        int  noisefilter;
                        int  audiobitrate;           /* audio options */
                        int  outputbitrate;
                        char forcestereo[SHORTOPT];
                        char forcemono[SHORTOPT];
                        char forcevcd[SHORTOPT];
                        int  bitrate;              /* mpeg2enc options */
                        int  qualityfactor;
                        int  minGop;
                        int  maxGop;
                        int  sequencesize;
                        int  nonvideorate;
                        int  searchradius;
                        int  muxformat;              /* mplex options */
                        char muxvbr[SHORTOPT];
                        int  streamdatarate;
			int  decoderbuffer;        /* also used for mpeg2enc */
                      };
/************************* END *********************/
struct encodingoptions encoding;  /* for mpeg1 */
struct encodingoptions encoding2; /* for mpeg2 */
struct encodingoptions encoding_vcd; /* for mpeg2 */
struct encodingoptions encoding_svcd; /* for mpeg2 */

char *record_dir;
int scene_detection_width_decimation;
int scene_detection_treshold;

int tv_width_process, tv_height_process, tv_height_capture, tv_width_capture;
int tv_height_edit, tv_width_edit, port;
char studioconfigfile[64];
char editlist_filename[256];

/* config.c */
void load_config(void);
void save_config(void);
void open_options_window(GtkWidget *widget, gpointer data);
int chk_dir(char *name);

/* config_encode.c */
void load_config_encode(void);
void save_config_encode(void);
void open_encoptions_window(GtkWidget *widget, gpointer data);

/* lavrec_pipe.c */
void table_set_text(int h, int m, int s, int f, int l, int ins, int del, int aerr);
GtkWidget *create_lavrec_layout(GtkWidget* window);
void emulate_ctrl_c(void);
void reset_lavrec_tv(void);
void reset_audio(void);
void scene_detection(void);

/* lavplay_pipe.c */
GtkWidget *create_lavplay_layout(void);
void quit_lavplay(void);
void reset_lavplay_tv(void);

/* lavedit.c */
GtkWidget *create_lavedit_layout(GtkWidget *window);
void reset_lavedit_tv(void);
void quit_lavplay_edit(void);
void set_background_color(GtkWidget *widget, int r, int g, int b);
int open_eli_file(char *filename);

/* lavencode.c */
GtkWidget *create_lavencode_layout(void);

/* lavencode_mpeg.c */
void open_mpeg_window(GtkWidget *widget, gpointer data);

/* studio.c */
void global_open_location(char *location);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CONFIG_FOR_STUDIO__ */
