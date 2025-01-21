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
  if(action)
    vm_run_action(action, widget, event, win, istate);
}

void action_trigger_cb ( GBytes *action )
{
  action_exec(NULL, action, NULL, NULL, NULL);
}

void action_trigger_add ( GBytes *action, gchar *trigger )
{
  trigger_add(trigger, (GSourceFunc)action_trigger_cb, action);
}
