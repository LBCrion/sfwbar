#include "xdg-output-unstable-v1.h"
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

static struct zxdg_output_manager_v1 *xdg_output_manager;

static void noop ()
{
}

static void xdg_output_handle_name ( void *data,
    struct zxdg_output_v1 *xdg_output, const gchar *name )
{
  GdkMonitor *monitor = data;

  g_free(g_object_get_data(G_OBJECT(monitor),"xdg_name"));
  g_object_set_data(G_OBJECT(monitor),"xdg_name",strdup(name));
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
  .logical_position = noop,
  .logical_size = noop,
  .done = noop,
  .name = xdg_output_handle_name,
  .description = noop,
};

void wayland_output_new ( GdkMonitor *gmon )
{
  struct wl_output *output;
  struct zxdg_output_v1 *xdg;

  if(!gmon || !xdg_output_manager)
    return;

  output = gdk_wayland_monitor_get_wl_output(gmon);

  if(!output)
    return;

  xdg = zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, output);

  if(xdg)
  {
    zxdg_output_v1_add_listener(xdg,&xdg_output_listener,gmon);
    g_object_set_data(G_OBJECT(gmon),"xdg_output",xdg);
  }
}

void wayland_output_destroy ( GdkMonitor *gmon )
{
  struct zxdg_output_v1 *xdg;

  if(!gmon || !xdg_output_manager)
    return;

  xdg = g_object_get_data(G_OBJECT(gmon),"xdg_output");

  if(xdg)
    zxdg_output_v1_destroy(xdg);
}

gboolean wayland_xdg_output_register ( void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  gint nmon,i;
  GdkDisplay *gdisp;
  struct wl_display *wdisp;

  if (g_strcmp0(interface,zxdg_output_manager_v1_interface.name))
    return FALSE;

  xdg_output_manager = wl_registry_bind(registry, name,
      &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);

  gdisp = gdk_display_get_default();
  wdisp = gdk_wayland_display_get_wl_display(gdisp);
  nmon = gdk_display_get_n_monitors(gdisp);
  for(i=0;i<nmon;i++)
    wayland_output_new(gdk_display_get_monitor(gdisp,i));
  wl_display_roundtrip(wdisp);
