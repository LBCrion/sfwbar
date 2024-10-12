#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include <gtk/gtk.h>

GdkMonitor *widget_get_monitor ( GtkWidget *self );
void xdg_output_init ( void );
void xdg_output_new ( GdkMonitor *gmon );
void xdg_output_destroy ( GdkMonitor *gmon );
gboolean xdg_output_check ( void );
GdkMonitor *monitor_default_get ( void );
gchar *monitor_get_name ( GdkMonitor *monitor );
void monitor_init ( void );

#endif
