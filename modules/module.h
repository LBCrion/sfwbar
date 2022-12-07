#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

void sfwbar_module_init ( void );
gchar *sfwbar_module_poll ( gchar *param );
gchar *sfwbar_module_cmd ( gchar *param );
void sfwbar_module_function ( gchar *param, gchar *id );

#endif
