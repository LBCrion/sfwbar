#ifndef __WAYLAND_H__
#define __WAYLAND_H__

#include <gtk/gtk.h>

typedef struct _wayland_iface {
  gchar *iface;
  guint32 global;
  guint32 version;
} wayland_iface_t;

void wayland_init ( void );
gpointer wayland_iface_register ( const gchar *interface,
    guint32 min_ver, guint32 max_ver, const void *impl );
void xdg_output_init ( void );
void wayland_monitor_probe ( void );
void foreign_toplevel_init ( void );
void cw_init ( void );
void xdg_output_new ( GdkMonitor *gmon );
void xdg_output_destroy ( GdkMonitor *gmon );
gboolean xdg_output_check ( void );
GdkMonitor *wayland_monitor_get_default ( void );

#endif
