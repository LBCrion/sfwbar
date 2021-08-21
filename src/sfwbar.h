#if !defined(__SFWBAR_H__)
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <ucl.h>

struct ipc_event {
  gint8 event;
  gint64 pid;
  gint64 wid;
  gchar *title;
  gchar *appid;
};

struct context {
  gint32 features;
  int ipc;
  gint32 tb_focus;
  gint32 tb_rows;
  gint32 tb_cols;
  gint32 tb_isize;
  char sw_hstate;
  gint32 sw_count;
  gint32 sw_max;
  gint32 sw_cols;
  gint32 sw_isize;
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
  GList *wt_list;
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
};

struct rect {
  int x,y,w,h;
};

struct scan_var {
  GRegex *regex;
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
  unsigned char md5[16];
  GList *vars;
  };

int ipc_open(int);
int ipc_subscribe( int sock );
gchar *ipc_poll( int sock, gint32 *etype );
int ipc_send ( int sock, gint32 type, gchar *command );
struct ipc_event ipc_parse_event ( const ucl_object_t *obj );

void place_window ( gint64 wid, gint64 pid, struct context *context );
void placement_init ( struct context *context, const ucl_object_t *obj );

void dispatch_event ( struct ipc_event *ev, struct context *context );

GtkWidget *taskbar_init ( struct context *context );
GtkIconSize taskbar_icon_size ( gchar *str );
void wintree_populate ( struct context *context );
void taskbar_refresh ( struct context *context );
void taskbar_update_window (struct ipc_event *ev, struct context *context, struct wt_window *win);
void wintree_delete_window (gint64 wid, struct context *context);
void wintree_update_window (struct ipc_event *ev, struct context *context);

void switcher_event ( struct context *context, const ucl_object_t *obj );
void switcher_update ( struct context *context );
void switcher_update_window (struct ipc_event *ev, struct context *context, struct wt_window *win);
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
struct rect parse_rect ( const ucl_object_t *obj );

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
	VF_CHMD5 = 8
	};

#endif
