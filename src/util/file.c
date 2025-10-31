/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <glib.h>
#include <unistd.h>
#include "meson.h"

gchar *confname;
gchar *sockname;

gboolean file_test_read ( gchar *filename )
{
  if( !filename || !g_file_test(filename, G_FILE_TEST_EXISTS) )
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

