#ifndef QUICKTIME_YUV_H
#define QUICKTIME_YUV_H

typedef struct
{
	long rtoy_tab[256], gtoy_tab[256], btoy_tab[256];
	long rtou_tab[256], gtou_tab[256], btou_tab[256];
	long rtov_tab[256], gtov_tab[256], btov_tab[256];

	long vtor_tab[256], vtog_tab[256];
	long utog_tab[256], utob_tab[256];
	long *vtor, *vtog, *utog, *utob;
} quicktime_yuv_t;

#endif
