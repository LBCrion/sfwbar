
#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

#define MODULE_API_VERSION 3

typedef void (*ModuleInvalidator)( void );
typedef gboolean (*ModuleInitializer)( void );

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

gboolean module_load ( gchar *name );
void module_invalidate_all ( void );
void module_interface_select ( gchar *interface );
void module_interface_activate ( ModuleInterfaceV1 *iface );
void module_interface_deactivate ( ModuleInterfaceV1 *iface );
gchar *module_interface_provider_get ( gchar *interface );

#endif
