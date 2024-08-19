#ifndef __SFWBAR_H__
#define __SFWBAR_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <json.h>
#include "scanner.h"

#if GLIB_MINOR_VERSION < 68
#define g_memdup2(x,y) g_memdup(x,y)
#endif

void css_init ( gchar * );
void css_file_load ( gchar * );
void css_widget_apply ( GtkWidget *widget, gchar *css );
void css_widget_cascade ( GtkWidget *widget, gpointer data );
void css_add_class ( GtkWidget *widget, gchar *css_class );
void css_remove_class ( GtkWidget *widget, gchar *css_class );

void signal_subscribe ( void );

struct json_object *jpath_parse ( gchar *path, struct json_object *obj );

void widget_set_css ( GtkWidget *, gpointer );
void widget_parse_css ( GtkWidget *widget, gchar *css );

GdkMonitor *widget_get_monitor ( GtkWidget *self );
gint socket_connect ( const gchar *sockaddr, gint to );
json_object *recv_json ( gint sock, gssize len );
void list_remove_link ( GList **list, void *child );
gchar *get_xdg_config_file ( gchar *fname, gchar *extra );
const gchar *json_string_by_name ( struct json_object *obj, gchar *name );
gint64 json_int_by_name ( struct json_object *obj, gchar *name, gint64 defval);
gboolean json_bool_by_name ( struct json_object *obj, gchar *name, gboolean defval);
gdouble json_double_by_name ( struct json_object *obj, gchar *name, gdouble defval);
gboolean pattern_match ( gchar **dict, gchar *string );
gboolean regex_match_list ( GList *dict, gchar *string );
int md5_file( gchar *path, guchar output[16] );
struct rect parse_rect ( struct json_object *obj );
guint str_nhash ( gchar *str );
gboolean str_nequal ( gchar *str1, gchar *str2 );
void *ptr_pass ( void *ptr );
gchar *str_replace ( gchar *str, gchar *old, gchar *new );
void hypr_ipc_init ( void );
enum ipc_type ipc_get ( void );
void ipc_set ( enum ipc_type new );

#endif
