#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

#include <gtk/gtk.h>

enum {
  WORKSPACE_FOCUSED = 0x0001,
  WORKSPACE_VISIBLE = 0x0002,
  WORKSPACE_URGENT =  0x0004,

  WORKSPACE_CAN_ACTIVATE = 0x0100,
};

#define WORKSPACE_STATE 0x00FF
#define WORKSPACE_CAPS 0xFF00

typedef struct workspace_s {
  gpointer id;
  gchar *name;
  guint32 state;
  gpointer custom_data;
  gint refcount;
} workspace_t;

struct workspace_api {
  void (*set_workspace) ( workspace_t *);
  guint (*get_geom) ( workspace_t *, GdkRectangle **, GdkRectangle *, gint *);
  void (*free_data) ( void *);
};

#define PAGER_PIN_ID (GINT_TO_POINTER(-1))

gboolean workspaces_supported( void );

void workspace_new ( workspace_t *new );
void workspace_set_focus ( gpointer id );
void workspace_set_name ( workspace_t *ws, const gchar *name );
void workspace_set_active ( workspace_t *ws, const gchar *output );
gpointer workspace_get_active ( GtkWidget *widget );
gboolean workspace_is_focused ( workspace_t *ws );
gpointer workspace_id_from_name ( const gchar *name );
workspace_t *workspace_from_id ( gpointer id );
gpointer workspace_get_focused ( void );
void workspace_api_register ( struct workspace_api *new );
void workspace_activate ( workspace_t *ws );
guint workspace_get_geometry ( workspace_t *, GdkRectangle **, GdkRectangle *,
    gint * );
void workspace_pin_add ( gchar *pin );
GList *workspace_get_list ( void );
void workspace_ref ( gpointer id );
void workspace_unref ( gpointer id );

#endif
