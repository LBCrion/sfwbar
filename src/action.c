/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "action.h"
#include "trigger.h"
#include "gui/taskbaritem.h"
#include "util/string.h"
#include "vm/vm.h"

static GHashTable *functions;

void action_function_add ( gchar *name, action_t *action )
{
  if(!functions)
    functions = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, NULL);

  g_hash_table_insert(functions, g_strdup(name),
        g_list_append(g_hash_table_lookup(functions, name), action));
}

void action_function_exec ( gchar *name, GtkWidget *w, GdkEvent *ev,
    window_t *win, guint16 *state )
{
  GList *iter;
  window_t *stat_win;

  if(!name || !functions)
    return;

  if(win)
    stat_win = g_memdup2(win, sizeof(window_t));
  else
    stat_win = NULL;

  for(iter=g_hash_table_lookup(functions, name); iter; iter=g_list_next(iter))
    action_exec(w, iter->data, ev, stat_win, state);

  g_free(stat_win);
}

guint16 action_state_build ( GtkWidget *widget, window_t *win )
{
  guint16 state = 0;

  if(win)
  {
    state = win->state;
    if(wintree_is_focused(win->uid))
      state |= WS_FOCUSED;
  }
  if(widget && IS_BASE_WIDGET(widget))
      state |= base_widget_get_state(widget);
  return state;
}

void action_exec ( GtkWidget *widget, action_t *action,
    GdkEvent *event, window_t *win, guint16 *istate )
{
/*  ModuleActionHandlerV1 *ahandler;
  expr_cache_t *addr;
  GList *children, *iter;
  guint16 state;
  action_t *caction;*/

  if(action && action->code)
    vm_run_action(action->code, widget, event);
//  vm_run_action((gchar *)action->id, action->addr->definition,
//      action->command->definition, widget, event, action->cond, action->ncond);

/*  if( !(ahandler = module_action_get(action->id)) )
    return;

  addr = (ahandler->flags & MODULE_ACT_ADDRESS_ONLY)?
    action->command:action->addr;

  addr->widget = widget;
  addr->event = event;
  expr_cache_eval(addr);
  if(addr->cache && ahandler->flags & MODULE_ACT_WIDGET_ADDRESS )
  {
    widget = base_widget_from_id(addr->cache);
    if(IS_TASKBAR_ITEM(widget))
      win = flow_item_get_source(widget);
    event = NULL;
    istate = NULL;
  }

  state = istate? *istate : action_state_build ( widget, win );

  if(action->cond & WS_CHILDREN)
    state |= WS_CHILDREN;

  if(((action->cond & 0x0f) || (action->ncond & 0x0f)) && !win)
      return;
  if(((action->cond & 0xf0) || (action->ncond & 0xf0)) && !widget )
      return;
  if((state & action->cond) != action->cond)
    return;
  if((~state & action->ncond) != action->ncond)
    return;

  if( !(ahandler->flags & MODULE_ACT_CMD_BY_DEF) &&
   !(ahandler->flags & MODULE_ACT_ADDRESS_ONLY) )
  {
    action->command->widget = widget;
    action->command->event = event;
    expr_cache_eval(action->command);
    action->command->widget = NULL;
    action->command->event = NULL;
  }

  g_debug("action: %s '%s', '%s', widget=%p, win=%d from '%s', '%s'",
      ahandler->name, action->addr->cache, action->command->cache,
      (void *)widget, win?GPOINTER_TO_INT(win->uid):0,
      action->addr->definition, action->command->definition);

  if(action->cond & WS_CHILDREN &&
      GTK_IS_CONTAINER(base_widget_get_child(widget)))
  {
    caction = action_dup(action);
    caction->cond = 0;
    caction->ncond = 0;
    children = gtk_container_get_children(
        GTK_CONTAINER(base_widget_get_child(widget)));
    for(iter=children; iter; iter=g_list_next(iter))
      action_exec(iter->data, caction, event,
          IS_TASKBAR_ITEM(iter->data)?flow_item_get_source(iter->data):win,
          NULL);
    g_list_free(children);
    action_free(caction, NULL);
    return;
  }

  module_action_exec(action->id,
      (ahandler->flags & MODULE_ACT_CMD_BY_DEF)?action->command->definition:
      action->command->cache, action->addr->cache, widget, event, win, &state);*/
}

action_t *action_new ( void )
{
  action_t *new;

  new = g_malloc0(sizeof(action_t));
//  new->command = expr_cache_new();
//  new->addr = expr_cache_new();

  return new;
}

/*action_t *action_dup ( action_t *src )
{
  action_t *dest;

  if(!src)
    return NULL;

  dest = action_new();
  dest->cond = src->cond;
  dest->ncond = src->ncond;
  dest->id = src->id;

  dest->command->definition = g_strdup(src->command->definition);
  dest->command->cache = g_strdup(src->command->cache);
  dest->command->eval = src->command->eval;
  dest->addr->definition = g_strdup(src->addr->definition);
  dest->addr->cache = g_strdup(src->addr->cache);
  dest->addr->eval = src->addr->eval;
  return dest;
}*/

void action_free ( action_t *action, GObject *old )
{
  if(!action)
    return;

//  expr_cache_free(action->command);
//  expr_cache_free(action->addr);
  g_bytes_unref(action->code);

  g_free(action);
}

void action_trigger_cb ( action_t *action )
{
  action_exec(NULL, action, NULL, NULL, NULL);
}

void action_trigger_add ( action_t *action, gchar *trigger )
{
  trigger_add(trigger, (GSourceFunc)action_trigger_cb, action);
}
