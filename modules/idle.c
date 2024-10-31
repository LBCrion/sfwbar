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
#include "trigger.h"
#include "util/string.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;
static struct ext_idle_notifier_v1 *idle_notifier;
static GHashTable *idle_timers;

typedef struct _idle_notification {
  struct ext_idle_notification_v1 *notif;
  gint64 timeout;
  gchar *trigger;
} idle_notification_t;

static void idle_idled ( void *data, struct ext_idle_notification_v1 *not )
{
  trigger_emit(data);
}

static void idle_resumed ( void *data, struct ext_idle_notification_v1 *not )
{
  trigger_emit("resumed");
}

static const struct ext_idle_notification_v1_listener idle_listener = {
  .idled = idle_idled,
  .resumed = idle_resumed,
};

void idle_notification_free ( idle_notification_t *notif )
{
  ext_idle_notification_v1_destroy(notif->notif);
  g_free(notif->trigger);
  g_free(notif);
}

gboolean sfwbar_module_init ( void )
{
  idle_notifier = wayland_iface_register(
          ext_idle_notifier_v1_interface.name, 1, 1,
          &ext_idle_notifier_v1_interface);
  if(!idle_notifier)
    return FALSE;
  idle_timers = g_hash_table_new_full((GHashFunc)str_nhash, (GCompareFunc)str_nequal,
      (GDestroyNotify)idle_notification_free, NULL);
  return TRUE;
}

static void idle_timeout_action ( gchar *stime, gchar *name, void *widget,
    void *d2, void *d3, void *d4 )
{
  struct ext_idle_notification_v1 *notif;
  idle_notification_t *timer;
  gint64 new_timeout;

  if(!stime || !name)
    return;

  new_timeout = g_ascii_strtoll(stime, NULL, 10) * 1000;

  if( (timer = g_hash_table_lookup(idle_timers, name)) &&
      (timer->timeout != new_timeout)) 
    g_hash_table_remove(idle_timers, name);

  if(new_timeout < 1)
    return;

  if( (notif = ext_idle_notifier_v1_get_idle_notification(
        idle_notifier, new_timeout, gdk_wayland_seat_get_wl_seat(
          gdk_display_get_default_seat(gdk_display_get_default())))) )
  {
    timer = g_malloc0(sizeof(idle_notification_t));
    timer->timeout = new_timeout;
    timer->trigger = g_strdup(name);
    timer->notif = notif;
    ext_idle_notification_v1_add_listener(notif, &idle_listener, timer->trigger);
    g_hash_table_insert(idle_timers, timer->trigger, timer);
  }
}

static ModuleActionHandlerV1 act_handler1 = {
  .name = "IdleTimeout",
  .function = idle_timeout_action
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  NULL
};
