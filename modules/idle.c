/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- Sfwbar maintainers
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include "module.h"
#include "wayland.h"
#include "ext-idle-notify-v1.h"
#include "gui/basewidget.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;
static struct ext_idle_notifier_v1 *idle_notifier;
static struct ext_idle_notification_v1 *idle_notification;
static gint64 timeout = 20000;

static void idle_idled ( void *data, struct ext_idle_notification_v1 *not )
{
  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("idled"));
}

static void idle_resumed ( void *data, struct ext_idle_notification_v1 *not )
{
  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("resumed"));
}

static const struct ext_idle_notification_v1_listener idle_listener = {
  .idled = idle_idled,
  .resumed = idle_resumed,
};

void idle_notification_get ( void )
{
  if(idle_notifier)
  {
    idle_notification = ext_idle_notifier_v1_get_idle_notification(
        idle_notifier, timeout, gdk_wayland_seat_get_wl_seat(
          gdk_display_get_default_seat(gdk_display_get_default())));
    if(idle_notification)
      ext_idle_notification_v1_add_listener(idle_notification, &idle_listener,
          NULL);
  }
}

gboolean sfwbar_module_init ( void )
{
  idle_notifier = wayland_iface_register(
          ext_idle_notifier_v1_interface.name, 1, 1,
          &ext_idle_notifier_v1_interface);
  idle_notification_get();
  return TRUE;
}

static void idle_timeout_action ( gchar *stime, gchar *dummy, void *widget,
    void *d2, void *d3, void *d4 )
{
  gint64 new_timeout;

  if(!stime)
    return;

  new_timeout = g_ascii_strtoll(stime, NULL, 10) * 1000;
  if(timeout == new_timeout)
    return;

  if(idle_notification)
    ext_idle_notification_v1_destroy(idle_notification);

  timeout = new_timeout;
  if(new_timeout > 0)
    idle_notification_get();
  else
    idle_notification = NULL;
}

static ModuleActionHandlerV1 act_handler1 = {
  .name = "SetIdleTimeout",
  .function = idle_timeout_action
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  NULL
};
