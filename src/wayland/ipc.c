/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gdk/gdkwayland.h>
#include "cosmic-workspace-unstable-v1.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"

void foreign_toplevel_register(struct wl_registry *registry,
    uint32_t global, uint32_t version);

void cosmic_workspaces_register(struct wl_registry *registry,
    uint32_t global, uint32_t version);

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t global, const gchar *interface, uint32_t version)
{
  if (!g_strcmp0(interface,zwlr_foreign_toplevel_manager_v1_interface.name))
    foreign_toplevel_register(registry, global, version);
  else if (!g_strcmp0(interface, zcosmic_workspace_manager_v1_interface.name))
    cosmic_workspaces_register(registry, global, version);
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
  .global_remove = handle_global_remove
};

void wayland_ipc_init ( void )
{
  struct wl_display *wdisp;
  struct wl_registry *registry;

  wdisp = gdk_wayland_display_get_wl_display(gdk_display_get_default());
  if(!wdisp)
    g_error("Can't get wayland display\n");

  registry = wl_display_get_registry(wdisp);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(wdisp);

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
