#if !defined(__SFWBAR_H__)
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <json.h>
#include "wlr-foreign-toplevel-management-unstable-v1.h"

struct wt_window {
  GtkWidget *button;
  GtkWidget *switcher;
  gchar *title;
  gchar *appid;
  gint64 pid;
  gint64 wid;
  gpointer uid;
};

struct rect {
  guint x,y,w,h;
};

struct scan_var {
  gchar *name;
  GRegex *regex;
  gchar *json;
  gchar *str;
  double val;
  double pval;
  gint64 time;
  gint64 ptime;
  gint count;
  gint multi;
  guchar type;
  guchar status;
  struct scan_file *file;
};

struct scan_file {
  gchar *fname;
  gint flags;
  guchar source;
  time_t mtime;
  GList *vars;
};

struct layout_widget {
  GtkWidget *widget;
  GtkWidget *lobject;
  gchar *style;
  gchar *css;
  gchar *value;
  gchar *action;
  gchar *icon;
  gchar *eval;
  gint64 interval;
  gint64 next_poll;
  gboolean invalid;
  gboolean ready;
  gint wtype;
  gint dir;
  struct rect rect;
};

extern gchar *expr_token[];

extern struct wl_seat *seat;

void sway_ipc_init ( void );
gboolean sway_ipc_active ( void );
gchar *sway_ipc_poll ( gint sock, gint32 *etype );
int sway_ipc_open (int to);
int sway_ipc_send ( gint sock, gint32 type, gchar *command );
void sway_ipc_command ( gchar *cmd, ... );
int sway_ipc_subscribe ( gint sock );
gboolean sway_ipc_event ( GIOChannel *, GIOCondition , gpointer );
void place_window ( gint64 wid, gint64 pid );
void placer_config ( gint xs, gint ys, gint xo, gint yo, gboolean pid );

void taskbar_init ( GtkWidget * );
void taskbar_invalidate ( void );
void taskbar_set_visual ( gboolean nicons, gboolean nlabels );
void taskbar_update ( void );
void taskbar_window_init ( struct wt_window *win );
void taskbar_set_label ( struct wt_window *win, gchar *title );

struct wt_window *wintree_window_init ( void );
struct wt_window *wintree_from_id ( gpointer id );
struct wt_window *wintree_from_pid ( gint64 pid );
void wintree_window_append ( struct wt_window *win );
void wintree_window_delete ( gpointer id );
void wintree_set_focus ( gpointer id );
gboolean wintree_is_focused ( gpointer id );
GList *wintree_get_list ( void );

void wayland_init ( GtkWindow *, gboolean );

gboolean window_hide_event ( struct json_object *obj );
gboolean switcher_event ( struct json_object *obj );
void switcher_invalidate ( void );
void switcher_update ( void );
void switcher_set_label ( struct wt_window *win, gchar *title );
void switcher_window_init ( struct wt_window *win);
void switcher_config ( GtkWidget *, GtkWidget *, gint, gboolean, gboolean);

void pager_init ( GtkWidget * );
void pager_set_preview ( gboolean pv );
void pager_add_pin ( gchar *pin );
void pager_update ( void );

void sni_init ( GtkWidget *w );
void sni_update ( void );
struct layout_widget *config_parse ( gchar * );

struct layout_widget *layout_widget_new ( void );
gpointer layout_scanner_thread ( gpointer data );
void layout_widget_config ( struct layout_widget *lw );
void layout_widget_attach ( struct layout_widget *lw );
void layout_widget_free ( struct layout_widget *lw );
void layout_widgets_update ( GMainContext * );
void layout_widgets_draw ( void );
void widget_set_css ( GtkWidget * );

GtkWidget *flow_grid_new( gboolean limit );
void flow_grid_set_rows ( GtkWidget *cgrid, gint rows );
void flow_grid_set_cols ( GtkWidget *cgrid, gint cols );
void flow_grid_attach ( GtkWidget *cgrid, GtkWidget *w );
void flow_grid_pad ( GtkWidget *cgrid );
void flow_grid_clean ( GtkWidget *cgrid );

void scanner_var_attach ( struct scan_var *var );
void scanner_expire ( void );
int scanner_reset_vars ( GList *var_list );
int scanner_glob_file ( struct scan_file *file );

char *expr_parse ( gchar *expr_str, guint * );
gboolean parser_expect_symbol ( GScanner *, gchar , gchar *);
char *string_from_name ( gchar *name );
double numeric_from_name ( gchar *name );
void *list_by_name ( GList *prev, gchar *name );
char *parse_identifier ( gchar *id, gchar **fname );

gchar *gdk_monitor_get_xdg_name ( GdkMonitor *monitor );

gchar *get_xdg_config_file ( gchar *fname );
gchar *json_string_by_name ( struct json_object *obj, gchar *name );
gint64 json_int_by_name ( struct json_object *obj, gchar *name, gint64 defval);
gboolean json_bool_by_name ( struct json_object *obj, gchar *name, gboolean defval);
gdouble json_double_by_name ( struct json_object *obj, gchar *name, gdouble defval);
int md5_file( gchar *path, guchar output[16] );
void str_assign ( gchar **dest, gchar *source );
struct rect parse_rect ( struct json_object *obj );

void scale_image_set_image ( GtkWidget *widget, gchar *image );
GtkWidget *scale_image_new();
int scale_image_update ( GtkWidget *widget );
void scale_image_set_pixbuf ( GtkWidget *widget, GdkPixbuf * );

#define SCAN_VAR(x) ((struct scan_var *)x)
#define AS_WINDOW(x) ((struct wt_window *)(x))

enum {
  SV_ADD = 1,
  SV_PRODUCT = 2,
  SV_REPLACE = 3,
  SV_FIRST = 4
};

enum {
  SO_FILE = 0,
  SO_EXEC = 1
};

enum {
  VP_REGEX = 0,
  VP_JSON = 1,
  VP_GRAB = 2
};

enum {
  VF_CHTIME = 1,
  VF_NOGLOB = 2
};

enum {
  G_TOKEN_TIME    = G_TOKEN_LAST + 1,
  G_TOKEN_MIDW    = G_TOKEN_LAST + 2,
  G_TOKEN_EXTRACT = G_TOKEN_LAST + 3,
  G_TOKEN_DISK    = G_TOKEN_LAST + 4,
  G_TOKEN_VAL     = G_TOKEN_LAST + 5,
  G_TOKEN_STRW    = G_TOKEN_LAST + 6
};

#endif
