#ifndef __ACTION_H__
#define __ACTION_H__

#include "wintree.h"

void action_exec ( GtkWidget *, GBytes *, GdkEvent *, window_t *, guint16 *);
void action_function_add ( gchar *, GBytes *);
void action_function_exec ( gchar *, GtkWidget *, GdkEvent *, window_t *,
    guint16 *);
void action_trigger_add ( GBytes *action, gchar *trigger );
void action_lib_init ( void );
guint16 action_state_build ( GtkWidget *widget, window_t *win );

#endif
