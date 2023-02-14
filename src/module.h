
#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

typedef struct {
  GMainContext *gmc;
  gboolean (*emit_trigger)(gchar *);
  void (*config_string)(gchar *);
} ModuleApiV1;

typedef gpointer (*ModuleExpressionFunc)(gpointer *);
typedef void (*ModuleActionFunc)(gchar *, gchar *, void *, void *, void *,
    void *);
typedef void (*ModuleInvalidator)( void );
typedef void (*ModuleInitializer)( ModuleApiV1 * );
typedef gboolean (*ModuleEmitTrigger) ( gchar * );
typedef void (*ModuleInitTrigger) (GMainContext *, ModuleEmitTrigger );

typedef struct {
  gchar *name;
  ModuleExpressionFunc function;
  gchar *parameters;
  gboolean numeric;
} ModuleExpressionHandlerV1;

typedef struct {
  gchar *name;
  ModuleActionFunc function;
} ModuleActionHandlerV1;

#define MODULE_TRIGGER_EMIT(x) \
  if(sfwbar_module_api && sfwbar_module_api->emit_trigger) \
    g_main_context_invoke(sfwbar_module_api->gmc, \
        (GSourceFunc)sfwbar_module_api->emit_trigger,x);

#define MODULE_CONFIG_STRING(x) \
  if(sfwbar_module_api && sfwbar->module_api->config_string) \
    sfwbar_module_api->config_string(x)

gboolean module_load ( gchar *name );
void module_invalidate_all ( void );
gboolean module_is_function ( gchar *identifier );
gboolean module_is_numeric ( gchar *identifier );
gchar *module_get_string ( GScanner *scanner );
void module_action_exec ( gchar *name, gchar *param, gchar *addr, void *,
    void *, void *, void * );
void module_actions_add ( ModuleActionHandlerV1 **ahandler, gchar *name );
void module_expr_funcs_add ( ModuleExpressionHandlerV1 **ehandler,gchar *name);

#endif
