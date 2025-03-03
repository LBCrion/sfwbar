#ifndef __ACTION_H__
#define __ACTION_H__

#include "wintree.h"
#include "vm/vm.h"

void action_function_add ( gchar *, GBytes *);
void action_function_exec ( gchar *, GtkWidget *, GdkEvent *, window_t *,
    guint16 *);
void action_trigger_add ( gchar *trigger, vm_closure_t *closure );
void action_lib_init ( void );
guint16 action_state_build ( GtkWidget *widget, window_t *win );

#endif
