#ifndef __MENU_H__
#define __MENU_H_

#include <gtk/gtk.h>
#include "action.h"

GtkWidget *menu_from_name ( gchar *name );
GtkWidget *menu_new ( gchar *name );
void menu_remove ( gchar *name );
void menu_popup ( GtkWidget *, GtkWidget *, GdkEvent *, gpointer, guint16 * );
GtkWidget *menu_item_new ( gchar *label, action_t *action );
void menu_popup_get_gravity ( GtkWidget *widget, GdkGravity *wanchor,
    GdkGravity *manchor );

#endif
