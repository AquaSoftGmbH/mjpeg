/* gap_enc_audio.h
 *
 *  GAP common encoder tool procedures
 *  with no dependencies to external libraries.
 *
 */

#ifndef GAP_ENC_LIB_H
#define GAP_ENC_LIB_H

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lib.h"
#include "gap_arr_dialog.h"

void        p_info_win(GRunModeType run_mode, char *msg, char *button_text);
gint32      p_get_filesize(char *fname);

char*       p_load_file(char *fname);



/* 0 = PAL, 1 = NTSC */
#define VIDEONORM_DEFAULT 0


#endif        /* GAP_ENC_LIB_H */
