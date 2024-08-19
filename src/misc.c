/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json.h>
#include <sys/time.h>
#include "sfwbar.h"
#include "meson.h"

gchar *confname;
gchar *sockname;

gint socket_connect ( const gchar *sockaddr, gint to )
{
  gint sock;
  struct sockaddr_un addr;
  struct timeval timeout = {.tv_sec = to/1000, .tv_usec = to%1000};

  sock = socket(AF_UNIX,SOCK_STREAM,0);
  if (sock == -1)
    return -1;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, sockaddr, sizeof(addr.sun_path) - 1);
  if(connect(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr_un)) != -1 )
    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != -1)
      return sock;
  close(sock);
  return -1;
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

void list_remove_link ( GList **list, void *child )
{
  *list = g_list_delete_link(*list,g_list_find(*list,child));
}

gboolean file_test_read ( gchar *filename )
{
  if( !g_file_test(filename, G_FILE_TEST_EXISTS) )
    return FALSE;
  if( access(filename, R_OK) == -1 )
    return FALSE;
  return TRUE;
}

/* get xdg config file name, first try user xdg config directory,
 * if file doesn't exist, try system xdg data dirs */
gchar *get_xdg_config_file ( gchar *fname, gchar *extra )
{
  gchar *full, *dir;
  const gchar * const *xdg_data;
  gint i;

  if(fname && *fname == '/')
  {
    if(file_test_read(fname))
      return g_strdup(fname);
    else
      return NULL;
  }

  if(confname)
  {
    dir = g_path_get_dirname(confname);
    full = g_build_filename ( dir, fname, NULL );
    g_free(dir);
    if( file_test_read(full) )
      return full;
    g_free(full);
  }

  full = g_build_filename ( g_get_user_config_dir(), "sfwbar", fname, NULL );
  if( file_test_read(full) )
    return full;
  g_free(full);

  xdg_data = g_get_system_data_dirs();
  for(i=0;xdg_data[i];i++)
  {
    full = g_build_filename ( xdg_data[i], "sfwbar", fname, NULL );
    if( file_test_read(full) )
      return full;
    g_free(full);
  }

  full = g_build_filename( SYSTEM_CONF_DIR, fname, NULL);
  if( file_test_read(full) )
    return full;
  g_free(full);

  if(!extra)
    return NULL;
  full = g_build_filename ( extra, fname, NULL );
  if( file_test_read(full) )
    return full;
  g_free(full);

  return NULL;
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
gdouble json_double_by_name ( struct json_object *obj, gchar *name, gdouble defval)
{
  struct json_object *ptr;

  if(json_object_object_get_ex(obj,name,&ptr))
    return json_object_get_double(ptr);

  return defval;
}

gboolean pattern_match ( gchar **dict, gchar *string )
{
  gint i;

  if(!dict)
    return FALSE;

  for(i=0;dict[i];i++)
    if(g_pattern_match_simple(dict[i],string))
      return TRUE;

  return FALSE;
}

gboolean regex_match_list ( GList *dict, gchar *string )
{
  GList *iter;

  for(iter=dict;iter;iter=g_list_next(iter))
    if(g_regex_match(iter->data, string, 0, NULL))
      return TRUE;

  return FALSE;
}

/* compute md5 checksum for a file */
int md5_file( gchar *path, guchar output[16] )
{
    FILE *f;
    gssize n;
    GChecksum *ctx;
    guchar buf[1024];

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( 1 );

    ctx = g_checksum_new(G_CHECKSUM_MD5);

    while( ( n = fread( buf, 1, sizeof( buf )-1, f ) ) > 0 )
    {
      buf[n]='\0';
      g_checksum_update( ctx, buf, n );
    }

    n=16;
    g_checksum_get_digest( ctx, output, (gsize *)&n );

    g_checksum_free( ctx );
    fclose( f );
    return( 0 );
}

void str_assign ( gchar **dest, gchar *source )
{
  g_free(*dest);
  if(source==NULL)
    *dest = NULL;
  else
    *dest = g_strdup(source);
}

guint str_nhash ( gchar *str )
{
  guint ret = 5381;
  guint i=0;

  while(str[i])
    ret += g_ascii_toupper(str[i++]);

  return ret;
}

gboolean str_nequal ( gchar *str1, gchar *str2 )
{
  return (!g_ascii_strcasecmp(str1,str2));
}

gchar *str_replace ( gchar *str, gchar *old, gchar *new )
{
  gssize olen, nlen;
  gint n;
  gchar *ptr, *pptr, *dptr, *dest;

  if(!str || !old || !new)
    return g_strdup(str);

  olen = strlen(old);
  nlen = strlen(new);

  n=0;
  ptr = strstr(str,old);
  while(ptr)
  {
    n++;
    ptr = strstr(ptr+olen,old);
  }

  if(!n)
    return g_strdup(str);

  dest = g_malloc(strlen(str)+n*(nlen-olen)+1);

  pptr = str;
  ptr = strstr(str,old);
  dptr = dest;
  while(ptr)
  {
    strncpy(dptr,pptr,ptr-pptr);
    dptr += ptr - pptr;
    strcpy(dptr,new);
    dptr += nlen;
    pptr = ptr + olen;
    ptr = strstr(pptr,old);
  }
  strcpy(dptr,pptr);

  return dest;
}

void *ptr_pass ( void *ptr )
{
  return ptr;
}

GdkMonitor *widget_get_monitor ( GtkWidget *self )
{
  GtkWidget *parent, *w;
  GdkWindow *win;
  GdkDisplay *disp;

  g_return_val_if_fail(GTK_IS_WIDGET(self),NULL);

  if(gtk_widget_get_mapped(self))
    win = gtk_widget_get_window(self);
  else
  {
    for(w=self; w; w=gtk_widget_get_parent(w))
      if( (parent=g_object_get_data(G_OBJECT(w), "parent_window")) )
        break;
    if(!w)
      return NULL;
    win = gtk_widget_get_window(parent);
  }

  if(!win || !(disp = gdk_window_get_display(win)) )
      return NULL;
  return gdk_display_get_monitor_at_window(disp, win);
}
