void reconstruct_lp_frame_420 (uint8_t * dest[3], uint8_t * src[3], int field);

void aa_interpolation_luma (uint8_t * frame, uint8_t * inframe, int field);
void sinc_interpolation_luma (uint8_t * frame, uint8_t * inframe, int field);
void interpolation_420JPEG_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field);
void interpolation_420MPEG2_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field);
void interpolation_420PALDV_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field);
void interpolation_411_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field);
void interpolation_422_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field);

void C444_to_C420 (uint8_t * , uint8_t * );

void subsample ( uint8_t * , uint8_t * );
void lowpass   ( uint8_t * , uint8_t * );
