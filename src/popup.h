#ifndef __POPUP_H__
#define __POPUP_H__

GtkWidget *popup_new ( gchar *name );
GtkWidget *popup_from_name ( gchar *name );
void popup_trigger ( GtkWidget *parent, gchar *name );
void popup_show ( GtkWidget *parent, GtkWidget *popup );
void popup_get_gravity ( GtkWidget *widget, GdkGravity *, GdkGravity * );

#endif
