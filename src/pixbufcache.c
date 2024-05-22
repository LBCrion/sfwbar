/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include <glib.h>
#include <gdk.h>

static GHashTable *scaleimage_pb_cache;

gboolean pb_cache_insert ( gchar *name, GdkPixbuf *pb )
{
  if(!pb_cache)
    pb_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
        g_free, g_object_unref);

  return g_hash_table_insert(pb_cache, name, pb);
}

GdkPixbuf pb_cache_lookup ( gchar *name )
{
  if(!g_str_has_prefix(name, "<pixbufcache/>"))
    return NULL;

  return g_hash_table_lookup(pb_cache, name+14);
}
