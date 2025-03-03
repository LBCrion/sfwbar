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

void action_trigger_cb ( vm_closure_t *closure )
{
  if(closure)
    vm_run_action(closure->code, NULL, NULL, NULL, NULL, closure->store);
}

void action_trigger_add ( gchar *trigger, vm_closure_t *closure )
{
  trigger_add(trigger, (GSourceFunc)action_trigger_cb, closure );
}
