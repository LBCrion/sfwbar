
#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

#define MODULE_API_VERSION 3

typedef enum {
  MODULE_THREAD_MAIN,
  MODULE_THREAD_MODULE,
  MODULE_THREAD_OWN
} module_thread_t;

typedef void (*ModuleInvalidator)( void );

typedef struct {
  gboolean ready;
  gboolean active;
  gchar *interface;
  gchar *provider;
  GMainContext *context;
  void (*activate) (void);
  void (*deactivate) (void);
} ModuleInterfaceV1;

typedef struct {
  GMutex mutex;
  GCond cond;
  GMainContext *context;
  gboolean result, ready;
  module_thread_t thread;
  gboolean (*func)(void);
} module_init_closure_t;

gboolean module_load ( gchar *name );
void module_invalidate_all ( void );
void module_interface_activate ( ModuleInterfaceV1 *iface );
void module_interface_deactivate ( ModuleInterfaceV1 *iface );
gchar *module_interface_provider_get ( gchar *interface );
guint module_timeout_add ( guint i, GSourceFunc func, gpointer d );

#endif
