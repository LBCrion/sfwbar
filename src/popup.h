#ifndef __POPUP_H__
#define __POPUP_H__

GtkWidget *popup_new ( gchar *name );
GtkWidget *popup_from_name ( gchar *name );
void popup_trigger ( GtkWidget *parent, gchar *name, GdkEvent *ev );
void popup_show ( GtkWidget *parent, GtkWidget *popup, GdkSeat *seat );
void popup_get_gravity ( GtkWidget *widget, GdkGravity *, GdkGravity * );
void popup_set_autoclose ( GtkWidget *win, gboolean autoclose );
void popup_popdown_autoclose ( void );

#endif
