/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <glib.h>
#include <gdk/gdkwayland.h>
#include "xdg-output-unstable-v1.h"
#include "../wayland.h"

static struct zxdg_output_manager_v1 *xdg_output_manager;

static void xdg_output_noop ()
{
}

static void xdg_output_handle_name ( void *monitor,
    struct zxdg_output_v1 *xdg_output, const gchar *name )
{
  g_object_set_data_full(G_OBJECT(monitor),"xdg_name",g_strdup(name),g_free);
}

static void xdg_output_handle_done ( void *monitor,
    struct zxdg_output_v1 *xdg_output )
{
  zxdg_output_v1_destroy(xdg_output);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
  .logical_position = xdg_output_noop,
  .logical_size = xdg_output_noop,
  .done = xdg_output_handle_done,
  .name = xdg_output_handle_name,
  .description = xdg_output_noop,
};

void xdg_output_new ( GdkMonitor *monitor )
{
  struct wl_output *output;
  struct zxdg_output_v1 *xdg;

  if(!monitor || !xdg_output_manager)
    return;

  output = gdk_wayland_monitor_get_wl_output(monitor);

  if(!output)
    return;

  xdg = zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, output);

  if(xdg)
  {
    zxdg_output_v1_add_listener(xdg,&xdg_output_listener,monitor);
    g_object_set_data(G_OBJECT(monitor),"xdg_output",xdg);
  }
}

void xdg_output_destroy ( GdkMonitor *gmon )
{
  struct zxdg_output_v1 *xdg;

  if(!gmon || !xdg_output_manager)
    return;

  xdg = g_object_get_data(G_OBJECT(gmon),"xdg_output");

  if(xdg)
    zxdg_output_v1_destroy(xdg);
}

gboolean xdg_output_check ( void )
{
  GdkDisplay *gdisp;
  gint i;

  if(!xdg_output_manager)
    return TRUE;

  gdisp = gdk_display_get_default();

  for(i=0;i<gdk_display_get_n_monitors(gdisp);i++)
    if(!g_object_get_data(G_OBJECT(gdk_display_get_monitor(gdisp,i)),
          "xdg_name"))
      return FALSE;

  return TRUE;
}

void xdg_output_init ( void )
{
  GdkDisplay *display;
  gint i,n;

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

  display = gdk_display_get_default();
  n = gdk_display_get_n_monitors(display);
  for(i=0;i<n;i++)
    xdg_output_new(gdk_display_get_monitor(display,i));
  wl_display_roundtrip(gdk_wayland_display_get_wl_display(display));
}
