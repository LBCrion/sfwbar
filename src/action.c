/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "action.h"
#include "trigger.h"
#include "gui/taskbaritem.h"
#include "util/string.h"
#include "vm/vm.h"

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

void action_exec ( GtkWidget *widget, GBytes *action,
    GdkEvent *event, window_t *win, guint16 *istate )
{
/*  ModuleActionHandlerV1 *ahandler;
  expr_cache_t *addr;
  GList *children, *iter;
  guint16 state;
  action_t *caction;*/

  if(action)
    vm_run_action(action, widget, event, win, istate);
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

void action_trigger_cb ( GBytes *action )
{
  action_exec(NULL, action, NULL, NULL, NULL);
}

void action_trigger_add ( GBytes *action, gchar *trigger )
{
  trigger_add(trigger, (GSourceFunc)action_trigger_cb, action);
}
