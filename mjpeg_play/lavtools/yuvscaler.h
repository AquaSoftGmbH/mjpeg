// yuvscaler.c
void yuvscaler_print_usage (char *argv[]);
void yuvscaler_print_information (y4m_stream_info_t in_streaminfo, y4m_ratio_t frame_rate);
uint8_t yuvscaler_nearest_integer_division (unsigned long int, unsigned long int);
//static y4m_ratio_t yuvscaler_calculate_output_sar (int out_w, int out_h,
//				int in_w, int in_h, y4m_ratio_t in_sar);
int yuvscaler_y4m_read_frame (int fd, y4m_frame_info_t * frameinfo,unsigned long int buflen, uint8_t * buf);
int blackout(uint8_t *input_y,uint8_t *input_u,uint8_t *input_v);
void handle_args_global (int argc, char *argv[]);
void handle_args_dependent (int argc, char *argv[]);
int main (int argc, char *argv[]);
unsigned int pgcd(unsigned int,unsigned int);

// yuvscaler_resample
int average_coeff(unsigned int,unsigned int,unsigned int *);
int average(unsigned char *,unsigned char *, unsigned int *, unsigned int *,unsigned int);
int average_specific(unsigned char *,unsigned char *, unsigned int *, unsigned int *, unsigned int);
// yuvscaler_bicubic
int cubic_scale (uint8_t *, uint8_t *, unsigned int *,float *,unsigned int *, float *,int16_t *,int16_t *, unsigned int);
int16_t cubic_spline(float,unsigned int);
int padding(uint8_t *,uint8_t *,unsigned int); 
int padding_interlaced (uint8_t *, uint8_t *, uint8_t *, unsigned int);
int cubic_scale_interlaced (uint8_t *, uint8_t *, uint8_t *, unsigned int * ,float *,unsigned int *,
			    float *, int16_t *, int16_t *, unsigned int);
