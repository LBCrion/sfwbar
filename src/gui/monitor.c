/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "wayland.h"
#include "gui/bar.h"
#include "trigger.h"
#include "wlr-layer-shell-unstable-v1.h"
#include "xdg-output-unstable-v1.h"

static GdkMonitor *default_monitor;
static struct zxdg_output_manager_v1 *xdg_output_manager;

GdkMonitor *monitor_default_get ( void )
{
  GdkDisplay *gdisp = gdk_display_get_default();
  gint n, i;

  n = gdk_display_get_n_monitors(gdisp);
  for(i=0; i<n; i++)
    if(gdk_display_get_monitor(gdisp, i) == default_monitor)
      return default_monitor;

  return gdk_display_get_monitor(gdisp, 0);
}

static void monitor_noop() {
}

static void monitor_surface_enter ( void *data,
    struct wl_surface *surface,
    struct wl_output *output )
{
  GdkDisplay *gdisp = gdk_display_get_default();
  GdkMonitor *gmon = NULL;
  gint n, i;

  n = gdk_display_get_n_monitors(gdisp);
  for(i=0; i<n; i++)
  {
    gmon = gdk_display_get_monitor(gdisp, i);
    if(gdk_wayland_monitor_get_wl_output(gmon)==output)
      default_monitor = gmon;
  }
}

struct wl_surface_listener monitor_surface_listener = {
  .enter = monitor_surface_enter,
  .leave = (void (*)(void *, struct wl_surface *, struct wl_output *))monitor_noop
};

void monitor_layer_surface_configure ( void *data,
    struct zwlr_layer_surface_v1 * surface,
    uint32_t s, uint32_t w, uint32_t h)

{
  zwlr_layer_surface_v1_ack_configure(surface,s);
}

struct zwlr_layer_surface_v1_listener monitor_layer_surface_listener = {
  .configure = monitor_layer_surface_configure,
  .closed = (void (*)(void *, struct zwlr_layer_surface_v1 *))monitor_noop
};

void monitor_default_probe ( void )
{
  struct wl_shm *shm;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct zwlr_layer_surface_v1 *layer_surface;
  struct wl_compositor *compositor;
  struct wl_display *display;
  struct wl_surface *surface;
  struct wl_buffer *buffer;
  struct wl_shm_pool *pool;
  void *shm_data = NULL;
  gchar *name;
  gint fd, retries = 100, size = 4;

  display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
  compositor = gdk_wayland_display_get_wl_compositor(gdk_display_get_default());
  shm = wayland_iface_register(wl_shm_interface.name, 1, 1, &wl_shm_interface);
  if(!display || !compositor || !shm)
    return;

  if( !(layer_shell = wayland_iface_register(
          zwlr_layer_shell_v1_interface.name, 3, 3,
          &zwlr_layer_shell_v1_interface)) )
  {
    wl_shm_destroy(shm);
    return;
  }

  do
  {
    name = g_strdup_printf("/sfwbar-probe-%lld",
      (long long int)g_get_monotonic_time());
    if( (fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600))>=0 )
      shm_unlink(name);
    g_free(name);
  } while (--retries > 0 && errno == EEXIST && fd < 0 );

  if((fd <= 0) || (ftruncate(fd, size) < 0) ||
    (shm_data = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
    MAP_FAILED)
  {
    if(fd>0)
      close(fd);
    wl_shm_destroy(shm);
    zwlr_layer_shell_v1_destroy(layer_shell);
    return;
  }

  pool = wl_shm_create_pool(shm, fd, size);
  buffer = wl_shm_pool_create_buffer(pool, 0, 1, 1, 4, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);

  memset(shm_data, 0, size);

  surface = wl_compositor_create_surface(compositor);
  wl_surface_add_listener(surface, &monitor_surface_listener, NULL);
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell,
      surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "sfwbar-test");
  zwlr_layer_surface_v1_set_anchor(layer_surface,
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
  zwlr_layer_surface_v1_set_size(layer_surface, 1, 1);
  zwlr_layer_surface_v1_add_listener(layer_surface,
      &monitor_layer_surface_listener, NULL);

  wl_surface_commit(surface);
  wl_display_roundtrip(display);

  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_commit(surface);
  wl_display_roundtrip(display);

  zwlr_layer_surface_v1_destroy(layer_surface);
  wl_surface_destroy(surface);
  wl_buffer_destroy(buffer);
  munmap(shm_data, size);
  close(fd);
  zwlr_layer_shell_v1_destroy(layer_shell);
  wl_shm_destroy(shm);
}

static void monitor_handle_name ( void *monitor,
    struct zxdg_output_v1 *xdg_output, const gchar *name )
{
  g_object_set_data_full(G_OBJECT(monitor), "xdg_name", g_strdup(name),
      g_free);
}

static void monitor_handle_done ( void *monitor,
    struct zxdg_output_v1 *xdg_output )
{
  zxdg_output_v1_destroy(xdg_output);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
  .logical_position = (void (*)(void *, struct zxdg_output_v1 *, int32_t,  int32_t))monitor_noop,
  .logical_size = (void (*)(void *, struct zxdg_output_v1 *, int32_t,  int32_t))monitor_noop,
  .done = monitor_handle_done,
  .name = monitor_handle_name,
  .description = (void (*)(void *, struct zxdg_output_v1 *, const char *))monitor_noop,
};

static void xdg_output_new ( GdkMonitor *monitor )
{
  struct wl_output *output;
  struct zxdg_output_v1 *xdg;

  if(!monitor || !xdg_output_manager)
    return;

  if( !(output = gdk_wayland_monitor_get_wl_output(monitor)) )
    return;

  xdg = zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, output);

  if(xdg)
    zxdg_output_v1_add_listener(xdg, &xdg_output_listener, monitor);
}

gchar *monitor_get_name ( GdkMonitor *monitor )
{
  return monitor?g_object_get_data(G_OBJECT(monitor), "xdg_name"):NULL;
}

gboolean xdg_output_check ( void )
{
  GdkDisplay *gdisp;
  gint i;

  if(!xdg_output_manager)
    return TRUE;

  gdisp = gdk_display_get_default();

  for(i=0; i<gdk_display_get_n_monitors(gdisp); i++)
    if(!monitor_get_name(gdk_display_get_monitor(gdisp, i)))
      return FALSE;

  return TRUE;
}

GdkMonitor *monitor_from_widget ( GtkWidget *self )
{
  GtkWidget *parent, *w;
  GdkWindow *win;
  GdkDisplay *disp;

  g_return_val_if_fail(GTK_IS_WIDGET(self),NULL);

  if(gtk_widget_get_mapped(self))
    win = gtk_widget_get_window(self);
  else
  {
    for(w=self; w; w=gtk_widget_get_parent(w))
      if( (parent=g_object_get_data(G_OBJECT(w), "parent_window")) )
        break;
    if(!w)
      return NULL;
    win = gtk_widget_get_window(parent);
  }

  if(!win || !(disp = gdk_window_get_display(win)) )
      return NULL;
  return gdk_display_get_monitor_at_window(disp, win);
}

static void monitor_added_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  GHashTable *bar_list;
  GHashTableIter iter;
  void *key, *bar;
  char trigger[256];

  xdg_output_new(gmon);
  if((bar_list = bar_get_list()))
  {
    g_hash_table_iter_init(&iter, bar_list);
    while(g_hash_table_iter_next(&iter, &key, &bar))
      g_idle_add((GSourceFunc)bar_update_monitor, bar);
  }

  g_snprintf(trigger, 255, "%s_connected", monitor_get_name(gmon));
  trigger_emit(trigger);
}

static void monitor_removed_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  static char trigger[256];

  g_snprintf(trigger, 255, "%s_disconnected", monitor_get_name(gmon));
  trigger_emit(trigger);
}

static void monitor_list_print ( void )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon;
  gint nmon, i;

  gdisp = gdk_display_get_default();
  nmon = gdk_display_get_n_monitors(gdisp);
  for(i=0; i<nmon; i++)
  {
    gmon = gdk_display_get_monitor(gdisp,i);
    g_message("%s: %s %s",monitor_get_name(gmon),
        gdk_monitor_get_manufacturer(gmon), gdk_monitor_get_model(gmon));
  }
  exit(0);
}

void monitor_init ( gchar *monitor )
{
  GdkDisplay *display;
  gint i, n;

  if( monitor && !g_ascii_strcasecmp(monitor, "list") )
    monitor_list_print();

  display = gdk_display_get_default();
  g_signal_connect(display, "monitor-added",
      G_CALLBACK(monitor_added_cb), NULL);
  g_signal_connect(display, "monitor-removed",
      G_CALLBACK(monitor_removed_cb), NULL);

  if( !(xdg_output_manager = wayland_iface_register(
      zxdg_output_manager_v1_interface.name,
      ZXDG_OUTPUT_V1_NAME_SINCE_VERSION,
      ZXDG_OUTPUT_V1_NAME_SINCE_VERSION,
      &zxdg_output_manager_v1_interface)) )
  {
    g_warning("Unable to registry xdg-output protocol version %u",
      ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);
    return;
  }

  n = gdk_display_get_n_monitors(display);
  for(i=0; i<n; i++)
    xdg_output_new(gdk_display_get_monitor(display, i));
  wl_display_roundtrip(gdk_wayland_display_get_wl_display(display));

  monitor_default_probe();
  g_debug("default output: %s", monitor_get_name(monitor_default_get()));

}
