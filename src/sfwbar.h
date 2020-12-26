#if !defined(__SFWBAR_H__)
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <json.h>

struct ipc_event {
  gint8 event;
  gint64 pid;
  gchar *title;
  gchar *appid;
};

struct context {
  gint32 features;
  int ipc;
  gint32 tb_focus;
  gint32 tb_rows;
  gint32 tb_isize;
  gint32 position;
  gint32 wp_x,wp_y;
  GtkCssProvider *css;
  GtkWidget *box;
  GList *buttons;
  GList *widgets;
  GList *file_list;
  GList *scan_list;
  int default_dec;
  int line_num;
  int buff_len;
  char *read_buff;
  char *ret_val;
};

struct tb_button {
  GtkWidget *button;
  GtkWidget *label;
  gint64 pid;
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

int ipc_open();
int ipc_subscribe( int sock );
json_object *ipc_poll( int sock );
int ipc_send ( int sock, gint32 type, gchar *command );
struct ipc_event ipc_parse_event ( json_object *obj );

void place_window ( gint64 pid, struct context *context );

void dispatch_event ( struct ipc_event *ev, struct context *context );

GtkWidget *taskbar_init ( struct context *context );
GtkIconSize taskbar_icon_size ( gchar *str );
void taskbar_populate ( struct context *context );
void taskbar_refresh ( struct context *context );
void taskbar_delete_window (gint64 pid, struct context *context);
void taskbar_update_window (struct ipc_event *ev, struct context *context);

GtkWidget *layout_init ( struct context *context, json_object *obj );
void widget_update_all( struct context *context );
void widget_action ( GtkWidget *widget, gpointer data );

GtkWidget *clamp_grid_new();
int scanner_expire ( GList *start );
int update_var_tree ( struct context *context );
int update_var_file ( struct context *context, FILE *in, GList *var_list );
int reset_var_list ( GList *var_list );
int update_var_files ( struct context *context, struct scan_file *file );
char *parse_expr ( struct context *context, char *expr_str );
char *string_from_name ( struct context *context, char *name );
double numeric_from_name ( struct context *context, char *name );
char *time_str ( void );
char *extract_str ( char *str, char *pattern );
void *list_by_name ( GList *prev, char *name );
char *parse_identifier ( char *id, char **fname );
void scanner_init ( struct context *context, json_object *obj );
GList *scanner_add_vars( struct context *context, json_object *obj, struct scan_file *file );
char *numeric_to_str ( double num, int dec );
char *str_mid ( char *str, int c1, int c2 );

gchar *get_xdg_config_file ( gchar *fname );
gchar *json_string_by_name ( json_object *obj, gchar *name);
gint64 json_int_by_name ( json_object *obj, gchar *name, gint64 defval );
gboolean json_bool_by_name ( json_object *obj, gchar *name, gboolean defval );
int md5_file( char *path, unsigned char output[16] );

#define SCAN_VAR(x) ((struct scan_var *)x)
#define SCAN_FILE(x) ((struct scan_file *)x)
#define AS_BUTTON(x) ((struct tb_button *)(x))

enum {
        F_TASKBAR   = 1<<0,
        F_PLACEMENT = 1<<1,
        F_TB_ICON   = 1<<2,
        F_TB_LABEL  = 1<<3,
        F_TB_EXPAND = 1<<4
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
