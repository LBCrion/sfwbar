/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include "sfwbar.h"
#include "wayland.h"
#include "xdg-output-unstable-v1.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "wlr-layer-shell-unstable-v1.h"
#include <gdk/gdkwayland.h>

static GList *wayland_ifaces;
static struct wl_registry *wayland_registry;
static gboolean wayland_init_complete;

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

struct wl_registry *wayland_registry_get ( void )
{
  return wayland_registry;
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

  wdisp = gdk_wayland_display_get_wl_display(gdk_display_get_default());
  if(!wdisp)
    g_error("Can't get wayland display\n");

  wayland_registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(wayland_registry, &registry_listener, NULL);
  wl_display_roundtrip(wdisp);
  wayland_init_complete = TRUE;
  xdg_output_init();
  wayland_monitor_probe();
  g_debug("default output: %s", (gchar *)g_object_get_data(
        G_OBJECT(wayland_monitor_get_default()), "xdg_name"));

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
