/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include "sfwbar.h"
#include "wayland.h"
#include "xdg-output-unstable-v1.h"
#include "idle-inhibit-unstable-v1.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "wlr-layer-shell-unstable-v1.h"
#include <gdk/gdkwayland.h>

void foreign_toplevel_register (struct wl_registry *registry, uint32_t name);
void idle_inhibit_register (struct wl_registry *registry, uint32_t name);
void xdg_output_register (struct wl_registry *registry, uint32_t name);
void layer_shell_register (struct wl_registry *, uint32_t , uint32_t );
void shm_register (struct wl_registry *registry, uint32_t name );
void wayland_monitor_probe ( void );

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  if (!g_strcmp0(interface,zwp_idle_inhibit_manager_v1_interface.name))
    idle_inhibit_register(registry,name);
  else if (!g_strcmp0(interface,zxdg_output_manager_v1_interface.name))
    xdg_output_register(registry,name);
  else if (!g_strcmp0(interface, wl_shm_interface.name))
    shm_register(registry, name);
  else if (!g_strcmp0(interface, zwlr_layer_shell_v1_interface.name))
    layer_shell_register(registry, name, version);
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
  struct wl_registry *registry;

  wdisp = gdk_wayland_display_get_wl_display(gdk_display_get_default());
  if(!wdisp)
    g_error("Can't get wayland display\n");

  registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(wdisp);
  wayland_monitor_probe();
  GdkMonitor *mon = wayland_monitor_get_default();
  g_message("default output: %s",(gchar *)g_object_get_data(G_OBJECT(mon),"xdg_name"));

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
