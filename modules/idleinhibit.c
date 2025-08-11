/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include "module.h"
#include "trigger.h"
#include "wayland.h"
#include "idle-inhibit-unstable-v1.h"
#include "vm/vm.h"
#include "gui/basewidget.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;
static struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;

static value_t idle_inhibit_state ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget = vm_widget_get(vm, NULL);
  return value_new_string(g_strdup((widget && g_object_get_data(
            G_OBJECT(widget), "inhibitor"))? "on" : "off"));
}

static value_t idle_inhibitor_action ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget = vm_widget_get(vm, NULL);
  struct wl_surface *surface;
  struct zwp_idle_inhibitor_v1 *inhibitor;
  gboolean inhibit;

  vm_param_check_np(vm, np, 1, "SetIdleInhibitor");
  vm_param_check_string(vm, p, 0, "SetIdleInhibitor");

  inhibitor = g_object_get_data(G_OBJECT(widget), "inhibitor");

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "on"))
    inhibit = TRUE;
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "off"))
    inhibit = FALSE;
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "toggle"))
    inhibit = !inhibitor;
  else
    return value_na;

  if(inhibit && !inhibitor)
  {
    if( !(surface = gdk_wayland_window_get_wl_surface(
        gtk_widget_get_window(widget))) )
      return value_na;

    inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
        idle_inhibit_manager, surface );
    g_object_set_data(G_OBJECT(widget), "inhibitor", inhibitor);
    trigger_emit("idleinhibitor");
  }
  else if( !inhibit && inhibitor )
  {
    g_object_set_data(G_OBJECT(widget), "inhibitor", NULL);
    zwp_idle_inhibitor_v1_destroy(inhibitor);
    trigger_emit("idleinhibitor");
  }
  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  vm_func_add("idleinhibitstate", idle_inhibit_state, FALSE, TRUE);
  vm_func_add("setidleinhibitor", idle_inhibitor_action, TRUE, TRUE);
  idle_inhibit_manager = wayland_iface_register(
          zwp_idle_inhibit_manager_v1_interface.name, 1, 1,
          &zwp_idle_inhibit_manager_v1_interface);
  return TRUE;
}
