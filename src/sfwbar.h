#ifndef __SFWBAR_H__
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <json.h>
#include "scanner.h"

enum ipc_type {
  IPC_SWAY    = 1,
  IPC_HYPR    = 2,
  IPC_WAYLAND = 3
};

void hypr_ipc_init ( void );
enum ipc_type ipc_get ( void );
void ipc_set ( enum ipc_type new );
void css_init ( gchar * );
void css_file_load ( gchar * );
void css_widget_apply ( GtkWidget *widget, gchar *css );
void css_widget_cascade ( GtkWidget *widget, gpointer data );
void css_add_class ( GtkWidget *widget, gchar *css_class );
void css_remove_class ( GtkWidget *widget, gchar *css_class );

void client_exec ( ScanFile *file );
void client_socket ( ScanFile *file );

char *expr_parse ( gchar *expr_str, guint * );
gboolean expr_cache ( gchar **expr, gchar **cache, guint *vcount );
void **expr_module_parameters ( GScanner *scanner, gchar *spec, gchar *name );
gchar *expr_dtostr ( double num, gint dec );
void expr_lib_init ( void );

struct json_object *jpath_parse ( gchar *path, struct json_object *obj );

void widget_set_css ( GtkWidget *, gpointer );
void widget_parse_css ( GtkWidget *widget, gchar *css );

void mpd_ipc_init ( ScanFile *file );
void mpd_ipc_command ( gchar *command );

gint socket_connect ( const gchar *sockaddr, gint to );
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

#endif
