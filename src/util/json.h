#ifndef __JSON_H__
#define __JSON_H__

#include <json.h>
#include <sys/un.h>
#include <gdk/gdk.h>

#define json_int_ptr_by_name(json, key, dflt) \
  GINT_TO_POINTER(json_int_by_name(json, key, dflt))

gint socket_connect ( const gchar *sockaddr, gint to );
gboolean recv_retry ( gint sock, gpointer buff, gsize len );
json_object *recv_json ( gint sock, gssize len );
json_object *json_recv_channel ( GIOChannel *chan );

const gchar *json_string_by_name ( struct json_object *obj, gchar *name );
gint64 json_int_by_name ( struct json_object *obj, gchar *name, gint64 defval);
gboolean json_bool_by_name ( struct json_object *obj, gchar *name, gboolean defval);
gdouble json_double_by_name ( struct json_object *obj, gchar *name, gdouble defval);
struct json_object *json_array_by_name ( struct json_object *obj, gchar *key );
struct json_object *json_node_by_name ( struct json_object *json, gchar *key );
GdkRectangle json_rect_get ( struct json_object *json );
gboolean json_array_to_strv ( struct json_object *json, gchar ***strv );
void json_foreach ( struct json_object *json,
    void (*func)(struct json_object *, gpointer data), gpointer data );

struct json_object *jpath_parse ( gchar *path, struct json_object *obj );

#endif
