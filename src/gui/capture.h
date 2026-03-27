#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include <glib.h>

typedef enum {
  CAPTURE_TYPE_OUTPUT,
  CAPTURE_TYPE_WORKSPACE,
  CAPTURE_TYPE_WINDOW
} capture_type_t;

void capture_init ( void );
void capture_output ( gchar *name );
void capture_window ( gpointer wid );
gboolean capture_support_check ( capture_type_t type );

#endif
