
#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

typedef gpointer (*ModuleExpressionFunc)( gpointer *);
typedef void (*ModuleInvalidator)( void );
typedef gboolean (*ModuleEmitTrigger) ( gchar * );
typedef void (*ModuleInitTrigger) (GMainContext *, ModuleEmitTrigger );

typedef struct {
  gchar *name;
  ModuleExpressionFunc function;
  gchar *parameters;
  gboolean numeric;
} ModuleExpressionHandler;

#define MODULE_TRIGGER_DECL \
GMainContext *sfwbar_gmc; \
GSourceFunc sfwbar_emit_func; \
void sfwbar_trigger_init ( GMainContext *context, GSourceFunc emit ) \
{ \
  sfwbar_gmc = context; \
  sfwbar_emit_func = emit; \
} \

#define MODULE_EMIT_TRIGGER(x) \
  g_main_context_invoke(sfwbar_gmc,sfwbar_emit_func,x);

gboolean module_load ( gchar *name );
void module_invalidate_all ( void );
gboolean module_is_function ( GScanner *scanner );
gboolean module_is_numeric ( gchar *identifier );
gchar *module_get_string ( GScanner *scanner );
gdouble module_get_numeric ( GScanner *scanner );

#endif
