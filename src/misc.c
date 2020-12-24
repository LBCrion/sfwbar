/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <json.h>

/* get xdg config file name, first try user xdg config directory,
 * if file doesn't exist, try /usr/share/swfbar/ */
gchar *get_xdg_config_file ( gchar *fname )
{
  gchar *full;
  full = g_build_filename ( g_get_user_config_dir(), "sfwbar", fname, NULL );
  if( g_file_test(full, G_FILE_TEST_EXISTS) )
    return full;
  g_free(full);
  full = g_build_filename ( "/usr/share", "sfwbar", fname, NULL );
  if( g_file_test(full, G_FILE_TEST_EXISTS) )
    return full;
  return NULL;
}

/* get string value from an object within current object */
gchar *json_string_by_name ( json_object *obj, gchar *name)
{
  json_object *ptr;

  if(json_object_object_get_ex(obj,name,&ptr))
    return g_strdup(json_object_get_string(ptr));
  else
    return NULL;
}

/* get int value from an object within current object */
gint64 json_int_by_name ( json_object *obj, gchar *name, gint64 defval )
{
  json_object *ptr;

  if(json_object_object_get_ex(obj,name,&ptr))
    if(json_object_is_type(ptr,json_type_int))
      return json_object_get_int64(ptr);
  
  return defval;
}

/* get bool value from an object within current object */
gboolean json_bool_by_name ( json_object *obj, gchar *name, gboolean defval )
{
  json_object *ptr;

  if(json_object_object_get_ex(obj,name,&ptr))
    if(json_object_is_type(ptr,json_type_boolean))
      return json_object_get_boolean(ptr);
  
  return defval;
}

/* compute md5 checksum for a file */
int md5_file( char *path, unsigned char output[16] )
{
    FILE *f;
    gssize n;
    GChecksum *ctx;
    unsigned char buf[1024];

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( 1 );

    ctx = g_checksum_new(G_CHECKSUM_MD5);

    while( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
        g_checksum_update( ctx, buf, n );

    n=16;
    g_checksum_get_digest( ctx, output, (gsize *)&n );

    g_checksum_free( ctx );
    fclose( f );
    return( 0 );
}
