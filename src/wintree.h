#ifndef __WINTREE_H__
#define __WINTREE_H__

#include <gtk/gtk.h>
#include "workspace.h"

typedef struct wt_window {
  gchar *title;
  gchar *appid;
  GList *outputs;
  gpointer *workspace;
  gint64 pid;
  gpointer uid;
  guint16 state;
  gboolean floating;
  gboolean valid;
} window_t;

struct wintree_api {
  void (*minimize) ( void *);
  void (*unminimize) ( void *);
  void (*maximize) ( void *);
  void (*unmaximize) ( void *);
  void (*focus) ( void *);
  void (*close) ( void *);
  void (*move_to) (  void *, void * );
};

void wintree_api_register ( struct wintree_api *new );
window_t *wintree_window_init ( void );
window_t *wintree_from_id ( gpointer id );
window_t *wintree_from_pid ( gint64 pid );
void wintree_window_append ( window_t *win );
void wintree_window_delete ( gpointer id );
void wintree_commit ( window_t *win );
void wintree_log ( gpointer id );
void wintree_set_focus ( gpointer id );
void wintree_set_active ( gchar *title );
void wintree_set_title ( gpointer wid, const gchar *title );
void wintree_set_app_id ( gpointer wid, const gchar *app_id);
void wintree_set_workspace ( gpointer wid, gpointer wsid );
void wintree_set_float ( gpointer wid, gboolean floating );
void wintree_focus ( gpointer id );
void wintree_close ( gpointer id );
void wintree_minimize ( gpointer id );
void wintree_maximize ( gpointer id );
void wintree_unminimize ( gpointer id );
void wintree_unmaximize ( gpointer id );
void wintree_move_to ( gpointer id, gpointer wsid );
gpointer wintree_get_focus ( void );
gchar *wintree_get_active ( void );
gboolean wintree_is_focused ( gpointer id );
GList *wintree_get_list ( void );
gint wintree_compare ( window_t *a, window_t *b);
void wintree_appid_map_add ( gchar *pattern, gchar *appid );
gchar *wintree_appid_map_lookup ( gchar *title );
void wintree_filter_appid ( gchar *pattern );
void wintree_filter_title ( gchar *pattern );
gboolean wintree_is_filtered ( window_t *win );
void wintree_placer_conf( gint xs, gint ys, gint xo, gint yo, gboolean pid );
gboolean wintree_placer_state ( void );
gboolean wintree_placer_check ( gint pid );
void wintree_placer_calc ( gint nobs, GdkRectangle *obs, GdkRectangle output,
    GdkRectangle *win );
gboolean wintree_get_disown ( void );
void wintree_set_disown ( gboolean new );

enum {
  WS_FOCUSED =    1<<0,
  WS_MINIMIZED =  1<<1,
  WS_MAXIMIZED =  1<<2,
  WS_FULLSCREEN = 1<<3,
  WS_URGENT =     1<<4,
  WS_USERSTATE =  1<<5,
  WS_USERSTATE2 = 1<<6,
  WS_CHILDREN =   1<<7
};

#endif
