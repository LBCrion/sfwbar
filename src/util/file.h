#ifndef __SFWBAR_FILE__
#define __SFWBAR_FILE__

#include <glib.h>

gboolean file_test_read ( gchar *filename );
gchar *get_xdg_config_file ( gchar *fname, gchar *extra );

#endif
