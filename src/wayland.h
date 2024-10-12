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
void foreign_toplevel_init ( void );
void cw_init ( void );

#endif
