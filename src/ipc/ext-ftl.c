/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include "wayland.h"
#include "wintree.h"
#include "gui/capture.h"
#include "ext-foreign-toplevel-list-v1.h"

#define EXT_FOREIGN_TOPLEVEL_LIST_VERSION 1

static struct ext_foreign_toplevel_list_v1 *ftl_list;

GHashTable *ext_ftl_map;

static gboolean ext_ftl_map_match ( gpointer key, gpointer value, gpointer d )
{
  return (value == d);
}

gpointer ext_ftl_lookup ( gchar *id )
{
  return id? g_hash_table_lookup(ext_ftl_map, id) : NULL;
}

static void ext_ftl_handle_closed ( void *data,
    struct ext_foreign_toplevel_handle_v1 *toplevel )
{
  g_hash_table_foreach_remove(ext_ftl_map, ext_ftl_map_match, toplevel);
}

static void ext_ftl_handle_done ( void *data,
    struct ext_foreign_toplevel_handle_v1 *toplevel )
{
  /* nop */
}

static void ext_ftl_handle_title ( void *data,
    struct ext_foreign_toplevel_handle_v1 *toplevel,
    const char *title )
{
  /* nop */
}

static void ext_ftl_handle_appid ( void *data,
    struct ext_foreign_toplevel_handle_v1 *toplevel,
    const char *appid )
{
  /* nop */
}

static void ext_ftl_handle_identifier ( void *data,
    struct ext_foreign_toplevel_handle_v1 *toplevel,
    const char *id )
{
  GList *iter;

  g_hash_table_insert(ext_ftl_map, g_strdup(id), toplevel);

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    if(!g_strcmp0(((window_t *)(iter->data))->stable_id, id))
      capture_window(iter->data);
}

static struct ext_foreign_toplevel_handle_v1_listener ext_ftl_handle_impl = {
  .closed = ext_ftl_handle_closed,
  .done = ext_ftl_handle_done,
  .title = ext_ftl_handle_title,
  .app_id = ext_ftl_handle_appid,
  .identifier = ext_ftl_handle_identifier,
};

static void ext_ftl_toplevel ( void *data,
    struct ext_foreign_toplevel_list_v1 *list,
    struct ext_foreign_toplevel_handle_v1 *toplevel )
{
  ext_foreign_toplevel_handle_v1_add_listener(toplevel, &ext_ftl_handle_impl,
      NULL);
}

static void ext_ftl_finished ( void *data,
    struct ext_foreign_toplevel_list_v1 *list )
{
  /* nop */
}

static struct ext_foreign_toplevel_list_v1_listener ext_ftl_impl = {
  .toplevel = ext_ftl_toplevel,
  .finished = ext_ftl_finished,
};

void ext_ftl_init ( void )
{
  if( !(ftl_list = wayland_iface_register(
          ext_foreign_toplevel_list_v1_interface.name,
          EXT_FOREIGN_TOPLEVEL_LIST_VERSION,
          EXT_FOREIGN_TOPLEVEL_LIST_VERSION,
          &ext_foreign_toplevel_list_v1_interface)) )
    return;

  ext_ftl_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  ext_foreign_toplevel_list_v1_add_listener(ftl_list, &ext_ftl_impl, NULL);
}
