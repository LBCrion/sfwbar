#ifndef __WINTREE_H__
#define __WINTREE_H__

typedef struct wt_window {
  GtkWidget *switcher;
  gchar *title;
  gchar *appid;
  gchar *output;
  gint64 pid;
  gpointer uid;
  guint16 state;
  gboolean valid;
} window_t;

window_t *wintree_window_init ( void );
window_t *wintree_from_id ( gpointer id );
window_t *wintree_from_pid ( gint64 pid );
void wintree_window_append ( window_t *win );
void wintree_window_delete ( gpointer id );
void wintree_commit ( window_t *win );
void wintree_set_focus ( gpointer id );
void wintree_set_active ( gchar *title );
void wintree_set_title ( gpointer wid, const gchar *title );
void wintree_set_app_id ( gpointer wid, const gchar *app_id);
void wintree_focus ( gpointer id );
void wintree_close ( gpointer id );
void wintree_minimize ( gpointer id );
void wintree_maximize ( gpointer id );
void wintree_unminimize ( gpointer id );
void wintree_unmaximize ( gpointer id );
gpointer wintree_get_focus ( void );
gchar *wintree_get_active ( void );
gboolean wintree_is_focused ( gpointer id );
GList *wintree_get_list ( void );
gint wintree_compare ( window_t *a, window_t *b);
void wintree_appid_map_add ( gchar *pattern, gchar *appid );
gchar *wintree_appid_map_lookup ( gchar *title );

enum {
  WS_FOCUSED =    1<<0,
  WS_MINIMIZED =  1<<1,
  WS_MAXIMIZED =  1<<2,
  WS_FULLSCREEN = 1<<3,
  WS_INHIBIT =    1<<4,
  WS_USERSTATE =  1<<5
};

#endif
