
#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

typedef struct _module_queue {
  GList *list;
  GMutex mutex;
  void *(*duplicate) (void *);
  void (*free) (void *);
  void *(*get_str) (void *, gchar *);
  void *(*get_num) (void *, gchar *);
  gboolean (*compare) (const void *, const void *);
  const gchar *trigger;
} module_queue_t;

typedef gpointer (*ModuleExpressionFunc)(gpointer *, gpointer, gpointer);
typedef void (*ModuleActionFunc)(gchar *, gchar *, void *, void *, void *,
    void *);
typedef void (*ModuleInvalidator)( void );
typedef gboolean (*ModuleInitializer)( void );
typedef gboolean (*ModuleEmitTrigger) ( gchar * );
typedef void (*ModuleInitTrigger) (GMainContext *, ModuleEmitTrigger );

typedef struct {
  gchar *name;
  ModuleExpressionFunc function;
  gchar *parameters;
  gint flags;
} ModuleExpressionHandlerV1;

typedef struct {
  gchar *name;
  gint flags;
  ModuleActionFunc function;
} ModuleActionHandlerV1;

enum ModuleFlags {
  MODULE_EXPR_NUMERIC       = 1,
  MODULE_EXPR_DETERMINISTIC = 2
};

enum ModuleActionFlags {
  MODULE_ACT_WIDGET_ADDRESS = 1,
  MODULE_ACT_CMD_BY_DEF     = 2,
  MODULE_ACT_ADDRESS_ONLY   = 4
};

void module_queue_append ( module_queue_t *queue, void *item );
void module_queue_remove ( module_queue_t *queue );
void *module_queue_get_string ( module_queue_t *queue, gchar *param );
void *module_queue_get_numeric ( module_queue_t *queue, gchar *param );

gboolean module_load ( gchar *name );
void module_invalidate_all ( void );
gboolean module_is_function ( gchar *identifier );
gboolean module_check_flag ( gchar *identifier, gint flag );
gchar *module_get_string ( GScanner *scanner );
ModuleActionHandlerV1 *module_action_get ( GQuark quark );
void module_action_exec ( GQuark quark, gchar *param, gchar *addr, void *,
    void *, void *, void * );
void module_actions_add ( ModuleActionHandlerV1 **ahandler, gchar *name );
void module_expr_funcs_add ( ModuleExpressionHandlerV1 **ehandler,gchar *name);

#endif
