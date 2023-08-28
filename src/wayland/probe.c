/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "wlr-layer-shell-unstable-v1.h"

static GdkMonitor *default_monitor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;

void shm_register (struct wl_registry *registry, uint32_t name )
{
  shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
}

void layer_shell_register (struct wl_registry *registry, uint32_t name,
    uint32_t version)
{
  layer_shell = wl_registry_bind(registry, name,
      &zwlr_layer_shell_v1_interface,version);
}

GdkMonitor *wayland_monitor_get_default ( void )
{
  GdkDisplay *gdisp = gdk_display_get_default();
  gint n, i;

  n = gdk_display_get_n_monitors(gdisp);
  for(i=0; i<n; i++)
    if(gdk_display_get_monitor(gdisp, i) == default_monitor)
    return default_monitor;

  return gdk_display_get_monitor(gdisp, 0);
}

static void noop() {
}

static void surface_enter ( void *data,
    struct wl_surface *surface,
    struct wl_output *output )
{
  GdkDisplay *gdisp = gdk_display_get_default();
  GdkMonitor *gmon = NULL;
  gint n,i;

  n = gdk_display_get_n_monitors(gdisp);
  for(i=0;i<n;i++)
  {
    gmon = gdk_display_get_monitor(gdisp,i);
    if(gdk_wayland_monitor_get_wl_output(gmon)==output)
      default_monitor = gmon;
  }
}

struct wl_surface_listener surface_listener = {
  .enter = surface_enter,
  .leave = noop
};

void layer_surface_configure ( void *data,
    struct zwlr_layer_surface_v1 * surface,
    uint32_t s, uint32_t w, uint32_t h)

{
  zwlr_layer_surface_v1_ack_configure(surface,s);
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
  .configure = layer_surface_configure,
  .closed = noop
};

void wayland_monitor_probe ( void )
{
  struct zwlr_layer_surface_v1 *layer_surface;
  struct wl_compositor *compositor;
  struct wl_display *display;
  struct wl_surface *surface;
  struct wl_buffer *buffer;
  struct wl_shm_pool *pool;
  void *shm_data = NULL;
  gchar *name;
  gint fd;
  gint retries = 100, size = 4;

  display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
  compositor = gdk_wayland_display_get_wl_compositor(gdk_display_get_default());
  if(!display || !compositor || !shm || !layer_shell)
    return;

  do
  {
    name = g_strdup_printf("/sfwbar-probe-%ld", g_get_monotonic_time());
    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0)
      shm_unlink(name);
    g_free(name);
  } while (--retries > 0 && errno == EEXIST && fd < 0 );

  if (fd < 0)
    return;

  if (ftruncate(fd, size) < 0)
  {
    close(fd);
    return;
  }

  shm_data = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_data == MAP_FAILED)
  {
    close(fd);
    return;
  }

  pool = wl_shm_create_pool(shm, fd, size);
  buffer = wl_shm_pool_create_buffer(pool, 0, 1, 1, 4, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);

  memset(shm_data, 0, size);

  surface = wl_compositor_create_surface(compositor);
  wl_surface_add_listener(surface,&surface_listener,NULL);
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell,
      surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "sfwbar-test");
  zwlr_layer_surface_v1_set_anchor(layer_surface,
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
  zwlr_layer_surface_v1_set_size(layer_surface,1,1);
  zwlr_layer_surface_v1_add_listener(layer_surface,
      &layer_surface_listener,NULL);

  wl_surface_commit(surface);
  wl_display_roundtrip(display);

  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_commit(surface);
  wl_display_roundtrip(display);

  zwlr_layer_surface_v1_destroy(layer_surface);
  wl_surface_destroy(surface);
  wl_buffer_destroy(buffer);
  munmap(shm_data,size);
  close(fd);
  zwlr_layer_shell_v1_destroy(layer_shell);
  wl_shm_destroy(shm);
}
