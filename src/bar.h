#ifndef __BAR_H__
#define __BAR_H__

GtkWindow *bar_new ( gchar * );
void bar_set_monitor ( gchar *, GtkWidget * );
void bar_set_layer ( gchar *, gchar *);
void bar_set_size ( gchar *, gchar * );
void bar_set_exclusive_zone ( gchar *, gchar * );
gchar *bar_get_output ( GtkWidget * );
gint bar_get_toplevel_dir ( GtkWidget * );
gboolean bar_hide_event ( const gchar *mode );
void bar_monitor_added_cb ( GdkDisplay *, GdkMonitor * );
void bar_monitor_removed_cb ( GdkDisplay *, GdkMonitor * );
void bar_update_monitor ( GtkWindow *win );
GtkWidget *bar_get_by_name ( gchar *name );
GtkWidget *bar_grid_by_name ( gchar *addr );

#endif
