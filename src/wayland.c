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

void foreign_toplevel_register (struct wl_registry *registry, uint32_t name);
void xdg_output_init ( void );
void layer_shell_register (struct wl_registry *, uint32_t , uint32_t );
void shm_register (struct wl_registry *registry, uint32_t name );
void wayland_monitor_probe ( void );

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

wayland_iface_t *wayland_iface_lookup ( const gchar *interface, guint32 ver )
{
  GList *iter;
  wayland_iface_t *iface;

  for(iter=wayland_ifaces; iter; iter=g_list_next(iter))
  {
    iface = iter->data;
    if(iface->version >= ver && !g_strcmp0(iface->iface, interface))
      return iface;
  }
  return NULL;
}

struct wl_registry *wayland_registry_get ( void )
{
  return wayland_registry;
}

/*
  if (!g_strcmp0(interface,zxdg_output_manager_v1_interface.name))
    xdg_output_register(wayland_registry, name, version);
  else if (!g_strcmp0(interface, wl_shm_interface.name))
    shm_register(wayland_registry, name);
  else if (!g_strcmp0(interface, zwlr_layer_shell_v1_interface.name))
    layer_shell_register(wayland_registry, name, version);
}*/

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
        G_OBJECT(wayland_monitor_get_default()),"xdg_name"));

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
