/* If changing MAX_EDIT_LIST_FILES, the macros below
   also have to be adapted. */

#define MAX_EDIT_LIST_FILES 256

#define N_EL_FRAME(x)  ( (x)&0xffffff )
#define N_EL_FILE(x)   ( ((x)>>24)&0xff )
#define EL_ENTRY(file,frame) ( ((file)<<24) | ((frame)&0xffffff) )

typedef struct
{
   long video_frames;
   long video_width;
   long video_height;
   long video_inter;
   long video_norm;

   long max_frame_size;
   int  MJPG_422;

   int  has_audio;
   long audio_rate;
   int  audio_chans;
   int  audio_bits;
   int  audio_bps;

   long num_video_files;
   char *(video_file_list[MAX_EDIT_LIST_FILES]);
   lav_file_t *(lav_fd[MAX_EDIT_LIST_FILES]);
   long num_frames[MAX_EDIT_LIST_FILES];
   long *frame_list;

   int  last_afile;
   long last_apos;
}
EditList;

int el_get_video_frame(char *vbuff, long nframe, EditList *el);  
void read_video_files(char **filename, int num_files, EditList *el); 
int el_get_video_frame(char *vbuff, long nframe, EditList *el);
int el_get_audio_data(char *abuff, long nframe, EditList *el, int mute);
void read_video_files(char **filename, int num_files, EditList *el);
int write_edit_list(char *name, long n1, long n2, EditList *el);
