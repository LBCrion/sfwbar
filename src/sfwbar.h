#ifndef __SFWBAR_H__
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <json.h>

#define MAX_BUTTON 8

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

typedef struct scan_file {
  gchar *fname;
  gchar *trigger;
  gint flags;
  guchar source;
  time_t mtime;
  GList *vars;
  GSocketConnection *scon;
  GIOChannel *out;
} ScanFile;

typedef struct scan_var {
  GRegex *regex;
  gchar *json;
  gchar *str;
  double val;
  double pval;
  gint64 time;
  gint64 ptime;
  gint count;
  gint multi;
  guint type;
  guchar status;
  ScanFile *file;
} ScanVar;

typedef struct user_action {
  guchar cond;
  guchar ncond;
  guint type;
  gchar *command;
  gchar *addr;
} action_t;

#define MAX_STRING 9

typedef struct sni_host {
  gchar *iface;
  gchar *watcher;
  gchar *item_iface;
  GList *items;
} SniHost;

typedef struct sni_watcher {
  guint regid;
  gboolean watcher_registered;
  gchar *iface;
  GList *items;
  GDBusNodeInfo *idata;
  SniHost *host;
} SniWatcher;

typedef struct sni_item {
  gchar *uid;
  gchar *udest;
  gchar *dest;
  gchar *path;
  gchar *string[MAX_STRING];
  gchar *menu_path;
  GdkPixbuf *pixbuf[3];
  gboolean menu;
  gboolean dirty;
  gint ref;
  guint signal;
  GCancellable *cancel;
  GtkWidget *image;
  GtkWidget *box;
  SniHost *host;
} SniItem;

void action_exec ( GtkWidget *, action_t *, GdkEvent *, window_t *, guint16 *);
void action_free ( action_t *, GObject *);
void action_function_add ( gchar *, GList *);
void action_function_exec ( gchar *, GtkWidget *, GdkEvent *, window_t *,
    guint16 *);
void action_trigger_add ( action_t *action, gchar *trigger );
action_t *action_trigger_lookup ( gchar *trigger );

void client_exec ( ScanFile *file );
void client_socket ( ScanFile *file );

void sway_ipc_init ( void );
gboolean sway_ipc_active ( void );
gchar *sway_ipc_poll ( gint sock, gint32 *etype );
int sway_ipc_open (int to);
int sway_ipc_send ( gint sock, gint32 type, gchar *command );
void sway_ipc_command ( gchar *cmd, ... );
int sway_ipc_subscribe ( gint sock );
gboolean sway_ipc_event ( GIOChannel *, GIOCondition , gpointer );
void sway_ipc_bar_id ( gchar *id );
void sway_ipc_client_init ( ScanFile *file );
void sway_ipc_pager_populate ( void );

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

void wayland_init ( void );
void wayland_set_idle_inhibitor ( GtkWidget *widget, gboolean inhibit );
void wayland_reset_inhibitors ( GtkWidget *w, gpointer data );
void wayland_output_new ( GdkMonitor *gmon );
void wayland_output_destroy ( GdkMonitor *gmon );
void foreign_toplevel_activate ( gpointer tl );

char *expr_parse ( gchar *expr_str, guint * );
struct json_object *jpath_parse ( gchar *path, struct json_object *obj );

void pager_populate ( void );
void pager_set_preview ( gboolean pv );
void pager_set_numeric ( gboolean pn );
void pager_add_pin ( GtkWidget *pager, gchar *pin );
void pager_event ( struct json_object *obj );

void sni_init ( void );
void sni_update ( void );
GDBusConnection *sni_get_connection ( void );
void sni_item_set_icon ( SniItem *sni, gint icon, gint pix );
void sni_get_menu ( SniItem *sni, GdkEvent *event );
SniItem *sni_item_new (GDBusConnection *, SniHost *, const gchar *);
void sni_item_free ( SniItem *sni );

GtkWidget *menu_from_name ( gchar *name );
void menu_add ( gchar *name, GtkWidget *menu );
void menu_remove ( gchar *name );
void menu_popup ( GtkWidget *, GtkWidget *, GdkEvent *, gpointer, guint16 * );
gboolean menu_action_cb ( GtkWidget *widget, action_t *action );

void widget_set_css ( GtkWidget *, gpointer );
void widget_parse_css ( GtkWidget *widget, gchar *css );

void scanner_expire ( void );
int scanner_reset_vars ( GList * );
void scanner_update_json ( struct json_object *, ScanFile * );
int scanner_update_file ( GIOChannel *, ScanFile * );
int scanner_glob_file ( ScanFile * );
char *scanner_get_string ( gchar *, gboolean );
double scanner_get_numeric ( gchar *, gboolean );
void scanner_var_attach ( gchar *, ScanFile *, gchar *, guint, gint );
ScanFile *scanner_file_get ( gchar *trigger );
ScanFile *scanner_file_new ( gint , gchar *, gchar *, gint );

GtkWindow *bar_new ( gchar * );
void bar_set_monitor ( gchar *, gchar * );
void bar_set_layer ( gchar *, gchar *);
void bar_set_size ( gchar *, gchar * );
void bar_set_exclusive_zone ( gchar *, gchar * );
gchar *bar_get_output ( GtkWidget * );
gint bar_get_toplevel_dir ( GtkWidget * );
gboolean bar_hide_event ( const gchar *mode );
void bar_monitor_added_cb ( GdkDisplay *, GdkMonitor * );
void bar_monitor_removed_cb ( GdkDisplay *, GdkMonitor * );
void bar_update_monitor ( GtkWindow *win );
GtkWidget *bar_grid_by_name ( gchar *addr );

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

void scale_image_set_image ( GtkWidget *widget, gchar *image, gchar *extra );
GtkWidget *scale_image_new();
int scale_image_update ( GtkWidget *widget );
void scale_image_set_pixbuf ( GtkWidget *widget, GdkPixbuf * );

enum {
  WS_FOCUSED =    1<<0,
  WS_MINIMIZED =  1<<1,
  WS_MAXIMIZED =  1<<2,
  WS_FULLSCREEN = 1<<3,
  WS_INHIBIT =    1<<4,
  WS_USERSTATE =  1<<5
};

enum {
  SO_FILE = 0,
  SO_EXEC = 1,
  SO_CLIENT = 2
};

enum {
  VF_CHTIME = 1,
  VF_NOGLOB = 2
};

enum {
  SNI_PROP_CATEGORY = 0,
  SNI_PROP_ID = 1,
  SNI_PROP_TITLE = 2,
  SNI_PROP_STATUS = 3,
  SNI_PROP_ICON = 4,
  SNI_PROP_OVLAY = 5,
  SNI_PROP_ATTN = 6,
  SNI_PROP_ATTNMOV = 7,
  SNI_PROP_THEME = 8,
  SNI_PROP_ICONPIX = 9,
  SNI_PROP_OVLAYPIX = 10,
  SNI_PROP_ATTNPIX = 11,
  SNI_PROP_WINDOWID = 12,
  SNI_PROP_TOOLTIP = 13,
  SNI_PROP_ISMENU = 14,
  SNI_PROP_MENU = 15
};

#endif
