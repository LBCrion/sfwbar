#include <glib.h>
#include <gio/gio.h>
#include "module.h"
#include "trigger.h"
#include "gui/scaleimage.h"
#include "util/string.h"
#include "vm/vm.h"

typedef struct _dn_action {
  gchar *id;
  gchar *title;
} dn_action;

typedef struct _dn_notification {
  gchar *app_name, *app_icon, *summary, *body;
  gint32 timeout;
  GDateTime *time;
  guint32 id;
  gboolean action_icons, resident, transient, silent;
  gchar *category, *desktop, *image, *sound_file, *sound_name;
  gchar *action_id, *action_title;
  gint32 x,y;
  gchar urgency;
  /* need image data */
  guint timeout_handle;
} dn_notification;

#define DN_NOTIFICATION(x) ((dn_notification *)(x))

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;

static GDBusConnection *dn_con;
static guint32 dn_id_counter = 1;
static guint dn_pixbuf_counter;

static GList *notif_list;
static gchar *expanded_group;
static gint32 default_timeout = 0;

static GAppLaunchContext *dn_launch_context;
static GAppInfo *dn_app_info;

static const gchar *dn_bus = "org.freedesktop.Notifications";
static const gchar *dn_path = "/org/freedesktop/Notifications";

static const gchar dn_iface_xml[] = 
  "<node>"
  " <interface name='org.freedesktop.Notifications'>"
  "   <signal name='NotificationClosed'>"
  "     <arg name='id' type='u' direction='out'/>"
  "     <arg name='reason' type='u' direction='out'/>"
  "   </signal>"
  "   <signal name='ActionInvoked'>"
  "     <arg name='id' type='u' direction='out'/>"
  "     <arg name='action_key' type='s' direction='out'/>"
  "   </signal>"
  "   <signal name='ActivationToken'>"
  "     <arg name='id' type='u' direction='out'/>"
  "     <arg name='activation_token' type='s' direction='out'/>"
  "   </signal>"
  "   <method name='Notify'>"
  "     <arg type='u' direction='out'/>"
  "     <arg name='app_name' type='s' direction='in'/>"
  "     <arg name='replaces_id' type='u' direction='in'/>"
  "     <arg name='app_icon' type='s' direction='in'/>"
  "     <arg name='summary' type='s' direction='in'/>"
  "     <arg name='body' type='s' direction='in'/>"
  "     <arg name='actions' type='as' direction='in'/>"
  "     <arg name='hints' type='a{sv}' direction='in'/>"
  "     <arg name='timeout' type='i' direction='in'/>"
  "   </method>"
  "   <method name='CloseNotification'>"
  "     <arg name='id' type='u' direction='in'/>"
  "   </method>"
  "   <method name='GetCapabilities'>"
  "     <arg type='as' name='caps' direction='out'/>"
  "   </method>"
  "   <method name='GetServerInformation'>"
  "     <arg type='s' name='name' direction='out'/>"
  "     <arg type='s' name='vendor' direction='out'/>"
  "     <arg type='s' name='version' direction='out'/>"
  "     <arg type='s' name='spec_version' direction='out'/>"
  "   </method>"
  " </interface>"
  "</node>";

static dn_notification *dn_notification_dup ( dn_notification *src )
{
  dn_notification *dest;

  dest = g_malloc0(sizeof(dn_notification));
  dest->app_name = g_strdup(src->app_name);
  dest->app_icon = g_strdup(src->app_icon);
  dest->summary = g_strdup(src->summary);
  dest->body = g_strdup(src->body);
  dest->category = g_strdup(src->category);
  dest->desktop = g_strdup(src->desktop);
  dest->image = g_strdup(src->image);
  dest->sound_file = g_strdup(src->sound_file);
  dest->sound_name = g_strdup(src->sound_name);
  dest->action_id = g_strdup(src->action_id);
  dest->action_title = g_strdup(src->action_title);
  dest->timeout = src->timeout;
  dest->id = src->id;
  dest->x = src->x;
  dest->y = src->y;
  dest->action_icons = src->action_icons;
  dest->resident = src->resident;
  dest->transient = src->transient;
  dest->silent = src->silent;
  dest->urgency = src->urgency;
  dest->time = g_date_time_add_seconds(src->time, 0);

  return dest;
}

static void dn_notification_free ( dn_notification *notif )
{
  if(notif->timeout_handle)
    g_source_remove(notif->timeout_handle);
  if(notif->time)
    g_date_time_unref(notif->time);
  g_free(notif->app_name);
  g_free(notif->app_icon);
  g_free(notif->summary);
  g_free(notif->body);
  g_free(notif->category);
  g_free(notif->desktop);
  scale_image_cache_remove(notif->image);
  g_free(notif->image);
  g_free(notif->sound_file);
  g_free(notif->sound_name);
  g_free(notif->action_id);
  g_free(notif->action_title);
  g_free(notif);
}

static gint dn_notification_comp ( dn_notification *n1, dn_notification *n2 )
{
  if(n1->id != n2->id)
    return n1->id - n2->id;
  return g_strcmp0(n1->action_id, n2->action_id);
}

static gchar *dn_notification_get_str ( dn_notification *notif, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "id"))
    return g_strdup_printf("%d", notif->id);
  if(!g_ascii_strcasecmp(prop, "icon"))
    return g_strdup(notif->image?notif->image:notif->app_icon);
  if(!g_ascii_strcasecmp(prop, "app"))
    return g_strdup(notif->app_name);
  if(!g_ascii_strcasecmp(prop, "summary"))
    return g_strdup(notif->summary);
  if(!g_ascii_strcasecmp(prop, "body"))
    return g_strdup(notif->body);
  if(!g_ascii_strcasecmp(prop, "time"))
    return g_date_time_format(notif->time,"%s");
  if(!g_ascii_strcasecmp(prop, "category"))
    return g_strdup(notif->category);
  if(!g_ascii_strcasecmp(prop, "action_id"))
    return g_strdup(notif->action_id);
  if(!g_ascii_strcasecmp(prop, "action_title"))
    return g_strdup(notif->action_title);
  return NULL;
}

static module_queue_t update_q = {
  .free = (void (*)(void *))dn_notification_free,
  .duplicate = (void *(*)(void *))dn_notification_dup,
  .get_str = (void *(*)(void *, gchar *))dn_notification_get_str,
  .compare = (gboolean (*)(const void *, const void *))dn_notification_comp,
};

static void *dn_remove_get_str ( gchar *name, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "removed-id"))
    return g_strdup(name);
  return NULL;
}

static module_queue_t remove_q = {
  .free = g_free,
  .duplicate = (void *(*)(void *))ptr_pass,
  .get_str = (void *(*)(void *, gchar *))dn_remove_get_str,
  .compare = (gboolean (*)(const void *, const void *))g_strcmp0,
};

static dn_notification *dn_notification_lookup ( guint32 id )
{
  GList *iter;

  for(iter=notif_list; iter; iter=g_list_next(iter))
    if(DN_NOTIFICATION(iter->data)->id == id)
      break;
  if(!iter)
    return NULL;
  return iter->data;
}

static void dn_notification_close ( guint32 id, guchar reason )
{
  dn_notification *notif;
  GList *iter;

  g_debug("ncenter: close event: %d", id);

  if( !(notif = dn_notification_lookup(id)) )
    return;
  notif_list = g_list_remove(notif_list, notif);

  if(!g_strcmp0(notif->app_name, expanded_group))
  {
    for(iter=notif_list; iter; iter=g_list_next(iter))
      if(!g_strcmp0(DN_NOTIFICATION(iter->data)->app_name, notif->app_name))
        break;
    if(!iter)
      g_free(g_steal_pointer(&expanded_group));
  }

  dn_notification_free(notif);

  module_queue_append(&remove_q, g_strdup_printf("%d", id));
  trigger_emit("notification-group");
  g_dbus_connection_emit_signal(dn_con, NULL, dn_path, dn_bus,
      "NotificationClosed", g_variant_new("(uu)", id, reason), NULL);
}

static void dn_notification_expand ( guint32 id )
{
  dn_notification *notif;

  if( !(notif = dn_notification_lookup(id)) )
    return;

  g_debug("ncenter: expand event: '%s'", notif->app_name);

  g_free(expanded_group);
  expanded_group = g_strdup(notif->app_name);

  trigger_emit("notification-group");
}

static void dn_notification_collapse ( void )
{
  g_debug("ncenter: collapse event: '%s'", expanded_group);

  g_free(g_steal_pointer(&expanded_group));

  trigger_emit("notification-group");
}

static gchar *dn_parse_image_data ( GVariant *dict )
{
  GdkPixbuf *pixbuf;
  GVariant *vdata;
  gint32 w, h, row_stride, bps, channels;
  gboolean alpha;
  gsize len;
  gchar *name;
  const void *data;
  void *copy;

  if(!g_variant_lookup(dict, "image-data", "(iiibii@ay)", &w, &h, &row_stride,
        &alpha, &bps, &channels, &vdata))
    return NULL;

  data = g_variant_get_fixed_array(vdata, &len, sizeof(guchar));
  if(len != h*row_stride)
    return NULL;
  copy = g_memdup2(data, len);

  if( !(pixbuf = gdk_pixbuf_new_from_data(copy, GDK_COLORSPACE_RGB, alpha,
          bps, w, h, row_stride, (GdkPixbufDestroyNotify)g_free, NULL)) )
  {
    g_free(copy);
    return NULL;
  }

  name = g_strdup_printf("<pixbufcache/>ncenter-%d", dn_pixbuf_counter++);
  scale_image_cache_insert(name, pixbuf);
  return name;
}

static void dn_notification_action ( guint32 id, gchar *action )
{
  gchar *token;

  token = g_app_launch_context_get_startup_notify_id(dn_launch_context,
      dn_app_info, NULL);
  g_debug("ncenter: invoke action: %d, '%s' (token: %d)", id, action, !!token);
  g_dbus_connection_emit_signal(dn_con, NULL, dn_path, dn_bus,
      "ActivationToken", g_variant_new("(us)", id, token), NULL);
  g_dbus_connection_emit_signal(dn_con, NULL, dn_path, dn_bus,
      "ActionInvoked", g_variant_new("(us)", id, action), NULL);
  g_free(token);
}

gboolean dn_timeout ( dn_notification *notif )
{
  notif->timeout_handle = 0;
  dn_notification_close(notif->id, 1);
  return FALSE;
}

guint32 dn_notification_parse ( GVariant *params )
{
  dn_notification *notif;
  GVariantIter *aiter;
  GVariant *hints;
  GList *iter;
  gchar *action_title, *action_id;
  guint32 id;

  g_variant_get(params, "(susssas@a{sv}i)", NULL, &id,
      NULL,NULL,NULL,NULL,NULL,NULL);
  for(iter=notif_list; iter; iter=g_list_next(iter))
    if(DN_NOTIFICATION(iter->data)->id == id)
      break;

  if(iter)
    notif = iter->data;
  else
  {
    notif = g_malloc0(sizeof(dn_notification));
    notif_list = g_list_append(notif_list, notif);
  }
  g_variant_get(params, "(susssas@a{sv}i)", &notif->app_name, &notif->id,
      &notif->app_icon, &notif->summary,
      &notif->body, &aiter, &hints, &notif->timeout);

  if(notif->id < 1)
    notif->id = dn_id_counter++;

  g_clear_pointer(&notif->category, g_free);
  g_variant_lookup(hints, "category", "s", &notif->category);
  g_clear_pointer(&notif->desktop, g_free);
  g_variant_lookup(hints, "desktop-entry", "s", &notif->desktop);
  g_clear_pointer(&notif->image, g_free);
  if( !(notif->image = dn_parse_image_data(hints)) )
    g_variant_lookup(hints, "image-path", "s", &notif->image);
  g_clear_pointer(&notif->sound_file, g_free);
  g_variant_lookup(hints, "sound-file", "s", &notif->sound_file);
  g_clear_pointer(&notif->sound_name, g_free);
  g_variant_lookup(hints, "sound-name", "s", &notif->sound_name);
  if(!g_variant_lookup(hints, "action-icons", "b", &notif->action_icons))
    notif->action_icons = FALSE;
  if(!g_variant_lookup(hints, "transient", "b", &notif->transient))
    notif->transient = FALSE;
  if(!g_variant_lookup(hints, "resident", "b", &notif->resident))
    notif->resident = FALSE;
  if(!g_variant_lookup(hints, "suppress-sound", "b", &notif->silent))
    notif->silent = FALSE;
  if(!g_variant_lookup(hints, "urgency", "y", &notif->urgency))
    notif->urgency = 1;
  if(!g_variant_lookup(hints, "x", "i", &notif->x))
    notif->x = 0;
  if(!g_variant_lookup(hints, "y", "i", &notif->y))
    notif->y = 0;
  
  if(notif->time)
    g_date_time_unref(notif->time);
  notif->time = g_date_time_new_now_local();

  if(notif->timeout == -1)
    notif->timeout = notif->resident?0:default_timeout;
  if(notif->timeout > 0)
    notif->timeout_handle =
      g_timeout_add(notif->timeout, (GSourceFunc)dn_timeout, notif);

  module_queue_append(&update_q, notif);
  trigger_emit("notification-group");

  while(g_variant_iter_next(aiter, "&s", &action_id) &&
    g_variant_iter_next(aiter, "&s", &action_title))
  {
    g_free(notif->action_id);
    notif->action_id = g_strdup(action_id);
    g_free(notif->action_title);
    notif->action_title = g_strdup(action_title);
    module_queue_append(&update_q, notif);
    g_debug("ncenter: app: %u, action: %s: '%s'", notif->id, action_id,
        action_title);
  }
  g_variant_iter_free(aiter);

  g_debug("ncenter: app: '%s', id: %u, icon: '%s', summary '%s', body '%s', timeout: %d",
      notif->app_name, notif->id, notif->app_icon, notif->summary,
      notif->body, notif->timeout);

  return notif->id;
}

static void dn_iface_method(GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *method, GVariant *parameters,
    GDBusMethodInvocation *invocation, gpointer data)
{
  GVariant *caps[5];
  guint32 id;

  if(!g_strcmp0(method, "GetServerInformation"))
  {
    g_dbus_method_invocation_return_value(invocation, g_variant_new("(ssss)",
          "sfwbar", "org.hosers", "1", "1.2"));
    return;
  }
  if(!g_strcmp0(method, "GetCapabilities"))
  {
    caps[0] = g_variant_new_string("body");
    caps[1] = g_variant_new_string("icon-static");
    caps[2] = g_variant_new_string("actions");
    caps[3] = g_variant_new_string("action-icons");
    caps[4] = g_variant_new_string("persistence");
    g_dbus_method_invocation_return_value(invocation, g_variant_new("(@as)",
        g_variant_new_array(G_VARIANT_TYPE_STRING, caps, 5)));
    return;
  }
  if(!g_strcmp0(method, "Notify"))
  {
    g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)",
          dn_notification_parse(parameters)));
    return;
  }
  if(!g_strcmp0(method, "CloseNotification"))
  {
    g_variant_get(parameters, "(u)", &id);
    dn_notification_close(id, 3);
    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }
}

static const GDBusInterfaceVTable dn_iface_vtable =
{
  (GDBusInterfaceMethodCallFunc)dn_iface_method,
  NULL,
  NULL
};

static void dn_bus_acquired_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
  GDBusNodeInfo *node;

  node = g_dbus_node_info_new_for_xml(dn_iface_xml, NULL);
  g_dbus_connection_register_object (con, dn_path, node->interfaces[0],
      &dn_iface_vtable, NULL, NULL, NULL);
  g_dbus_node_info_unref(node);
}

static void dn_name_acquired_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
}

static void dn_name_lost_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
}

static value_t dn_get_func ( vm_t *vm, value_t p[], gint np )
{
  gchar *result;

  if(np!=1 || !value_is_string(p[0]) || !p[0].value.string)
    return value_na;

  if( (result = module_queue_get_string(&update_q, p[0].value.string)) )
    return value_new_string(result);
  if( (result = module_queue_get_string(&remove_q, p[0].value.string)) )
    return value_new_string(result);

  return value_na;
}

static value_t dn_group_func ( vm_t *vm, value_t p[], gint np )
{
  dn_notification *notif;
  GList *iter;
  guint32 id, count = 0;
  gboolean header = FALSE;

  if(np!=1 || !value_is_string(p[0]))
    return value_na;

  if( !(id = g_ascii_strtoull(p[0].value.string, NULL, 10)) )
    return value_na;
  for(iter=notif_list; iter; iter=g_list_next(iter))
    if(DN_NOTIFICATION(iter->data)->id == id)
      break;

  if(!iter)
    return value_na;
  notif = iter->data;

  if(expanded_group)
  {
    if(!g_strcmp0(expanded_group, notif->app_name))
      return value_new_string(g_strdup("visible"));
    else
      return value_na;
  }

  for(iter=notif_list; iter; iter=g_list_next(iter))
    if(!g_strcmp0(DN_NOTIFICATION(iter->data)->app_name, notif->app_name))
    {
      if(!count && iter->data==notif)
        header = TRUE;
      count++;
    }

  if(count==1)
    return value_new_string(g_strdup("sole"));

  if(count>1 && header)
    return value_new_string(g_strdup("header"));

  return value_na;
}

static value_t dn_active_group_func ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(g_strdup(expanded_group));
}

static value_t dn_count_func ( vm_t *vm, value_t p[], gint np )
{
  GList *iter;
  dn_notification *notif;
  gdouble result = 0;
  guint32 id;

  if(np!=1 || !value_is_string(p[0]))
    return value_new_numeric(g_list_length(notif_list));
  if( (id=g_ascii_strtoull(p[0].value.string, NULL, 10)) )
    if( (notif=dn_notification_lookup(id)) )
      for(iter=notif_list; iter; iter=g_list_next(iter))
        if(!g_strcmp0(DN_NOTIFICATION(iter->data)->app_name, notif->app_name))
          result++;

  return value_new_numeric(result);
}

static value_t dn_expand_action ( vm_t *vm, value_t p[], gint np )
{
  guint64 id;
  gchar *end;

  vm_param_check_np(vm, np, 1, "NotificationExpand");
  vm_param_check_string(vm, p, 0, "NotificationExpand");

  if(value_is_string(p[0]) && p[0].value.string &&
      (id = g_ascii_strtoull(p[0].value.string, &end, 10)) &&
      !(end && *end) && id <= G_MAXUINT32)
    dn_notification_expand(id);

  return value_na;
}

static value_t dn_collapse_action ( vm_t *vm, value_t p[], gint np )
{
  dn_notification_collapse();

  return value_na;
}

static value_t dn_close_action ( vm_t *vm, value_t p[], gint np )
{
  guint64 id;
  gchar *end;

  vm_param_check_np(vm, np, 1, "NotificationClose");
  vm_param_check_string(vm, p, 0, "NotificationClose");

  if(value_is_string(p[0]) && p[0].value.string &&
      (id = g_ascii_strtoull(p[0].value.string, &end, 10)) &&
      !(end && *end) && id <= G_MAXUINT32)
    dn_notification_close(id, 2);

  return value_na;
}

static value_t dn_action_action ( vm_t *vm, value_t p[], gint np )
{
  guint64 id;
  gchar *end;

  vm_param_check_np(vm, np, 2, "NotificationAction");
  vm_param_check_string(vm, p, 0, "NotificationAction");
  vm_param_check_string(vm, p, 1, "NotificationAction");

  if(value_is_string(p[0]) && p[0].value.string && value_is_string(p[1]) &&
      p[1].value.string &&
      (id = g_ascii_strtoull(p[0].value.string, &end, 10)) &&
      !(end && *end) && id <= G_MAXUINT32)
    dn_notification_action(id, p[1].value.string);

  return value_na;
}

static value_t dn_ack_action ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "NotificationAck");
  vm_param_check_string(vm, p, 0, "NotificationAck");

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "updated"))
    module_queue_remove(&update_q);
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "removed"))
    module_queue_remove(&remove_q);
  trigger_emit("notification-group");

  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  vm_func_add("notificationget", dn_get_func, FALSE);
  vm_func_add("notificationgroup", dn_group_func, FALSE);
  vm_func_add("notificationactivegroup", dn_active_group_func, FALSE);
  vm_func_add("notificationcount", dn_count_func, FALSE);
  vm_func_add("notificationclose", dn_close_action, TRUE);
  vm_func_add("notificationaction", dn_action_action, TRUE);
  vm_func_add("notificationack", dn_ack_action, TRUE);
  vm_func_add("notificationexpand", dn_expand_action, TRUE);
  vm_func_add("notificationcollapse", dn_collapse_action, TRUE);
  update_q.trigger = g_intern_static_string("notification-updated");
  remove_q.trigger = g_intern_static_string("notification-removed");

  dn_con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  g_bus_own_name(G_BUS_TYPE_SESSION, dn_bus, G_BUS_NAME_OWNER_FLAGS_NONE,
      dn_bus_acquired_cb, dn_name_acquired_cb, dn_name_lost_cb, NULL, NULL);

  dn_launch_context = (GAppLaunchContext *)
    gdk_display_get_app_launch_context(gdk_display_get_default());
  dn_app_info = g_app_info_create_from_commandline("/bin/sh", NULL,
      G_APP_INFO_CREATE_NONE, NULL);

  return TRUE;
}
