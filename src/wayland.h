#ifndef __WAYLAND_H__
#define __WAYLAND_H__

#include <gtk/gtk.h>

typedef struct _wayland_iface {
  gchar *iface;
  guint32 global;
  guint32 version;
} wayland_iface_t;

typedef struct _wayland_buffer {
  struct wl_buffer *buffer;
  gpointer data;
  guint32 size;
} wayland_buffer_t;

void wayland_init ( void );
gpointer wayland_iface_register ( const gchar *interface,
    guint32 min_ver, guint32 max_ver, const void *impl );
void foreign_toplevel_init ( void );
void cw_init ( void );
void ew_init ( void );
wayland_buffer_t *wayland_buffer_new ( guint32 w, guint32 h, guint32 f );
void wayland_buffer_free ( wayland_buffer_t *buff );

void ext_ftl_init ( void );
gpointer ext_ftl_lookup ( gchar *id );

#endif
