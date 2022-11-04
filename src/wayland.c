#include "sfwbar.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include "xdg-output-unstable-v1.h"
#include "idle-inhibit-unstable-v1.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"

void foreign_toplevel_register (struct wl_registry *registry, uint32_t name);
void idle_inhibit_register (struct wl_registry *registry, uint32_t name);
void xdg_output_register (struct wl_registry *registry, uint32_t name);

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const gchar *interface, uint32_t version)
{
  if (!g_strcmp0(interface,zwp_idle_inhibit_manager_v1_interface.name))
    idle_inhibit_register(registry,name);
  else if (!g_strcmp0(interface,zxdg_output_manager_v1_interface.name))
    xdg_output_register(registry,name);
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

  wl_display_roundtrip(wdisp);
  wl_display_roundtrip(wdisp);
}
