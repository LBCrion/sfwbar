#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

#include <gtk/gtk.h>

typedef struct workspace_s {
  gpointer id;
  gchar *name;
  gboolean visible;
  gboolean focused;
} workspace_t;

struct pager_api {
  void (*set_workspace) ( workspace_t *);
  guint (*get_geom) ( workspace_t *, GdkRectangle **, GdkRectangle *, gint *);
};


void workspace_new ( workspace_t *new );
void workspace_delete ( gpointer id );
void workspace_set_focus ( gpointer id );
void workspace_set_active ( workspace_t *ws, const gchar *output );
gpointer workspace_get_active ( GtkWidget *widget );
gboolean workspace_is_focused ( workspace_t *ws );
gpointer workspace_id_from_name ( const gchar *name );
workspace_t *workspace_from_id ( gpointer id );
void workspace_api_register ( struct pager_api *new );
void workspace_activate ( workspace_t *ws );
guint workspace_get_geometry ( workspace_t *, GdkRectangle **, GdkRectangle *,
    gint * );
void workspace_pin_add ( gchar *pin );
gboolean workspace_pin_check ( gchar *pin );
void workspace_populate_pins ( void );
GList *workspace_get_list ( void );

#endif
