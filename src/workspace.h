#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

#include <gtk/gtk.h>

enum {
  WS_STATE_FOCUSED = 0x0001,
  WS_STATE_VISIBLE = 0x0002,
  WS_STATE_URGENT  = 0x0004,
  WS_STATE_INVALID = 0x0080,
  WS_STATE_ALL = 0x00ff,

  WS_CAP_ACTIVATE = 0x0100,
  WS_CAP_ALL = 0xff00,
};

typedef struct workspace_s {
  gpointer id;
  gchar *name;
  guint32 state;
  gint refcount;
} workspace_t;

struct workspace_api {
  void (*set_workspace) ( workspace_t *);
  guint (*get_geom) ( workspace_t *, GdkRectangle **, GdkRectangle *, gint *);
  gboolean (*get_can_create) (void);
};

#define PAGER_PIN_ID (GINT_TO_POINTER(-1))
#define WORKSPACE(x) ((workspace_t *)(x))

gboolean workspaces_supported( void );

workspace_t *workspace_new ( gpointer id );
void workspace_commit ( workspace_t *ws );
void workspace_set_focus ( gpointer id );
void workspace_set_state ( workspace_t *ws, guint32 state );
void workspace_set_caps ( workspace_t *ws, guint32 caps );
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
gboolean workspace_get_can_create ( void );
void workspace_pin_add ( gchar *pin );
GList *workspace_get_list ( void );
void workspace_ref ( gpointer id );
void workspace_unref ( gpointer id );

#endif
