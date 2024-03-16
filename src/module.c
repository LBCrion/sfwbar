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

static GList *module_list;
static GHashTable *expr_handlers, *interfaces;
static GData *act_handlers;
static GList *invalidators;

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
      ahandler[i]->quark = g_quark_from_string(lname);
      g_debug("module: register action '%s'",ahandler[i]->name);
      if(g_datalist_id_get_data(&act_handlers, ahandler[i]->quark))
        g_message("Duplicate module action: %s in module %s",
            ahandler[i]->name, name);
      else
        g_datalist_id_set_data(&act_handlers, ahandler[i]->quark, ahandler[i]);
      g_free(lname);
    }
}

void module_interface_select ( gchar *interface )
{
  ModuleInterfaceList *list;
  ModuleInterfaceV1 *new;
  GList *iter;
  gint i;

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

  if(list->active)
  {
    for(i=0; list->active->expr_handlers[i]; i++)
    {
      g_hash_table_remove(expr_handlers, list->active->expr_handlers[i]->name);
      expr_dep_trigger(list->active->expr_handlers[i]->name);
    }
    for(i=0; list->active->act_handlers[i]; i++)
      g_datalist_id_remove_data(&act_handlers,
          list->active->act_handlers[i]->quark);
  }
  list->active = new;
  if(new)
  {
    module_actions_add(new->act_handlers, new->provider);
    module_expr_funcs_add(new->expr_handlers, new->provider);
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

gboolean module_load ( gchar *name )
{
  GModule *module;
  ModuleExpressionHandlerV1 **ehandler;
  ModuleActionHandlerV1 **ahandler;
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
      !ver || *ver != 2 )
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

  if(g_module_symbol(module,"sfwbar_expression_handlers",(void **)&ehandler))
    module_expr_funcs_add(ehandler, name);

  if(g_module_symbol(module,"sfwbar_action_handlers",(void **)&ahandler))
    module_actions_add(ahandler, name);

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

void *module_get_value ( GScanner *scanner )
{
  ModuleExpressionHandlerV1 *handler;
  void **params;
  ExprCache *expr;
  gchar *result;
  gint i;

  E_STATE(scanner)->type = EXPR_VARIANT;
  if(!expr_handlers)
    return NULL;

  if(!(handler =
        g_hash_table_lookup(expr_handlers, scanner->value.v_identifier)))
    return NULL;

  g_debug("module: calling function `%s`", handler->name);
  params = expr_module_parameters(scanner, handler->parameters, handler->name);

  expr=E_STATE(scanner)->expr;
  while(!expr->widget && expr->parent)
    expr=expr->parent;

  result = handler->function(params, expr->widget, expr->event);

  if(params)
    for(i=0; i<strlen(handler->parameters); i++)
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

void module_queue_append ( module_queue_t *queue, void *item )
{
  gboolean trigger;
  GList *ptr;

  g_mutex_lock(&(queue->mutex));

  ptr = g_list_find_custom(queue->list, item, queue->compare);
  if(ptr && ptr != queue->list)
  {
    queue->free(ptr->data);
    ptr->data = queue->duplicate(item);
  }
  else
    queue->list = g_list_append(queue->list, queue->duplicate(item));

  trigger = !g_list_next(queue->list);
  g_mutex_unlock(&(queue->mutex));

  if(trigger && queue->trigger)
    g_idle_add((GSourceFunc)base_widget_emit_trigger,
        (gpointer)queue->trigger);
}

void module_queue_remove ( module_queue_t *queue )
{
  gboolean trigger;
  void *item;

  g_mutex_lock(&(queue->mutex));
  if(queue->list)
  {
    item = queue->list->data;
    queue->list = g_list_remove(queue->list, item);
    trigger = !!queue->list;
    queue->free(item);
  }
  else
    trigger = FALSE;
  g_mutex_unlock(&(queue->mutex));

  if(trigger && queue->trigger)
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)queue->trigger);
}

void *module_queue_get_string ( module_queue_t *queue, gchar *param )
{
  void *result;

  g_mutex_lock(&(queue->mutex));
  if(queue->list)
    result = queue->get_str(queue->list->data, param);
  else
    result = NULL;
  g_mutex_unlock(&(queue->mutex));

  return result;
}

void *module_queue_get_numeric ( module_queue_t *queue, gchar *param )
{
  void *result;

  g_mutex_lock(&(queue->mutex));
  if(queue->list)
    result = queue->get_num(queue->list->data, param);
  else
    result = NULL;
  g_mutex_unlock(&(queue->mutex));

  return result;
}
