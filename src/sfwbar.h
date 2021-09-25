#if !defined(__SFWBAR_H__)
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <ucl.h>
#include "wlr-foreign-toplevel-management-unstable-v1.h"

struct context {
  gint32 features;
  int ipc;
  gint64 tb_focus;
  gint32 tb_rows;
  gint32 tb_cols;
  char sw_hstate;
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
  gint64 wt_counter;
  GList *wt_list;
  char wt_dirty;
  GList *widgets;
  GList *file_list;
  GList *scan_list;
  int default_dec;
  int line_num;
  int buff_len;
  char *read_buff;
  char *ret_val;
};

struct wt_window {
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *switcher;
  char *title;
  char *appid;
  gint64 pid;
  gint64 wid;
  struct zwlr_foreign_toplevel_handle_v1 *wlr;
};

struct rect {
  int x,y,w,h;
};

struct scan_var {
  GRegex *regex;
  gchar *json;
  char *name;
  gchar *str;
  double val;
  double pval;
  int multi;
  int count;
  unsigned char var_type;
  unsigned char status;
  gint64 time;
  gint64 ptime;
  struct scan_file *file;
  };

struct scan_file {
  char *fname;
  int flags;
  time_t mod_time;
  GList *vars;
  };

typedef struct zwlr_foreign_toplevel_handle_v1 wlr_fth;
extern struct wl_seat *seat;

int ipc_open(int);
int ipc_subscribe( int sock );
gchar *ipc_poll( int sock, gint32 *etype );
int ipc_send ( int sock, gint32 type, gchar *command );

void place_window ( gint64 wid, gint64 pid, struct context *context );
void placement_init ( struct context *context, const ucl_object_t *obj );

GtkWidget *taskbar_init ( struct context *context );
void taskbar_refresh ( struct context *context );
void taskbar_window_init ( struct context *context, struct wt_window *win );
struct wt_window *wintree_window_init ( void );
void wintree_window_append ( struct context *context, struct wt_window *win );
void wintree_populate ( struct context *context );
void wintree_event ( struct context *context );

void wlr_ft_init ( struct context *);

void switcher_event ( struct context *context, const ucl_object_t *obj );
void switcher_update ( struct context *context );
void switcher_window_init (struct context *context, struct wt_window *win);
void switcher_init ( struct context *context, const ucl_object_t *obj );

GtkWidget *pager_init ( struct context *context );
void pager_update ( struct context *context );

GtkWidget *layout_init ( struct context *context, const ucl_object_t *obj, GtkWidget *, GtkWidget * );
void widget_update_all( struct context *context );
void widget_action ( GtkWidget *widget, gpointer data );
GtkWidget *widget_icon_by_name ( gchar *name, int size );
void widget_set_css ( GtkWidget * );

GtkWidget *clamp_grid_new();
GtkWidget *alabel_new();
int scanner_expire ( GList *start );
int update_var_tree ( struct context *context );
int update_var_file ( struct context *context, FILE *in, GList *var_list );
int reset_var_list ( GList *var_list );
int update_var_files ( struct context *context, struct scan_file *file );
char *parse_expr ( struct context *context, char *expr_str );
char *string_from_name ( struct context *context, char *name );
double numeric_from_name ( struct context *context, char *name );
char *time_str ( char *tz );
char *df_str ( char *fpath );
char *extract_str ( char *str, char *pattern );
void *list_by_name ( GList *prev, char *name );
char *parse_identifier ( char *id, char **fname );
void scanner_init ( struct context *context, const ucl_object_t *obj );
GList *scanner_add_vars( struct context *context, const ucl_object_t *obj, struct scan_file *file );
char *numeric_to_str ( double num, int dec );
char *str_mid ( char *str, int c1, int c2 );

gchar *get_xdg_config_file ( gchar *fname );
gchar *ucl_string_by_name ( const ucl_object_t *obj, gchar *name);
gint64 ucl_int_by_name ( const ucl_object_t *obj, gchar *name, gint64 defval );
gboolean ucl_bool_by_name ( const ucl_object_t *obj, gchar *name, gboolean defval );
int md5_file( char *path, unsigned char output[16] );
void str_assign ( gchar **dest, gchar *source );
struct rect parse_rect ( const ucl_object_t *obj );
void scale_image_set_image ( GtkWidget *widget, gchar *image );
GtkWidget *scale_image_new();
int scale_image_update ( GtkWidget *widget );

#define SCAN_VAR(x) ((struct scan_var *)x)
#define SCAN_FILE(x) ((struct scan_file *)x)
#define AS_WINDOW(x) ((struct wt_window *)(x))
#define AS_RECT(x) ((struct rect *)(x))

enum {
        F_TASKBAR   = 1<<0,
        F_PLACEMENT = 1<<1,
        F_PAGER     = 1<<2,
        F_TB_ICON   = 1<<3,
        F_TB_LABEL  = 1<<4,
        F_TB_EXPAND = 1<<5,
        F_SWITCHER  = 1<<6,
        F_SW_ICON   = 1<<7,
        F_SW_LABEL  = 1<<8,
        F_PL_CHKPID = 1<<9,
        F_PA_RENDER = 1<<10
};

enum {
	SV_ADD = 1,
	SV_PRODUCT = 2,
	SV_REPLACE = 3,
	SV_FIRST = 4
	};

enum {
	UP_RESET = 1,
	UP_UPDATE = 2,
	UP_NOUPDATE = 3
	};

enum {
	VF_CHTIME = 1,
	VF_EXEC = 2,
	VF_NOGLOB = 4,
	VF_JSON = 8
	};

#endif
