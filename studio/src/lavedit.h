#ifndef __LAVEDIT_FOR_STUDIO__
#define __LAVEDIT_FOR_STUDIO__

#include "gtkimageplug.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int number_of_files;
GtkWidget *image[MAX_NUM_SCENES];
int current_image;
char pal_or_ntsc;
int adjustment_b_changed;

/* lavedit_trimming.c */
void open_frame_edit_window(void);
void set_background_color(GtkWidget *widget, int r, int g, int b);
void create_lavplay_trimming_child(void);

/* lavedit.c */
void save_eli_temp_file(void);

/* lavedit_effects.c */
GtkWidget *get_effects_notebook_page(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LAVEDIT_FOR_STUDIO__ */
