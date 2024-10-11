/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "util/json.h"
#include <sys/socket.h>
#include <unistd.h>

gint socket_connect ( const gchar *sockaddr, gint to )
{
  gint sock;
  struct sockaddr_un addr;
  struct timeval timeout = {.tv_sec = to/1000, .tv_usec = to%1000};

  if( (sock = socket(AF_UNIX,SOCK_STREAM,0))==-1)
    return -1;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, sockaddr, sizeof(addr.sun_path) - 1);
  if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_un))!=-1 &&
      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))!=-1)
    return sock;
  close(sock);
  return -1;
}

gssize recv_retry ( gint sock, gpointer buff, gsize len )
{
  gssize rlen, tlen=0;

  while(tlen<len && (rlen = recv(sock, buff+tlen, len-tlen, 0))>0 )
    tlen += rlen;

  return tlen;
}

json_object *recv_json ( gint sock, gssize len )
{
  static gchar *buf;
  const gsize bufsize = 1024;
  json_tokener *tok;
  json_object *json = NULL;
  gssize rlen;

  if(!buf)
    buf = g_malloc(bufsize);

  tok = json_tokener_new();

  while(len && (rlen = recv(sock, buf, MIN(len<0?bufsize:len, bufsize), 0))>0 )
  {
    json = json_tokener_parse_ex(tok, buf, rlen);
    if(len>0)
      len-=MIN(rlen, MIN(len, bufsize));
  }
  json_tokener_free(tok);

  return json;
}

/* get string value from an object within current object */
const gchar *json_string_by_name ( struct json_object *obj, gchar *name )
{
  struct json_object *ptr;

  if(json_object_object_get_ex(obj,name,&ptr))
    return json_object_get_string(ptr);
  return NULL;
}

/* get int value from an object within current object */
gint64 json_int_by_name ( struct json_object *obj, gchar *name, gint64 defval)
{
  struct json_object *ptr;

  if(json_object_object_get_ex(obj,name,&ptr))
    return json_object_get_int64(ptr);

  return defval;
}

/* get bool value from an object within current object */
gboolean json_bool_by_name ( struct json_object *obj, gchar *name, gboolean defval)
{
  struct json_object *ptr;

  if(json_object_object_get_ex(obj,name,&ptr) && ptr)
    return json_object_get_boolean(ptr);

  return defval;
}

/* get double value from an object within current object */
gdouble json_double_by_name ( struct json_object *obj, gchar *key, gdouble defval)
{
  struct json_object *ptr;

  if(json_object_object_get_ex(obj, key, &ptr))
    return json_object_get_double(ptr);

  return defval;
}

struct json_object *json_array_by_name ( struct json_object *obj, gchar *key )
{
  struct json_object *ptr;

  if(json_object_object_get_ex(obj, key, &ptr) &&
      json_object_is_type(ptr, json_type_array))
    return ptr;
  return NULL;
}

struct json_object *json_node_by_name ( struct json_object *json, gchar *key )
{
  struct json_object *ptr;

  if(json_object_object_get_ex(json, key, &ptr))
    return ptr;
  return NULL;
}

GdkRectangle json_rect_get ( struct json_object *json )
{
  GdkRectangle ret;

  ret.x = json_int_by_name(json, "x", -1);
  ret.y = json_int_by_name(json, "y", -1);
  ret.width = json_int_by_name(json, "width", -1);
  ret.height = json_int_by_name(json, "height", -1);

  return ret;
}

