/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <ucl.h>

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
gchar *ucl_string_by_name ( const ucl_object_t *obj, gchar *name )
{
  const ucl_object_t *ptr;

  if((ptr = ucl_object_lookup(obj,name))!=NULL)
    if(ucl_object_type(ptr)==UCL_STRING)
      return g_strdup(ucl_object_tostring(ptr));
  return NULL;
}

/* get int value from an object within current object */
gint64 ucl_int_by_name ( const ucl_object_t *obj, gchar *name, gint64 defval)
{
  const ucl_object_t *ptr;

  if((ptr = ucl_object_lookup(obj,name))!=NULL)
    if(ucl_object_type(ptr)==UCL_INT)
      return ucl_object_toint(ptr);

  return defval;
}

/* get bool value from an object within current object */
gboolean ucl_bool_by_name ( const ucl_object_t *obj, gchar *name, gboolean defval)
{
  const ucl_object_t *ptr;

  if((ptr = ucl_object_lookup(obj,name))!=NULL)
    if(ucl_object_type(ptr)==UCL_BOOLEAN)
      return ucl_object_toboolean(ptr);

  return defval;
}

/* get double value from an object within current object */
gdouble ucl_double_by_name ( const ucl_object_t *obj, gchar *name, gdouble defval)
{
  const ucl_object_t *ptr;

  if((ptr = ucl_object_lookup(obj,name))!=NULL)
    if(ucl_object_type(ptr)==UCL_FLOAT)
      return ucl_object_todouble(ptr);

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
