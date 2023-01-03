/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include <glib.h>
#include <gmodule.h>
#include "module.h"
#include "config.h"
#include "basewidget.h"
#include "../meson.h"

GHashTable *expr_handlers;
GHashTable *act_handlers;
GList *invalidators;

static ModuleApi api = {
  .emit_trigger = base_widget_emit_trigger,
  .config_string = config_string,
};

gboolean module_load ( gchar *name )
{
  GModule *module;
  ModuleExpressionHandler **ehandler;
  ModuleActionHandler **ahandler;
  ModuleInvalidator invalidator;
  ModuleInitializer init;
  gint i;
  gint64 *sig;
  gchar *fname, *path;

  if(!name)
    return FALSE;
  g_debug("module: %s", name);

  fname = g_strconcat("lib",name,".so",NULL);
  path = get_xdg_config_file(fname,MODULE_DIR);
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

  if(g_module_symbol(module,"sfwbar_module_init",(void **)&init) && init)
  {
    g_debug("module: calling init function for %s",name);
    api.gmc = g_main_context_get_thread_default();
    init(&api);
  }

  if(g_module_symbol(module,"sfwbar_module_invalidate",(void **)&invalidator))
    invalidators = g_list_prepend(invalidators,invalidator);

  if(g_module_symbol(module,"sfwbar_expression_handlers",(void **)&ehandler))
    for(i=0;ehandler[i];i++)
      if(ehandler[i]->function && ehandler[i]->name)
      {
        if(!expr_handlers)
          expr_handlers = g_hash_table_new((GHashFunc)str_nhash,
            (GEqualFunc)str_nequal);
        g_debug("module: register expr function '%s'",ehandler[i]->name);
        if(g_hash_table_lookup(expr_handlers,ehandler[i]->name))
          g_message("Duplicate module expr function: %s in module %s",
              ehandler[i]->name,name);
        else
          g_hash_table_insert(expr_handlers,ehandler[i]->name,ehandler[i]);
      }

  if(g_module_symbol(module,"sfwbar_action_handlers",(void **)&ahandler))
    for(i=0;ahandler[i];i++)
      if(ahandler[i]->function && ahandler[i]->name)
      {
        if(!act_handlers)
          act_handlers = g_hash_table_new((GHashFunc)str_nhash,
            (GEqualFunc)str_nequal);
        g_debug("module: register action '%s'",ahandler[i]->name);
        if(g_hash_table_lookup(act_handlers,ahandler[i]->name))
          g_message("Duplicate module action: %s in module %s",
              ahandler[i]->name,name);
        else
          g_hash_table_insert(act_handlers,ahandler[i]->name,ahandler[i]);
      }

  return TRUE;
}

void module_invalidate_all ( void )
{
  GList *iter;

  for(iter=invalidators;iter;iter=g_list_next(iter))
    if(iter->data)
      ((ModuleInvalidator)(iter->data))();
}

void module_action_exec ( gchar *name, gchar *param, gchar *addr, void *widget,
    void *ev, void *win, void *state)
{
  ModuleActionHandler *handler;

  g_debug("module: checking action `%s`",name?name:"(null)");
  if(!act_handlers || !name)
    return;

  handler = g_hash_table_lookup(act_handlers, name);
  if(!handler)
    return;
  g_debug("module: calling action `%s`",name);

  handler->function(param, addr, widget,ev,win,state);
}

gboolean module_is_function ( GScanner *scanner )
{
  if(expr_handlers &&
      g_hash_table_lookup(expr_handlers,scanner->value.v_identifier))
    return TRUE;
  return FALSE;
}

gboolean module_is_numeric ( gchar *identifier )
{
  ModuleExpressionHandler *handler;

  if(!expr_handlers)
    return TRUE;

  handler = g_hash_table_lookup(expr_handlers,identifier);
  if(!handler)
    return TRUE;
  return handler->numeric;
}

gchar *module_get_string ( GScanner *scanner )
{
  ModuleExpressionHandler *handler;
  void **params;
  gchar *result;
  gint i;

  if(!expr_handlers)
    return NULL;

  handler = g_hash_table_lookup(expr_handlers,scanner->value.v_identifier);
  if(!handler)
    return NULL;

  g_debug("module: calling function `%s`",handler->name);
  params = expr_module_parameters(scanner,handler->parameters,handler->name);

  result = handler->function(params);

  if(params)
    for(i=0;i<strlen(handler->parameters);i++)
      g_free(params[i]);
  g_free(params);

  return result;
}
