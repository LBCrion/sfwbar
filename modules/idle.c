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
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;
static struct ext_idle_notifier_v1 *idle_notifier;
static GHashTable *idle_timers;
static gboolean idled;

typedef struct _idle_notification {
  struct ext_idle_notification_v1 *notif;
  gint64 timeout;
  gchar *trigger;
} idle_notification_t;

static void idle_idled ( void *data, struct ext_idle_notification_v1 *not )
{
  idled = TRUE;
  trigger_emit(data);
}

static void idle_resumed ( void *data, struct ext_idle_notification_v1 *not )
{
  if(!idled)
    return;
  trigger_emit("idle_resume");
  idled = FALSE;
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

static value_t idle_timeout_action ( vm_t *vm, value_t p[], gint np )
{
  struct ext_idle_notification_v1 *notif;
  idle_notification_t *timer;
  gint64 new_timeout;

  vm_param_check_np(vm, np, 2, "IdleTimeout");
  vm_param_check_string(vm, p, 0, "IdleTimeout");

  new_timeout = value_as_numeric(p[1]) * 1000;

  if( (timer = g_hash_table_lookup(idle_timers, value_get_string(p[0]))) &&
      (timer->timeout != new_timeout)) 
    g_hash_table_remove(idle_timers, value_get_string(p[0]));


  if(new_timeout < 1)
    return value_na;

  if( (notif = ext_idle_notifier_v1_get_idle_notification(
        idle_notifier, new_timeout, gdk_wayland_seat_get_wl_seat(
          gdk_display_get_default_seat(gdk_display_get_default())))) )
  {
    timer = g_malloc0(sizeof(idle_notification_t));
    timer->timeout = new_timeout;
    timer->trigger = g_strdup(value_get_string(p[0]));
    timer->notif = notif;
    ext_idle_notification_v1_add_listener(notif, &idle_listener, timer->trigger);
    g_hash_table_insert(idle_timers, timer->trigger, timer);
  }

  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  idle_notifier = wayland_iface_register(
          ext_idle_notifier_v1_interface.name, 1, 1,
          &ext_idle_notifier_v1_interface);
  if(!idle_notifier)
    return FALSE;
  vm_func_add("IdleTimeout", idle_timeout_action, TRUE);
  idle_timers = g_hash_table_new_full((GHashFunc)str_nhash, (GCompareFunc)str_nequal,
      NULL, (GDestroyNotify)idle_notification_free);
  return TRUE;
}
