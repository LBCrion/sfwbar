#ifndef __FILTER_H__
#define __FILTER_H__

#include <gtk/gtk.h>
#include "wintree.h"

enum WindowFilter {
  FILTER_FLOATING = 1,
  FILTER_MINIMIZED = 2,
  FILTER_OUTPUT = 4,
  FILTER_WORKSPACE = 8,
};

GType filter_type_get ( void );
gboolean filter_window_check ( GtkWidget *parent, window_t *win );

#endif
