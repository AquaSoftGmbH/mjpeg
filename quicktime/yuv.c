#include "yuv.h"

int quicktime_init_yuv(quicktime_yuv_t *yuv_table)
{
	int i;
	for(i = 0; i < 256; i++)
	{
// compression
		yuv_table->rtoy_tab[i] = (long)( 0.2990 * 65536 * i);
		yuv_table->rtou_tab[i] = (long)(-0.1687 * 65536 * i);
		yuv_table->rtov_tab[i] = (long)( 0.5000 * 65536 * i);

		yuv_table->gtoy_tab[i] = (long)( 0.5870 * 65536 * i);
		yuv_table->gtou_tab[i] = (long)(-0.3320 * 65536 * i);
		yuv_table->gtov_tab[i] = (long)(-0.4187 * 65536 * i);

		yuv_table->btoy_tab[i] = (long)( 0.1140 * 65536 * i);
		yuv_table->btou_tab[i] = (long)( 0.5000 * 65536 * i);
		yuv_table->btov_tab[i] = (long)(-0.0813 * 65536 * i);
	}

	yuv_table->vtor = &(yuv_table->vtor_tab[128]);
	yuv_table->vtog = &(yuv_table->vtog_tab[128]);
	yuv_table->utog = &(yuv_table->utog_tab[128]);
	yuv_table->utob = &(yuv_table->utob_tab[128]);
	for(i = -128; i < 128; i++)
	{
// decompression
		yuv_table->vtor[i] = (long)( 1.4020 * 65536 * i);
		yuv_table->vtog[i] = (long)(-0.7141 * 65536 * i);

		yuv_table->utog[i] = (long)(-0.3441 * 65536 * i);
		yuv_table->utob[i] = (long)( 1.7720 * 65536 * i);
	}
}

int quicktime_delete_yuv(quicktime_yuv_t *yuv_table)
{
}
