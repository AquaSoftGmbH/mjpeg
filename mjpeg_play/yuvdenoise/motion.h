
void
subsample_frame (uint8_t * dst[3], uint8_t * src[3]);

/* search steps */
void 
mb_search_44 (uint16_t x, uint16_t y);

void 
mb_search_22 (uint16_t x, uint16_t y);

void 
mb_search_11 (uint16_t x, uint16_t y);

void 
mb_search_00 (uint16_t x, uint16_t y);

/* no accel */
uint32_t 
calc_SAD_noaccel (uint8_t * frm, uint8_t * ref);

uint32_t 
calc_SAD_uv_noaccel (uint8_t * frm, uint8_t * ref);

uint32_t
calc_SAD_half_noaccel (uint8_t * ref, uint8_t * frm1, uint8_t * frm2);

/* MMXE */
uint32_t
calc_SAD_mmxe (uint8_t * frm, uint8_t * ref);

uint32_t
calc_SAD_uv_mmxe (uint8_t * frm, uint8_t * ref);

uint32_t
calc_SAD_half_mmxe (uint8_t * ref, uint8_t * frm1, uint8_t * frm2);

/* MMX */
uint32_t
calc_SAD_mmx (uint8_t * frm, uint8_t * ref);

uint32_t
calc_SAD_uv_mmx (uint8_t * frm, uint8_t * ref);

uint32_t
calc_SAD_half_mmx (uint8_t * ref, uint8_t * frm1, uint8_t * frm2);

