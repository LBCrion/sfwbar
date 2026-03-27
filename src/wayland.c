/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include <sys/mman.h>
#include <fcntl.h>
#include <gdk/gdkwayland.h>
#include "wayland.h"
#include "gui/monitor.h"
#include "xdg-output-unstable-v1.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "wlr-layer-shell-unstable-v1.h"

static GList *wayland_ifaces;
static struct wl_registry *wayland_registry;
static gboolean wayland_init_complete;
struct wl_shm *shm;

wayland_buffer_t *wayland_buffer_new ( guint32 w, guint32 h, guint32 f )
{
  wayland_buffer_t *buff;
  struct wl_shm_pool *pool;
  void *shm_data = NULL;
  gchar *name;
  gint fd, retries = 100, stride, size;

  if(!shm || f != WL_SHM_FORMAT_ARGB8888 || w==0 || h==0)
    return NULL;

  do
  {
    name = g_strdup_printf("/sfwbar-%ld", (long int)g_get_monotonic_time());
    if( (fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600))>=0 )
      shm_unlink(name);
    g_free(name);
  } while (--retries > 0 && errno == EEXIST && fd < 0 );

  if(fd<0)
    return NULL;

  stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w);
  size = stride * h;

  if( (ftruncate(fd, size) >= 0) &&
      (shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))
      != MAP_FAILED)
  {
    buff = g_malloc0(sizeof(wayland_buffer_t));
    pool = wl_shm_create_pool(shm, fd, size);
    buff->buffer = wl_shm_pool_create_buffer(pool, 0, w, h, stride, f);
    buff->data = shm_data;
    buff->size = size;
    wl_shm_pool_destroy(pool);
  }
  else
    buff = NULL;

  close(fd);

  return buff;
}

void wayland_buffer_free ( wayland_buffer_t *buff )
{
  wl_buffer_destroy(buff->buffer);
  munmap(buff->data, buff->size);
  g_free(buff);
}

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  wayland_iface_t *iface;

  if(wayland_init_complete)
    return;

  iface = g_malloc0(sizeof(wayland_iface_t));
  iface->iface = g_strdup(interface);
  iface->global = name;
  iface->version = version;
  wayland_ifaces = g_list_append(wayland_ifaces, iface);
}

gpointer wayland_iface_register ( const gchar *interface,
    guint32 min_ver, guint32 max_ver, const void *impl )
{
  GList *iter;
  wayland_iface_t *iface;

  for(iter=wayland_ifaces; iter; iter=g_list_next(iter))
  {
    iface = iter->data;
    if(iface->version >= min_ver && !g_strcmp0(iface->iface, interface))
      return wl_registry_bind(wayland_registry, iface->global, impl,
          MIN(iface->version, max_ver));
  }
  return NULL;
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
  .global_remove = handle_global_remove
};

void wayland_init ( void )
{
  struct wl_display *wdisp;

  if(!(wdisp = gdk_wayland_display_get_wl_display(gdk_display_get_default())) )
    g_error("Can't get wayland display\n");

  wayland_registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(wayland_registry, &registry_listener, NULL);
  wl_display_roundtrip(wdisp);
  wayland_init_complete = TRUE;

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);

  shm = wayland_iface_register(wl_shm_interface.name, 1, 1, &wl_shm_interface);
}
