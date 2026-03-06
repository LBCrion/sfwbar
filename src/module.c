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

static GList *module_list, *iface_providers, *invalidators;

static gint module_interface_active ( gconstpointer iface, gconstpointer d )
{
  return !(!g_strcmp0(((ModuleInterfaceV1 *)iface)->interface, d) &&
        ((ModuleInterfaceV1 *)iface)->active);
}

static gint module_interface_ready ( gconstpointer iface, gconstpointer d )
{
  return !(!g_strcmp0(((ModuleInterfaceV1 *)iface)->interface, d) &&
        ((ModuleInterfaceV1 *)iface)->ready);
}

static gint module_invocation_run ( void (*func)(void) )
{
  func();
  return G_SOURCE_REMOVE;
}

static void module_interface_select ( gchar *iface )
{
  ModuleInterfaceV1 *new, *old;
  GList *new_link, *old_link;

  new_link = g_list_find_custom(iface_providers, iface,
      module_interface_ready);
  old_link = g_list_find_custom(iface_providers, iface,
      module_interface_active);

  if(old_link && new_link != old_link)
  {
    old = old_link->data;
    g_debug("module: deactivating '%s' provider '%s'", iface,
        old->provider);
    g_main_context_invoke(old->context, (GSourceFunc)module_invocation_run,
        old->deactivate);
    old->active = FALSE;
  }

  if(new_link && new_link != old_link)
  {
    new = new_link->data;
    g_debug("module: activating '%s' provider '%s'", iface, new->provider);
    g_main_context_invoke(new->context, (GSourceFunc)module_invocation_run,
        new->activate);
    new->active = TRUE;
  }
}

static gint module_interface_activate_impl ( ModuleInterfaceV1 *iface )
{
  iface->ready = TRUE;
  module_interface_select(iface->interface);
  return G_SOURCE_REMOVE;
}

void module_interface_activate ( ModuleInterfaceV1 *iface )
{
  g_main_context_invoke(NULL, (GSourceFunc)module_interface_activate_impl,
      iface);
}

static gint module_interface_deactivate_impl ( ModuleInterfaceV1 *iface )
{
  iface->ready = FALSE;
  module_interface_select(iface->interface);
  return G_SOURCE_REMOVE;
}

void module_interface_deactivate ( ModuleInterfaceV1 *iface )
{
  g_main_context_invoke(NULL, (GSourceFunc)module_interface_deactivate_impl, iface);
}

static void module_interface_add ( ModuleInterfaceV1 *iface,
    gchar *name, GMainContext *context )
{
  if(!iface || !iface->interface || !iface->activate || !iface->deactivate)
    return;

  g_debug("module: adding provider: '%s' for interface '%s'",
      iface->provider, iface->interface);
  iface->context = context;
  iface_providers = g_list_append(iface_providers, iface);
  module_interface_select(iface->interface);
}

gchar *module_interface_provider_get ( gchar *interface )
{
  GList *link;

  if( !(link = g_list_find_custom(iface_providers, interface, module_interface_active)) )
    return g_strdup("");

  return g_strdup(((ModuleInterfaceV1 *)(link->data))->provider);
}

static gint module_source_func ( module_init_closure_t *closure )
{
  if( !(closure->result = closure->func()) )
    closure->context = NULL;
  g_mutex_lock(&closure->mutex);
  closure->ready = TRUE;
  g_cond_signal(&closure->cond);
  g_mutex_unlock(&closure->mutex);

  return G_SOURCE_REMOVE;
}

static gpointer module_thread_func ( module_init_closure_t *closure )
{
  GMainLoop *loop;

  closure->context = g_main_context_new();
  g_main_context_push_thread_default(closure->context);
  loop = g_main_loop_new(closure->context, FALSE);

  module_source_func(closure);

  if(closure->result)
    g_main_loop_run(loop);
  g_main_loop_unref(loop);

  return NULL;
}

static gboolean module_init ( gchar *name, GModule *module, GMainContext **ctx )
{
  static GMainContext *module_context;
  gboolean (*init)(void);
  module_init_closure_t *closure;
  module_thread_t thread, *tptr ;
  gboolean result;

  *ctx = NULL;
  if(!g_module_symbol(module, "sfwbar_module_init", (void **)&init) || !init)
    return FALSE;

  thread = (g_module_symbol(module, "sfwbar_module_thread", (void **)&tptr) &&
      tptr)? *tptr : MODULE_THREAD_MAIN;

  if(thread == MODULE_THREAD_MAIN)
    return init();

  closure = g_malloc0(sizeof(module_init_closure_t));
  closure->thread = thread;
  closure->func = init;

  g_mutex_lock(&closure->mutex);
  if(thread == MODULE_THREAD_MODULE && module_context)
    g_main_context_invoke(module_context, (GSourceFunc)module_source_func, closure);
  else
    g_thread_new(name, (GThreadFunc)module_thread_func, closure);
  while(!closure->ready)
    g_cond_wait(&closure->cond, &closure->mutex);
  g_mutex_unlock(&closure->mutex);

  if(thread == MODULE_THREAD_MODULE && !module_context)
    module_context = closure->context;
  *ctx = (thread == MODULE_THREAD_MODULE)? module_context : closure->context;
  result = closure->result;
  g_free(closure);

  return result;
}

gboolean module_load ( gchar *name )
{
  GModule *module;
  GMainContext *context;
  ModuleInvalidator invalidator;
  ModuleInterfaceV1 *iface;
  gint64 *sig;
  guint16 *ver;
  gchar *fname, *path;

  if(!name)
    return FALSE;

  if(g_list_find_custom(module_list, name, (GCompareFunc)g_strcmp0))
    return FALSE;

  fname = g_strconcat(name, ".so", NULL);
  path = g_build_filename(MODULE_DIR, fname, NULL);
  g_free(fname);
  g_debug("module: %s --> %s", name, path);
  module = g_module_open(path, G_MODULE_BIND_LOCAL);
  g_free(path);

  if(!module)
  {
    g_debug("module: failed to load %s", name);
    return FALSE;
  }

  if(!g_module_symbol(module, "sfwbar_module_signature", (void **)&sig) ||
      !sig || *sig != 0x73f4d956a1 )
  {
    g_debug("module: signature check failed for %s", name);
    g_module_close(module);
    return FALSE;
  }
  if(!g_module_symbol(module, "sfwbar_module_version", (void **)&ver) ||
      !ver || *ver != MODULE_API_VERSION )
  {
    g_debug("module: invalid version for %s", name);
    g_module_close(module);
    return FALSE;
  }

  module_list = g_list_prepend(module_list, g_strdup(name));

  if(!module_init(name, module, &context))
  {
    g_debug("module: init failed for %s", name);
    g_module_close(module);
    return FALSE;
  }

  if(g_module_symbol(module, "sfwbar_module_invalidate", (void **)&invalidator))
    invalidators = g_list_prepend(invalidators, invalidator);

  if(g_module_symbol(module, "sfwbar_interface", (void **)&iface))
    module_interface_add(iface, name, context);

  return TRUE;
}

void module_invalidate_all ( void )
{
  GList *iter;

  for(iter=invalidators; iter; iter=g_list_next(iter))
    if(iter->data)
      ((ModuleInvalidator)(iter->data))();
}

guint module_timeout_add ( guint i, GSourceFunc func, gpointer d )
{
  GSource *src = g_timeout_source_new(i);

  g_source_set_callback(src, func, d, NULL);
  return g_source_attach(src, g_main_context_get_thread_default());
}

guint module_idle_add ( GSourceFunc func, gpointer d )
{
  GSource *src = g_idle_source_new();

  g_source_set_callback(src, func, d, NULL);
  return g_source_attach(src, g_main_context_get_thread_default());
}

guint module_channel_watch_add ( GIOChannel *chan, gint pri, GIOCondition cond,
    GIOFunc func, gpointer d, GDestroyNotify free_func )
{
  GSource *src = g_io_create_watch(chan, cond);

  g_source_set_priority(src, pri);
  g_source_set_callback(src, (GSourceFunc)func, d, free_func);
  return g_source_attach(src, g_main_context_get_thread_default());
}
