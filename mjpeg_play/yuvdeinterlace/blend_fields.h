extern void		(*blend_fields)				(uint8_t * dst[3], uint8_t * src1[3], uint8_t * src2[3]);
void			blend_fields_non_accel		(uint8_t * dst[3], uint8_t * src1[3], uint8_t * src2[3]);
void			weave_fields				(uint8_t * dst[3], uint8_t * src1[3], uint8_t * src2[3]);
