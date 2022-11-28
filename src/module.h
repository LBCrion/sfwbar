
#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

typedef gpointer (*ModuleExpressionFunc)( GScanner *);

typedef struct {
  gchar *name;
  ModuleExpressionFunc function;
  gboolean numeric;
} ModuleExpressionHandler;

gboolean module_load ( gchar *name );
gboolean module_is_function ( GScanner *scanner );
gboolean module_is_numeric ( gchar *identifier );
gchar *module_get_string ( GScanner *scanner );
gdouble module_get_numeric ( GScanner *scanner );


#endif
