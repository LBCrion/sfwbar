#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include <gtk/gtk.h>

GdkMonitor *monitor_from_widget ( GtkWidget *self );
gboolean xdg_output_check ( void );
GdkMonitor *monitor_default_get ( void );
gchar *monitor_get_name ( GdkMonitor *monitor );
void monitor_init ( gchar * );

#endif
