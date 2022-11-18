/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include <glib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json.h>
#include <sys/time.h>

extern gchar *confname;

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

  if( file_test_read(fname) )
    return g_strdup(fname);

  if(confname!=NULL)
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
