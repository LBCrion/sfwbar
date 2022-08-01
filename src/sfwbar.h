#ifndef __SFWBAR_H__
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <json.h>
#include "scanner.h"

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

struct rect {
  guint x,y,w,h;
};


void client_exec ( ScanFile *file );
void client_socket ( ScanFile *file );

void place_window ( gint64 wid, gint64 pid );
void placer_config ( gint xs, gint ys, gint xo, gint yo, gboolean pid );

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

char *expr_parse ( gchar *expr_str, guint * );
struct json_object *jpath_parse ( gchar *path, struct json_object *obj );

void widget_set_css ( GtkWidget *, gpointer );
void widget_parse_css ( GtkWidget *widget, gchar *css );


void mpd_ipc_init ( ScanFile *file );
void mpd_ipc_command ( gchar *command );

void list_remove_link ( GList **list, void *child );
gchar *get_xdg_config_file ( gchar *fname, gchar *extra );
const gchar *json_string_by_name ( struct json_object *obj, gchar *name );
gint64 json_int_by_name ( struct json_object *obj, gchar *name, gint64 defval);
gboolean json_bool_by_name ( struct json_object *obj, gchar *name, gboolean defval);
gdouble json_double_by_name ( struct json_object *obj, gchar *name, gdouble defval);
int md5_file( gchar *path, guchar output[16] );
struct rect parse_rect ( struct json_object *obj );
guint str_nhash ( gchar *str );
gboolean str_nequal ( gchar *str1, gchar *str2 );

enum {
  WS_FOCUSED =    1<<0,
  WS_MINIMIZED =  1<<1,
  WS_MAXIMIZED =  1<<2,
  WS_FULLSCREEN = 1<<3,
  WS_INHIBIT =    1<<4,
  WS_USERSTATE =  1<<5
};

#endif
