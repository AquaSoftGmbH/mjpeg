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
int file_flush;
int max_file_size;
int software_encoding;
int software_recwidth;
int software_recheight;
int use_read;
int audio_size;
int audio_rate;
int stereo;
int audio_mute;
int audio_recsrc;
int sync_corr;
int MJPG_nbufs;
int MJPG_bufsize;
  char picture_aspect[8];

/* used for the encoding configuration */
#define FILELEN 100
char enc_inputfile [FILELEN];
char enc_outputfile[FILELEN];
char enc_audiofile [FILELEN];
char enc_videofile [FILELEN];
char selected_player [FILELEN];       /* the player for the video playback */
int  use_yuvplay_pipe;                          /* Encoding Preview on/off */
int  fourpelmotion;             /* Common Quality setting */
int  twopelmotion;              /* Common Quality setting */
int  use_bicubic;               /* Use bicubic algorithmus for the scaling */
int  saveonexit;                /* save the encoding options when exiting */

/* Structure that hat holds the encoding options */
#define SHORTOPT 2
#define LONGOPT 25
struct encodingoptions{
        char notblacksize[LONGOPT];           /**< yuvscaler: -I ACTIVE_size */
        char input_use[LONGOPT];                 /**< yuvscaler: -I USE_size */
        char output_size[LONGOPT];  /**< yuvscaler: -O SIZE_ or VCD/SVCD/DVD */
        char mode_keyword[LONGOPT];               /**< yuvscaler: -M various */
        char interlacecorr[LONGOPT];    /**< yuvscaler: interlacing optionis */
        int  addoutputnorm;         /**< yuvscaler adds the output norm here */
        int  use_yuvdenoise;                /**< filter: if we should use it */
        int  deinterlace;                  /**< filter: if we deinterlace -F */
        int  sharpness;             /**< filter: set sharpenss in percent -S */
        int  denois_thhold;       /**< filter: set the Denoiser threshold -t */
        int  average_frames;              /**< filter: Average 'n' frames -l */
        int  audiobitrate;                               /**< audio: bitrate */
        int  outputbitrate;           /**< audio: sample rate 32 44.1 48 kHz */
        char forcestereo[SHORTOPT];          /**< audio: force stereo output */
        char forcemono[SHORTOPT];              /**< audio: force mono output */
        char forcevcd[SHORTOPT];     /**< audio: force vcd compatible output */
        int  bitrate;                           /**< mpeg2enc: video bitrate */
        int  qualityfactor;                 /**< mpeg2enc: quality factor -q */
        int  minGop;                      /**< mpeg2enc: minimum GOP size -g */
        int  maxGop;                      /**< mpeg2enc: maximum GOX size -G */
        int  sequencesize;         /**< mpeg2enc,mplex: Sequence Lenght -S,  */
        int  nonvideorate;        /**< mpeg2enc,mplex: Nonvideo bitrate -B,  */
        int  searchradius;                   /**< mpeg2enc: Search Radius -r */
        int  muxformat;                          /**< mplex: multiple format */
        char muxvbr[SHORTOPT];        /**< mplex: If we have a VBR Stream -V */
        int  streamdatarate;          /**< mplex: VBR Stream max Datarate -r */
        int  decoderbuffer;    /**< mpeg2enc,mplex decoder Buffer size -V,-B */
        char codec[LONGOPT];                 /**< yuv2divx: codec to be used */
             /* for the other options needed for divx, other fields are used */
                      };
/************************* END *********************/
struct encodingoptions encoding;         /* for mpeg1 */
struct encodingoptions encoding2;        /* for mpeg2 */
struct encodingoptions encoding_gmpeg;   /* for general mpeg */
struct encodingoptions encoding_vcd;     /* for vcd */
struct encodingoptions encoding_svcd;    /* for svcd */
struct encodingoptions encoding_dvd;     /* for dvd */
struct encodingoptions encoding_yuv2lav; /* for yuv2lav */

/* Struct that holds the machine that thas to do the task */
struct machine {  int lav2wav;
                  int mp2enc;
                  int lav2yuv;
                  int yuvdenoise;
                  int yuvscaler;
                  int mpeg2enc;
                  int yuv2lav;
               };
/************************* END *********************/
struct machine machine4mpeg1;
struct machine machine4mpeg2;
struct machine machine4generic;
struct machine machine4vcd;
struct machine machine4svcd;
struct machine machine4dvd;
struct machine machine4yuv2lav;

GList *machine_names;
int enhanced_settings; /*use a different set of machines for every task (rsh)*/

/* Struct that holds the data of the script we want to create */
struct f_script {
                int mpeg1;    /* Bit 0 (1) = Audio */
                int mpeg2;    /* Bit 1 (2) = Video */
                int generic;  /* Bit 2 (4) = Mplex */
                int vcd;      /* Bit 3 (8) = Full  */  
                int svcd;
                int dvd;
                int yuv2lav;
              };
/**************************************************************/
struct f_script script;
char script_name [FILELEN]; /* used for the script name in script generation */
int script_use_distributed; /*used for check of usage of distributed encoding*/ 

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
char videodev[LONGOPT];   /** holds the video dev for LAV_VIDEO_DEV */
char audiodev[LONGOPT];   /** holds the audio dev for LAV_AUDIO_DEV */
char mixerdev[LONGOPT];   /** holds the mixer dev for LAV_MIXER_DEV */

/* config_encode.c */
void load_config_encode(void);
void save_config_encode(void);
void create_encoding_layout(GtkWidget *table);
void accept_encoptions(GtkWidget *widget, gpointer data);

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
GtkWidget *combo_entry_active;
GtkWidget *combo_entry_scalerinput;

/* lavencode_distributed.c */
void open_distributed_window(GtkWidget *widget, gpointer data);

/* lavencode_script.c */
void open_scriptgen_window(GtkWidget *widget, gpointer data);
void command_2string(char **command, char *string);
void create_command_mp2enc(char *mp2enc_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void create_command_lav2wav(char *lav2wav_command[256], int use_rsh,
           struct encodingoptions *option, struct machine *machine4);
void create_command_lav2yuv(char *lav2yuv_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4);
void create_command_yuvscaler(char *lav2yuv_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4);
void create_command_yuvdenoise(char *yuvdenoise_command[256], int use_rsh,
          struct encodingoptions *option, struct machine *machine4);
void create_command_mpeg2enc(char* mpeg2enc_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void create_command_yuv2lav(char* yuv2divx_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);
void create_command_mplex(char* mplex_command[256], int use_rsh,
  struct encodingoptions *option, struct machine *machine4, char ext[LONGOPT]);

/* lavencode_util.c */
void create_window_select(char filename[FILELEN]);
char area_size[LONGOPT];

/* studio.c */
void global_open_location(char *location);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CONFIG_FOR_STUDIO__ */
