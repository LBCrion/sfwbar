/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include "../src/module.h"
#include "../src/basewidget.h"
#include "../src/wayland.h"
#include "idle-inhibit-unstable-v1.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;
static struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;

gboolean sfwbar_module_init ( void )
{
  idle_inhibit_manager = wayland_iface_register(
          zwp_idle_inhibit_manager_v1_interface.name, 1, 1,
          &zwp_idle_inhibit_manager_v1_interface);
  return TRUE;
}

void *idle_inhibit_expr_func ( void **params, void *widget, void *event )
{
  if(widget && g_object_get_data(G_OBJECT(widget), "inhibitor"))
    return g_strdup("on");
  else
    return g_strdup("off");
}

static void idle_inhibitor_action ( gchar *act, gchar *dummy, void *widget,
    void *d2, void *d3, void *d4 )
{
  struct wl_surface *surface;
  struct zwp_idle_inhibitor_v1 *inhibitor;
  gboolean inhibit;

  inhibitor = g_object_get_data(G_OBJECT(widget), "inhibitor");

  if(!g_ascii_strcasecmp(act, "on"))
    inhibit = TRUE;
  else if(!g_ascii_strcasecmp(act, "off"))
    inhibit = FALSE;
  else if(!g_ascii_strcasecmp(act, "toggle"))
    inhibit = !inhibitor;
  else
    return;

  if(inhibit && !inhibitor)
  {
    if( !(surface = gdk_wayland_window_get_wl_surface(
        gtk_widget_get_window(widget))) )
      return;

    inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
        idle_inhibit_manager, surface );
    g_object_set_data(G_OBJECT(widget), "inhibitor", inhibitor);
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("idleinhibitor"));
  }
  else if( !inhibit && inhibitor )
  {
    g_object_set_data(G_OBJECT(widget), "inhibitor", NULL);
    zwp_idle_inhibitor_v1_destroy(inhibitor);
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("idleinhibitor"));
  }
}

ModuleExpressionHandlerV1 handler1 = {
  .flags = 0,
  .name = "IdleInhibitState",
  .parameters = "",
  .function = idle_inhibit_expr_func
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &handler1,
  NULL
};

ModuleActionHandlerV1 act_handler1 = {
  .name = "SetIdleInhibitor",
  .function = idle_inhibitor_action
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  NULL
};
