/* functions to make life easier */

#ifndef __GDK_PIXBUF_WRITE_H__
#define __GDK_PIXBUF_WRITE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_PNG
gboolean gdk_pixbuf_save_png (GdkPixbuf *pixbuf, char *file);
#endif
#ifdef HAVE_JPEG
gboolean gdk_pixbuf_save_jpg (GdkPixbuf *pixbuf, char *file, int quality);
#endif
void gdk_pixbuf_save_to_file (GdkPixbuf *pixbuf);

#endif /* __GDK_PIXBUF_WRITE_H__ */
