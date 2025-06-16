#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

GdkMonitor *monitor_from_widget ( GtkWidget *self );
gboolean xdg_output_check ( void );
GdkMonitor *monitor_default_get ( void );
gchar *monitor_get_name ( GdkMonitor *monitor );
GdkMonitor *monitor_from_name ( gchar *name );
GdkMonitor *monitor_from_wl_output ( struct wl_output *output );
void monitor_init ( gchar * );

#endif
