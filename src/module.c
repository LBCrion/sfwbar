/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "expr.h"
#include <glib.h>
#include <gmodule.h>
#include "module.h"
#include "config.h"
#include "basewidget.h"
#include "../meson.h"

GList *module_list;
GHashTable *expr_handlers;
GData *act_handlers;
GList *invalidators;

static ModuleApiV1 api_v1 = {
  .emit_trigger = base_widget_emit_trigger,
  .config_string = config_string,
};

void module_expr_funcs_add ( ModuleExpressionHandlerV1 **ehandler,gchar *name )
{
  gint i;

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
      {
        g_hash_table_insert(expr_handlers,ehandler[i]->name,ehandler[i]);
        expr_dep_trigger(ehandler[i]->name);
      }
    }
}

void module_actions_add ( ModuleActionHandlerV1 **ahandler, gchar *name )
{
  gint i;
  gchar *lname;

  for(i=0;ahandler[i];i++)
    if(ahandler[i]->function && ahandler[i]->name)
    {
      lname = g_ascii_strdown(ahandler[i]->name,-1);
      g_debug("module: register action '%s'",ahandler[i]->name);
      if(g_datalist_get_data(&act_handlers,lname))
        g_message("Duplicate module action: %s in module %s",
            ahandler[i]->name,name);
      else
        g_datalist_set_data(&act_handlers,lname,ahandler[i]);
      g_free(lname);
    }
}

gboolean module_load ( gchar *name )
{
  GModule *module;
  ModuleExpressionHandlerV1 **ehandler;
  ModuleActionHandlerV1 **ahandler;
  ModuleInvalidator invalidator;
  ModuleInitializer init;
  gint64 *sig;
  guint16 *ver;
  gchar *fname, *path;

  if(!name)
    return FALSE;
  g_debug("module: %s", name);

  if(g_list_find_custom(module_list,name,(GCompareFunc)g_strcmp0))
    return FALSE;

  fname = g_strconcat(name,".so",NULL);
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
  if(!g_module_symbol(module,"sfwbar_module_version",(void **)&ver) ||
      !ver || *ver != 1 )
  {
    g_debug("module: invalid version for %s",name);
    return FALSE;
  }

  module_list = g_list_prepend(module_list,g_strdup(name));

  if(g_module_symbol(module,"sfwbar_module_init",(void **)&init) && init)
  {
    g_debug("module: calling init function for %s",name);
    api_v1.gmc = g_main_context_get_thread_default();
    init(&api_v1);
  }

  if(g_module_symbol(module,"sfwbar_module_invalidate",(void **)&invalidator))
    invalidators = g_list_prepend(invalidators,invalidator);

  if(g_module_symbol(module,"sfwbar_expression_handlers",(void **)&ehandler))
    module_expr_funcs_add(ehandler, name);

  if(g_module_symbol(module,"sfwbar_action_handlers",(void **)&ahandler))
    module_actions_add(ahandler, name);

  return TRUE;
}

void module_invalidate_all ( void )
{
  GList *iter;

  for(iter=invalidators;iter;iter=g_list_next(iter))
    if(iter->data)
      ((ModuleInvalidator)(iter->data))();
}

ModuleActionHandlerV1 *module_action_get ( GQuark quark )
{
  return g_datalist_id_get_data(&act_handlers, quark);
}

void module_action_exec ( GQuark quark, gchar *param, gchar *addr, void *widget,
    void *ev, void *win, void *state)
{
  ModuleActionHandlerV1 *handler;

  g_debug("module: checking action `%s`",g_quark_to_string(quark));

  handler = module_action_get(quark);
  if(!handler)
    return;
  g_debug("module: calling action `%s`",g_quark_to_string(quark));

  handler->function(param, addr, widget,ev,win,state);
}

gboolean module_is_function ( gchar *identifier )
{
  if(expr_handlers &&
      g_hash_table_lookup(expr_handlers,identifier))
    return TRUE;
  return FALSE;
}

gboolean module_check_flag ( gchar *identifier, gint flag )
{
  ModuleExpressionHandlerV1 *handler;

  if(!expr_handlers)
    return FALSE;

  handler = g_hash_table_lookup(expr_handlers,identifier);
  if(!handler)
    return FALSE;
  return !!(handler->flags & flag);
}

gchar *module_get_string ( GScanner *scanner )
{
  ModuleExpressionHandlerV1 *handler;
  void **params;
  ExprCache *expr;
  void *widget;
  gchar *result;
  gint i;

  E_STATE(scanner)->type = EXPR_VARIANT;
  if(!expr_handlers)
    return g_strdup("");

  handler = g_hash_table_lookup(expr_handlers,scanner->value.v_identifier);
  if(!handler)
    return g_strdup("");

  g_debug("module: calling function `%s`",handler->name);
  params = expr_module_parameters(scanner,handler->parameters,handler->name);
  for(expr=E_STATE(scanner)->expr;!expr->widget&&expr->parent;expr=expr->parent);
  widget = expr->widget;

  result = handler->function(params, widget);

  if(params)
    for(i=0;i<strlen(handler->parameters);i++)
      g_free(params[i]);
  g_free(params);

  if(handler->flags & MODULE_EXPR_NUMERIC)
    E_STATE(scanner)->type = EXPR_NUMERIC;
  else
    E_STATE(scanner)->type = EXPR_STRING;
  if(!(handler->flags & MODULE_EXPR_DETERMINISTIC))
    E_STATE(scanner)->expr->vstate = TRUE;

  return result;
}
