
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
  gint limit;
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
  gboolean ready;
  gboolean active;
  gchar *interface;
  gchar *provider;
  void (*activate) (void);
  void (*deactivate) (void);
  void (*finalize) (void);
} ModuleInterfaceV1;

typedef struct {
  GList *list;
  ModuleInterfaceV1 *active;
} ModuleInterfaceList;

enum ModuleFlags {
  MODULE_EXPR_NUMERIC       = 1,
  MODULE_EXPR_DETERMINISTIC = 2,
  MODULE_EXPR_RAW           = 4
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
void module_interface_select ( gchar *interface );
void module_interface_activate ( ModuleInterfaceV1 *iface );
void module_interface_deactivate ( ModuleInterfaceV1 *iface );
gchar *module_interface_provider_get ( gchar *interface );

#endif
