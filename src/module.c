/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gmodule.h>
#include "meson.h"
#include "module.h"
#include "trigger.h"
#include "config/config.h"
#include "util/string.h"
#include "vm/expr.h"
#include "vm/vm.h"

static GList *module_list;
static GHashTable *interfaces;
static GList *invalidators;

void module_interface_select ( gchar *interface )
{
  ModuleInterfaceList *list;
  ModuleInterfaceV1 *new;
  GList *iter;

  if( !(list = g_hash_table_lookup(interfaces, interface)) )
    return;

  for(iter=list->list; iter; iter=g_list_next(iter))
    if(((ModuleInterfaceV1 *)iter->data)->ready)
      break;

  new = iter?iter->data:NULL;
  if(list->active == new)
    return;

  if(list->active && list->active->active)
  {
    list->active->ready = FALSE;
    list->active->deactivate();
    if(list->active && !list->active->active)
      module_interface_select(interface);
    return;
  }

  g_debug("module: switching interface '%s' from '%s' to '%s'", interface,
      list->active?list->active->provider:"none", new?new->provider:"none");

  if(list->active && list->active->finalize)
      list->active->finalize();

  list->active = new;
  if(new)
  {
    new->activate();
    new->active = TRUE;
  }
}

void module_interface_activate ( ModuleInterfaceV1 *iface )
{
  iface->ready = TRUE;
  module_interface_select(iface->interface);
}

void module_interface_deactivate ( ModuleInterfaceV1 *iface )
{
  iface->ready = FALSE;
  module_interface_select(iface->interface);
}

static void module_interface_add ( ModuleInterfaceV1 *iface,
    gchar *name )
{
  ModuleInterfaceList *list;

  if(!iface || !iface->interface || !iface->activate || !iface->deactivate)
    return;

  if(!interfaces)
    interfaces = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, NULL);

  if( !(list=g_hash_table_lookup(interfaces, iface->interface)) )
  {
    list = g_malloc0(sizeof(ModuleInterfaceList));
    g_hash_table_insert(interfaces, g_strdup(iface->interface), list);
  }
  g_debug("module: adding provider: '%s' for interface '%s'",
      iface->provider, iface->interface);
  list->list = g_list_append(list->list, iface);
  module_interface_select(iface->interface);
}

gchar *module_interface_provider_get ( gchar *interface )
{
  ModuleInterfaceList *list;

  if( !(list = g_hash_table_lookup(interfaces, interface)) || !list->active )
    return g_strdup("");

  return g_strdup(list->active->provider);
}

gboolean module_load ( gchar *name )
{
  GModule *module;
  ModuleInvalidator invalidator;
  ModuleInitializer init;
  ModuleInterfaceV1 *iface;
  gint64 *sig;
  guint16 *ver;
  gchar *fname, *path;

  if(!name)
    return FALSE;
  g_debug("module: %s", name);

  if(g_list_find_custom(module_list,name,(GCompareFunc)g_strcmp0))
    return FALSE;

  fname = g_strconcat(name,".so",NULL);
  path = g_build_filename(MODULE_DIR, fname, NULL);
  g_free(fname);
  g_debug("module: %s --> %s",name,path);
  module = g_module_open(path, G_MODULE_BIND_LOCAL);
  g_free(path);

  if(!module)
  {
    g_debug("module: failed to load %s",name);
    return FALSE;
  }

  if(!g_module_symbol(module,"sfwbar_module_signature",(void **)&sig) ||
      !sig || *sig != 0x73f4d956a1 )
  {
    g_debug("module: signature check failed for %s",name);
    return FALSE;
  }
  if(!g_module_symbol(module,"sfwbar_module_version",(void **)&ver) ||
      !ver || *ver != MODULE_API_VERSION )
  {
    g_debug("module: invalid version for %s",name);
    return FALSE;
  }

  module_list = g_list_prepend(module_list,g_strdup(name));

  if(g_module_symbol(module,"sfwbar_module_init",(void **)&init) && init)
  {
    g_debug("module: calling init function for %s",name);
    if(!init())
      return FALSE;
  }

  if(g_module_symbol(module,"sfwbar_module_invalidate",(void **)&invalidator))
    invalidators = g_list_prepend(invalidators,invalidator);

  if(g_module_symbol(module,"sfwbar_interface",(void **)&iface))
    module_interface_add(iface, name);

  return TRUE;
}

void module_invalidate_all ( void )
{
  GList *iter;

  for(iter=invalidators;iter;iter=g_list_next(iter))
    if(iter->data)
      ((ModuleInvalidator)(iter->data))();
}
