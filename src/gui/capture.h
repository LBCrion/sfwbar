#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include <glib.h>
#include "wintree.h"

typedef enum {
  CAPTURE_TYPE_OUTPUT,
  CAPTURE_TYPE_WORKSPACE,
  CAPTURE_TYPE_WINDOW
} capture_type_t;

void capture_init ( void );
void capture_output ( gchar *name );
void capture_window ( window_t *win );
void capture_window_image_set ( GtkWidget *image, window_t *win,
    gboolean preview );
gboolean capture_support_check ( capture_type_t type );

#endif
