// Data type for yuvcorrect overall functionnality
typedef struct {
   uint8_t verbose;
   uint8_t mode;  // =0 for non-interactive mode, =1 for interactive corrections in full mode, =2 in half mode
   uint8_t stat;  // =1 if statistics informations are to be displayed
   int RefFrame;  // Only useful for yuvcorrect_tune
} overall_t;

// Data type for yuvcorrect frame characteristics
typedef struct {
   // General frame characteristics
//   y4m_ratio_t sample_aspect_ratio = Y4M_SAR_UNKNOWN; // see yuv4mpeg.h and yuv4mpeg_intern.h for possible values
//   int interlaced = -1; // =Y4M_ILACE_NONE for progressive/not-interlaced, Y4M_ILACE_TOP_FIRST for top interlaced, Y4M_ILACE_BOTTOM_FIRST for bottom interlaced
   unsigned long int length;
   y4m_frame_info_t info;
   // Y characteristics
   uint8_t *y;
   unsigned int y_width;
   unsigned int y_height;
   unsigned long int nb_y;
   // UV characteristics
   uint8_t *u, *v;
   unsigned int uv_width;
   unsigned int uv_height;
   unsigned long int nb_uv;
} frame_t;

// Data type for general frame correction 
typedef struct  {
   // HEADER management
   uint8_t no_header; // =0 by default, =1 to suppress header output
   y4m_stream_info_t streaminfo;
   // Frame corrections
   uint8_t line_switch;  // =0 by default, =1 if line switching is activated
   short int field_move; // =0 by default, =1 if bottom field is moved one frame forward, =-1 if it is the top field 
} general_correction_t;

// Data type for correction in the YUV space
typedef struct  {
   // LUMINANCE correction
   uint8_t luma;         // =1 if luminance correction is activated
   uint8_t *luminance;
   float Gamma;
   uint8_t InputYmin;
   uint8_t InputYmax;
   uint8_t OutputYmin;
   uint8_t OutputYmax;
   // CHROMINANCE correction
   uint8_t chroma;       // =1 if chrominance correction is activated
   uint8_t *chrominance;
   float UVrotation;
   float Ufactor;
   uint8_t Urotcenter;
   float Vfactor;
   uint8_t Vrotcenter;
   uint8_t UVmin;
   uint8_t UVmax;
} yuv_correction_t;
  
/*
 * Data type for correction in the RGB space
*/
//typedef struct RGB_correction_t
//{
//}
//rgb_correct;

// Functions Prototypes
void yuvcorrect_print_usage(void);
void handle_args_overall (int argc, char *argv[], overall_t *overall);
void handle_args (int argc, char *argv[], overall_t *overall, general_correction_t *gen_correct, yuv_correction_t *yuv_correct);
void yuvcorrect_print_information (general_correction_t *gen_correct, yuv_correction_t *yuv_correct);
int yuvcorrect_y4m_read_frame (int fd, frame_t *frame, general_correction_t *gen_correct);
int yuvcorrect_luminance_init (yuv_correction_t *yuv_correct);
int yuvcorrect_chrominance_init (yuv_correction_t *yuv_correct);
int yuvcorrect_luminance_treatment (frame_t *frame, yuv_correction_t *yuv_correct);
int yuvcorrect_chrominance_treatment (frame_t *frame, yuv_correction_t *yuv_correct);
int bottom_field_storage (frame_t * frame, uint8_t oddeven, uint8_t * field1,uint8_t * field2);
int top_field_storage (frame_t * frame, uint8_t oddeven, uint8_t * field1,uint8_t * field2);
int bottom_field_replace (frame_t * frame, uint8_t oddeven, uint8_t * field1,uint8_t * field2);
int top_field_replace (frame_t * frame, uint8_t oddeven, uint8_t * field1,uint8_t * field2);
int main (int argc, char *argv[]);
uint8_t clip_0_255 (int16_t number);
int yuvcorrect_RGB_init (int16_t * delta_red, int16_t * delta_green,
		    int16_t * delta_blue, uint8_t InRmin, uint8_t InRmax,
		    uint8_t OutRmin, uint8_t OutRmax, uint8_t InGmin,
		    uint8_t InGmax, uint8_t OutGmin, uint8_t OutGmax,
		    uint8_t InBmin, uint8_t InBmax, uint8_t OutBmin,
		    uint8_t OutBmax);
void yuvstat (frame_t *frame);
int yuvcorrect_RGB_treatment (uint8_t * input, unsigned long size,
			 unsigned long input_height,
			 unsigned long input_width, uint8_t * delta_red,
			 uint8_t * delta_green, uint8_t * delta_blue);
uint8_t yuvcorrect_nearest_integer_division (unsigned long int p, unsigned long int q);

