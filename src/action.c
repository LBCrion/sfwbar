/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "action.h"
#include "trigger.h"
#include "gui/basewidget.h"
#include "vm/vm.h"


void action_trigger_cb ( vm_closure_t *closure )
{
  vm_run_action(closure->code, NULL, NULL, NULL, NULL, closure->store);
}
