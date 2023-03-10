/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include "idle-inhibit-unstable-v1.h"

static struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;

void wayland_set_idle_inhibitor ( GtkWidget *widget, gboolean inhibit )
{
  struct wl_surface *surface;
  struct zwp_idle_inhibitor_v1 *inhibitor;

  if(!idle_inhibit_manager)
    return;

  surface = gdk_wayland_window_get_wl_surface(
      gtk_widget_get_window(widget));

  if(!surface)
    return;

  inhibitor = g_object_get_data(G_OBJECT(widget),"inhibitor");
  if(inhibit && !inhibitor)
  {
    inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
        idle_inhibit_manager, surface );
    g_object_set_data(G_OBJECT(widget),"inhibitor",inhibitor);
  }
  else if( !inhibit && inhibitor )
  {
    g_object_set_data(G_OBJECT(widget),"inhibitor",NULL);
    zwp_idle_inhibitor_v1_destroy(inhibitor);
  }
}

void idle_inhibit_register (struct wl_registry *registry, uint32_t name)
{
  idle_inhibit_manager = wl_registry_bind(registry,name,
      &zwp_idle_inhibit_manager_v1_interface,1);
}
