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
  void *data;
} workspace_t;

struct workspace_api {
  void (*set_workspace) ( workspace_t * );
  guint (*get_geom) ( gpointer, GdkRectangle *, gpointer, GdkRectangle **,
      GdkRectangle *, gint * );
  gboolean (*get_can_create) ( void );
  gboolean (*check_monitor) ( gpointer, gchar * );
};

typedef struct _workspace_listener {
  void (*workspace_new) ( workspace_t *, void *);
  void (*workspace_invalidate) ( workspace_t *, void *);
  void (*workspace_destroy) ( workspace_t *, void *);
  void *data;
} workspace_listener_t;

#define WORKSPACE_LISTENER(x) ((workspace_listener_t *)(x))
#define PAGER_PIN_ID (GINT_TO_POINTER(-1))
#define WORKSPACE(x) ((workspace_t *)(x))

void workspace_api_register ( struct workspace_api *new );
gboolean workspace_api_check ( void );
void workspace_listener_register ( workspace_listener_t *, gpointer );
void workspace_listener_remove ( void *data );
workspace_t *workspace_new ( gpointer id );
void workspace_commit ( workspace_t *ws );
void workspace_set_focus ( gpointer id );
void workspace_set_state ( workspace_t *ws, guint32 state );
void workspace_set_caps ( workspace_t *ws, guint32 caps );
void workspace_set_name ( workspace_t *ws, const gchar *name );
void workspace_set_active ( workspace_t *ws, const gchar *output );
gpointer workspace_get_active ( GtkWidget *widget );
gpointer workspace_id_from_name ( const gchar *name );
workspace_t *workspace_from_id ( gpointer id );
gpointer workspace_get_focused ( void );
void workspace_activate ( workspace_t *ws );
guint workspace_get_geometry ( gpointer wid, GdkRectangle *wloc, gpointer wsid,
    GdkRectangle **wins, GdkRectangle *spc, gint *focus);
gboolean workspace_check_monitor ( gpointer wsid, gchar *output );
gboolean workspace_get_can_create ( void );
void workspace_pin_add ( gchar *pin );
GList *workspace_get_list ( void );
workspace_t *workspace_from_name ( const gchar *name );
void workspace_ref ( gpointer id );
void workspace_unref ( gpointer id );

#endif
