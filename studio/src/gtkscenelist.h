/* Scene List widget for Linux Video Studio
 * Copyright (C) 2001 - Ronald Bultje
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, 
 * USA.
 */

#ifndef __GTK_SCENELIST_H__
#define __GTK_SCENELIST_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_SCENELIST(obj)          GTK_CHECK_CAST (obj, gtk_scenelist_get_type (), GtkSceneList)
#define GTK_SCENELIST_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_scenelist_get_type (), GtkSceneListClass)
#define GTK_IS_SCENELIST(obj)       GTK_CHECK_TYPE (obj, gtk_scenelist_get_type ())


typedef enum {
	GTK_TRANSITION_BLEND,
	GTK_TRANSITION_WIPE_LEFT_TO_RIGHT,
	GTK_TRANSITION_WIPE_RIGHT_TO_LEFT,
	GTK_TRANSITION_WIPE_TOP_TO_BOTTOM,
	GTK_TRANSITION_WIPE_BOTTOM_TO_TOP,
	GTK_TRANSITION_OVERLAY_ENLARGE,
	GTK_TRANSITION_OVERLAY_ENSMALL
} GtkTransitionType;

typedef enum {
	GTK_EFFECT_IMAGE,
	GTK_EFFECT_TEXT
} GtkEffectType;

typedef struct _GtkScene			GtkScene;
typedef struct _GtkTransition			GtkTransition;
typedef struct _GtkEffect			GtkEffect;
typedef struct _GtkSceneList			GtkSceneList;
typedef struct _GtkSceneListClass		GtkSceneListClass;

struct _GtkTransition
{
	GtkTransitionType type;		/* type of transition (blend, wipe_??? or overlay_???) */
	gint length;			/* length (in frames) of the transition */

	/* some blend-transition-only-options */
	gint opacity_start;		/* starting opacity of second stream during transition */
	gint opacity_end;		/* end opacity of second stream during transition */

	/* some wipe-transition-only-options */
	gint num_rows;			/* number of rows/columns */
	gint reposition_1;		/* whether to reposition the first stream */
	gint reposition_2;		/* whether to reposition the second stream */

	/* some overlay-transition-only-options */
	gint poo_x;			/* X-point of origin */
	gint poo_y;			/* Y-point of origin */
	gint scaling;			/* whether to scale the stream */
};

struct _GtkEffect
{
	GtkEffectType type;		/* type of effect (image or text overlay) */

	gchar name[256];		/* image: the image-filename, text: the actual text */
	gint length;			/* duration of the effect (in frames) */
	gint offset;			/* frame offset of the effect (from first frame of the scene) */
	gint opacity;			/* opacity (0-100%) of the overlayed text/image */

	gint x;				/* x- and y-offset from the topleft of the video */
	gint y;
	gint w;				/* (can be scaled) width/height of the (text) image */
	gint h;

	/* some text-overlay-only-options */
	gchar *font;			/* font of the text */
	gdouble colors[3];		/* color of the text */
};

struct _GtkScene
{
	gint view_start;		/* view_start and view_end are the part of the scene which we */
	gint view_end;			/* actually want to see - this is changed by "trimming" */

	gint scene_start;		/* scene_start and scene_end are the beginning and end frame of */
	gint scene_end;			/* the scene in movie "movie_num" */

	gint start_total;		/* The number of the first frame (view_start) in the editlist */

	gint movie_num;			/* movie-number in the array in GtkSceneList */

	GdkPixbuf *image;		/* the first frame of the viewable part (so 'view_start') */

	GtkEffect *effect;		/* possible overlay of image or text */
};

struct _GtkSceneList
{
	GtkWidget widget;

	GdkPixbuf *background;		/* the background image to be used for each scene */

	GList *items;			/* GList which contains all the scenes, transitions and other info */

	gint num_frames;		/* Total number of frames in the current open editlist */
	GList *scene;			/* The scenes, as their number of occurrence in the GList *items; */
	GList *transition;		/* The transitions, as their number of occurrence in the GList *items; */
	GList *movie;			/* the movies */
	gchar norm;			/* 'p' or 'n', norm of the editlist */

	gint current_scene;		/* the first of the series of scenes currently painted */
	GtkAdjustment *adj;		/* the adjustment of the series of scenes and which are shown */
	gint selected_scene;		/* the scene which is currently selected */
};

struct _GtkSceneListClass
{
	GtkWidgetClass parent_class;

	void (*scene_selected)		(GtkWidget *widget,
					 gint scene_num);
	void (*scenelist_changed)	(GtkWidget *widget);
};

/* scenelist widget object */
guint gtk_scenelist_get_type (void);
GtkWidget *gtk_scenelist_new(gchar *editlist);

/* open, save, new editlist handling */
void gtk_scenelist_new_editlist(GtkSceneList *scenelist);
gint gtk_scenelist_open_editlist(GtkSceneList *scenelist, gchar *editlist);
gint gtk_scenelist_write_editlist(GtkSceneList *scenelist, gchar *filename);
void gtk_scenelist_draw(GtkWidget *widget);

/* positioning */
void gtk_scenelist_set_adjustment(GtkSceneList *scenelist, GtkAdjustment *adj);
void gtk_scenelist_view(GtkSceneList *scenelist, gint num);
void gtk_scenelist_select(GtkSceneList *scenelist, gint scene);
void gtk_scenelist_get_num_drawn(GtkSceneList *scenelist, gint *num, gint *left_border);

/* editing */
void gtk_scenelist_edit_move(GtkSceneList *scenelist, gint scene, gint direction);
void gtk_scenelist_edit_add(GtkSceneList *scenelist, char *movie, gint view_start,
	gint view_end, gint scene_start, gint scene_end, gint scene_num);
void gtk_scenelist_edit_delete(GtkSceneList *scenelist, gint scene);
void gtk_scenelist_edit_split(GtkSceneList *scenelist, gint scene, gint framenum);

/* get scenes, transitions and such */
GtkScene *gtk_scenelist_get_scene(GtkSceneList *scenelist, gint scene);
char *gtk_scenelist_get_movie(GtkSceneList *scenelist, gint scene);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SCENELIST_H__ */
