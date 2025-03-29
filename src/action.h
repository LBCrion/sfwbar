#ifndef __ACTION_H__
#define __ACTION_H__

#include "wintree.h"
#include "vm/vm.h"

void action_trigger_cb ( vm_closure_t *closure );
void action_lib_init ( void );
guint16 action_state_build ( GtkWidget *widget, window_t *win );

#endif
