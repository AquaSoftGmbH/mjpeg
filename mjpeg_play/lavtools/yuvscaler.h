int average_coeff(unsigned int,unsigned int,unsigned int *);
void print_usage(char *argv[]);
void handle_args_global (int argc, char *argv[]);
void handle_args_dependent (int argc, char *argv[]);
int average(unsigned char *,unsigned char *, unsigned int *, unsigned int *,unsigned int);
int average_specific(unsigned char *,unsigned char *, unsigned int *, unsigned int *, unsigned int,unsigned int);
unsigned int pgcd(unsigned int,unsigned int);
int cubic_scale (uint8_t *, uint8_t *, unsigned int *,float *,unsigned int *, float *,unsigned long int *,
		 unsigned long int *, unsigned int);
long int cubic_spline(float);
int padding(uint8_t *,uint8_t *,unsigned int); 
int padding_interlaced (uint8_t *, uint8_t *, uint8_t *, unsigned int);
int cubic_scale_interlaced (uint8_t *, uint8_t *, uint8_t *, unsigned int * ,float *,unsigned int *, float *,
			    unsigned long int *,unsigned long int *, unsigned int);
