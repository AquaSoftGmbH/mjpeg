int average_coeff(unsigned int,unsigned int,unsigned int *);
void print_usage(char *argv[]);
void handle_args_global (int argc, char *argv[]);
void handle_args_dependent (int argc, char *argv[]);
int average(unsigned char *,unsigned char *, unsigned int *, unsigned int *,unsigned int);
int average_specific(unsigned char *,unsigned char *, unsigned int *, unsigned int *, unsigned int);
unsigned int pgcd(unsigned int,unsigned int);
int cubic_scale (uint8_t *, uint8_t *, unsigned int *,float *,unsigned int *, float *,int16_t *,int16_t *, unsigned int);
int16_t cubic_spline(float);
int padding(uint8_t *,uint8_t *,unsigned int); 
int padding_interlaced (uint8_t *, uint8_t *, uint8_t *, unsigned int);
int cubic_scale_interlaced (uint8_t *, uint8_t *, uint8_t *, unsigned int * ,float *,unsigned int *,
			    float *, int16_t *, int16_t *, unsigned int);
int  my_y4m_read_frame(int, y4m_frame_info_t *, unsigned long int, char *, int);
int line_switch (uint8_t * input, uint8_t *line);
