#ifndef __LAVEDIT_FOR_STUDIO__
#define __LAVEDIT_FOR_STUDIO__

#include "gtkscenelist.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkWidget *scenelist;
int adjustment_b_changed;

/* lavedit_trimming.c */
void open_frame_edit_window(void);
void create_lavplay_trimming_child(void);

/* lavedit.c */
void save_eli_temp_file(void);
void create_filesel3(GtkWidget *widget, char *what_to_do);

/* lavedit_effects.c */
GtkWidget *get_effects_notebook_page(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LAVEDIT_FOR_STUDIO__ */
