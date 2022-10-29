#ifndef __BAR_H__
#define __BAR_H__

GtkWindow *bar_new ( gchar * );
void bar_set_monitor ( gchar *, GtkWindow * );
void bar_set_layer ( gchar *, GtkWindow *);
void bar_set_size ( gchar *, GtkWindow * );
void bar_set_exclusive_zone ( gchar *, GtkWindow * );
gchar *bar_get_output ( GtkWidget * );
gint bar_get_toplevel_dir ( GtkWidget * );
gboolean bar_hide_event ( const gchar *mode );
void bar_monitor_added_cb ( GdkDisplay *, GdkMonitor * );
void bar_monitor_removed_cb ( GdkDisplay *, GdkMonitor * );
void bar_update_monitor ( GtkWindow *win );
GtkWindow *bar_from_name ( gchar *name );
GtkWidget *bar_grid_from_name ( gchar *addr );

#endif
