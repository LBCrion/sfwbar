
#ifndef __MODULE_H__
#define __MODULE_H__

#include <glib.h>

#define MODULE_API_VERSION 3

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
