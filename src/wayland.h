#ifndef __WAYLAND_H__
#define __WAYLAND_H__

void wayland_init ( void );
void wayland_ipc_init ( void );
void wayland_set_idle_inhibitor ( GtkWidget *widget, gboolean inhibit );
void xdg_output_new ( GdkMonitor *gmon );
void xdg_output_destroy ( GdkMonitor *gmon );
void foreign_toplevel_activate ( gpointer tl );
gboolean xdg_output_check ( void );
gboolean foreign_toplevel_is_active ( void );

#endif
