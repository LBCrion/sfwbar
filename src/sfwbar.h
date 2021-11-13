#if !defined(__SFWBAR_H__)
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <ucl.h>
#include "wlr-foreign-toplevel-management-unstable-v1.h"

struct context {
  gint32 features;
  gint ipc;
  gint64 tb_focus;
  gint32 tb_rows;
  gint32 tb_cols;
  gchar sw_hstate;
  gint32 sw_count;
  gint32 sw_max;
  gint32 sw_cols;
  GtkWidget *sw_win;
  GtkWidget *sw_box;
  gint32 pager_rows;
  gint32 pager_cols;
  GList *pager_pins;
  gint32 position;
  gint32 wp_x,wp_y;
  gint32 wo_x,wo_y;
  GtkWindow *window;
  GtkCssProvider *css;
  GtkWidget *box;
  GtkWidget *pager;
  GtkWidget *tray;
  GList *sni_items;
  GList *sni_ifaces;
  gint64 wt_counter;
  GList *wt_list;
  gchar wt_dirty;
  GList *widgets;
  GList *file_list;
  GList *scan_list;
  gint line_num;
  gint buff_len;
  gchar *read_buff;
  gchar *ret_val;
  GScanner *escan;
};

struct wt_window {
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *switcher;
  gchar *title;
  gchar *appid;
  gint64 pid;
  gint64 wid;
  struct zwlr_foreign_toplevel_handle_v1 *wlr;
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
  gchar *style;
  gchar *css;
  gchar *value;
  gchar *action;
  gchar *icon;
  gint64 interval;
  gint64 next_poll;
  gint wtype;
  gint dir;
  struct rect rect;
};

extern struct context *context;
extern gchar *expr_token[];

typedef struct zwlr_foreign_toplevel_handle_v1 wlr_fth;
extern struct wl_seat *seat;

void sway_ipc_init ( void );
gchar *sway_ipc_poll ( gint sock, gint32 *etype );
int sway_ipc_open (int to);
int sway_ipc_send ( gint sock, gint32 type, gchar *command );
int sway_ipc_subscribe ( gint sock );
gboolean sway_ipc_event ( GIOChannel *, GIOCondition , gpointer );
void place_window ( gint64 wid, gint64 pid );
void placement_init ( const ucl_object_t *obj );

GtkWidget *taskbar_init ( GtkWidget * );
void taskbar_refresh ( void );
void taskbar_window_init ( struct wt_window *win );
struct wt_window *wintree_window_init ( void );
void wintree_window_append ( struct wt_window *win );
gint wintree_compare ( struct wt_window *a, struct wt_window *b);

void wlr_ft_init ( void );

gboolean hide_event ( const ucl_object_t *obj );
gboolean switcher_event ( const ucl_object_t *obj );
void switcher_update ( void );
void switcher_window_init ( struct wt_window *win);
void switcher_init ( const ucl_object_t *obj );

GtkWidget *pager_init ( GtkWidget * );
void pager_update ( void );

GtkWidget *sni_init ( void );
void sni_refresh ( void );
GtkWidget *config_parse ( gchar * );

struct layout_widget *layout_widget_new ( void );
void layout_widget_config ( struct layout_widget *lw );
void layout_widget_free ( struct layout_widget *lw );
GtkWidget *layout_init (  const ucl_object_t *obj, GtkWidget *, GtkWidget * );
void widget_update_all( void );
void widget_action ( GtkWidget *widget, gchar *cmd );
GtkWidget *widget_icon_by_name ( gchar *name, gint size );
void widget_set_css ( GtkWidget * );

GtkWidget *clamp_grid_new();
GtkWidget *alabel_new();
void scanner_expire ( void );
int update_var_tree ( void );
int update_var_file ( FILE *in, GList *var_list );
int reset_var_list ( GList *var_list );
int scanner_glob_file ( struct scan_file *file );
char *expr_parse ( gchar *expr_str, guint * );
gboolean parser_expect_symbol ( GScanner *, gchar , gchar *);
char *string_from_name ( gchar *name );
double numeric_from_name ( gchar *name );
void *list_by_name ( GList *prev, gchar *name );
char *parse_identifier ( gchar *id, gchar **fname );
void scanner_init (  const ucl_object_t *obj );
GList *scanner_add_vars( const ucl_object_t *obj, struct scan_file *file );

gchar *get_xdg_config_file ( gchar *fname );
gchar *ucl_string_by_name ( const ucl_object_t *obj, gchar *name);
gint64 ucl_int_by_name ( const ucl_object_t *obj, gchar *name, gint64 defval );
gboolean ucl_bool_by_name ( const ucl_object_t *obj, gchar *name, gboolean defval );
int md5_file( gchar *path, guchar output[16] );
void str_assign ( gchar **dest, gchar *source );
struct rect parse_rect ( const ucl_object_t *obj );
void scale_image_set_image ( GtkWidget *widget, gchar *image );
GtkWidget *scale_image_new();
int scale_image_update ( GtkWidget *widget );
void scale_image_set_pixbuf ( GtkWidget *widget, GdkPixbuf * );

#define SCAN_VAR(x) ((struct scan_var *)x)
#define SCAN_FILE(x) ((struct scan_file *)x)
#define AS_WINDOW(x) ((struct wt_window *)(x))
#define AS_RECT(x) ((struct rect *)(x))

enum {
  F_TASKBAR   = 1<<0,
  F_PLACEMENT = 1<<1,
  F_PAGER     = 1<<2,
  F_TRAY      = 1<<3,
  F_TB_ICON   = 1<<4,
  F_TB_LABEL  = 1<<5,
  F_TB_EXPAND = 1<<6,
  F_SWITCHER  = 1<<7,
  F_SW_ICON   = 1<<8,
  F_SW_LABEL  = 1<<9,
  F_PL_CHKPID = 1<<10,
  F_PA_RENDER = 1<<11
};

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
  VF_NOGLOB = 2,
  VF_CONCUR = 16,
  VF_FINAL = 32
};

enum {
  G_TOKEN_TIME    = G_TOKEN_LAST + 1,
  G_TOKEN_MIDW    = G_TOKEN_LAST + 2,
  G_TOKEN_EXTRACT = G_TOKEN_LAST + 3,
  G_TOKEN_DF      = G_TOKEN_LAST + 4,
  G_TOKEN_VAL     = G_TOKEN_LAST + 5,
  G_TOKEN_STRW    = G_TOKEN_LAST + 6
};


#endif
