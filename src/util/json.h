#ifndef __JSON_H__
#define __JSON_H__

#include <json.h>
#include <sys/un.h>
#include <gdk/gdk.h>

gint socket_connect ( const gchar *sockaddr, gint to );
gssize recv_retry ( gint sock, gpointer buff, gssize len );
json_object *recv_json ( gint sock, gssize len );

const gchar *json_string_by_name ( struct json_object *obj, gchar *name );
gint64 json_int_by_name ( struct json_object *obj, gchar *name, gint64 defval);
gboolean json_bool_by_name ( struct json_object *obj, gchar *name, gboolean defval);
gdouble json_double_by_name ( struct json_object *obj, gchar *name, gdouble defval);
struct json_object *json_array_by_name ( struct json_object *obj, gchar *key );
struct json_object *json_node_by_name ( struct json_object *json, gchar *key );
GdkRectangle json_rect_get ( struct json_object *json );

struct json_object *jpath_parse ( gchar *path, struct json_object *obj );

#endif
