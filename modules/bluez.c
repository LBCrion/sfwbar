/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <gio/gio.h>
#include "../src/module.h"
#include "../src/basewidget.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;

static GHashTable *devices;
static GList *adapters;
static GDBusConnection *bz_con;
static const gchar *bz_serv = "org.bluez";
static guint sub_add, sub_del, sub_chg;

typedef struct _bz_device {
  gchar *path;
  gchar *addr;
  gchar *name;
  gchar *icon;
  gboolean paired;
  gboolean trusted;
  gboolean connected;
  gboolean connecting;
} BzDevice;

typedef struct _bz_adapter {
  gchar *path;
  gchar *iface;
  guint scan_timeout;
  guint timeout_handle;
} BzAdapter;

static void bz_adapter_free ( gchar *object )
{
  GList *iter;
  BzAdapter *adapter;
  gboolean trigger;

  for(iter=adapters; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((BzAdapter *)(iter->data))->path, object))
      break;
  if(!iter)
    return;

  adapter = iter->data;
  adapters = g_list_remove(adapters, adapter);
  trigger = !adapters;
  if(trigger)
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("bluez_running"));
  if(adapter->timeout_handle)
    g_source_remove(adapter->timeout_handle);
  g_free(adapter->path);
  g_free(adapter->iface);
  g_free(adapter);
}

static void bz_device_free ( BzDevice *device )
{
  g_free(device->path);
  g_free(device->addr);
  g_free(device->name);
  g_free(device->icon);
  g_free(device);
}

static BzDevice *bz_device_dup ( BzDevice *src )
{
  BzDevice *dest;

  dest = g_malloc0(sizeof(BzDevice));
  dest->path = g_strdup(src->path);
  dest->addr = g_strdup(src->addr);
  dest->name = g_strdup(src->name);
  dest->icon = g_strdup(src->icon);
  dest->paired = src->paired;
  dest->trusted = src->trusted;
  dest->connected = src->connected;
  dest->connecting = src->connecting;

  return dest;
}

static gboolean bz_device_compare ( BzDevice *dev1, BzDevice *dev2 )
{
  if(!dev1 || !dev2)
    return 0;
  return g_strcmp0(dev1->path, dev2->path);
}

static gpointer bz_device_get_str ( BzDevice *device, gchar *prop )
{
    if(!g_ascii_strcasecmp(prop, "Name"))
      return g_strdup(device->name?device->name:"");
    if(!g_ascii_strcasecmp(prop, "Address"))
      return g_strdup(device->addr?device->addr:"");
    if(!g_ascii_strcasecmp(prop, "Icon"))
      return g_strdup(device->icon?device->icon:"");
    if(!g_ascii_strcasecmp(prop, "Path"))
      return g_strdup(device->path?device->path:"");
    return NULL;
}

static gpointer bz_device_get_num ( BzDevice *device, gchar *prop )
{
  gdouble result, *retval;

  if(!g_ascii_strcasecmp(prop, "Paired"))
    result = device->paired;
  else if(!g_ascii_strcasecmp(prop, "Connected"))
    result = device->connected;
  else if(!g_ascii_strcasecmp(prop, "Connecting"))
    result = device->connecting;
  else if(!g_ascii_strcasecmp(prop, "Trusted"))
    result = device->trusted;
  else
    return NULL;

  retval = g_malloc(sizeof(double));
  *retval = result;
  return retval;
}

module_queue_t update_q = {
  .free = (void (*)(void *))bz_device_free,
  .duplicate = (void *(*)(void *))bz_device_dup,
  .get_str = (void *(*)(void *, gchar *))bz_device_get_str,
  .get_num = (void *(*)(void *, gchar *))bz_device_get_num,
  .compare = (gboolean (*)(const void *, const void *))bz_device_compare,
};

static void *bz_remove_get_str ( gchar *name, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "RemovedPath"))
    return g_strdup(name);
  else
    return NULL;
}

module_queue_t remove_q = {
  .free = g_free,
  .duplicate = (void *(*)(void *))g_strdup,
  .get_str = (void *(*)(void *, gchar *))bz_remove_get_str,
  .compare = (gboolean (*)(const void *, const void *))g_strcmp0,
};

static BzAdapter *bz_adapter_get ( void )
{
  if(!adapters)
    return NULL;
  return adapters->data;
}

static void bz_scan_stop_cb ( GDBusConnection *con, GAsyncResult *res,
    BzAdapter *adapter)
{
  GVariant *result;

  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("bluez_scan_complete"));
  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result)
    g_variant_unref(result);
}

static gboolean bz_scan_stop ( BzAdapter *adapter )
{
  g_debug("bluez: scan off");
  g_dbus_connection_call(bz_con, bz_serv, adapter->path,
      adapter->iface, "StopDiscovery", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
      -1, NULL, (GAsyncReadyCallback)bz_scan_stop_cb, NULL);
  adapter->timeout_handle = 0;
  return FALSE;
}

static void bz_scan_cb ( GDBusConnection *con, GAsyncResult *res,
    BzAdapter *adapter)
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
  {
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("bluez_scan_complete"));
    return;
  }
  g_variant_unref(result);

  if(adapter->scan_timeout)
    adapter->timeout_handle = g_timeout_add(adapter->scan_timeout,
        (GSourceFunc)bz_scan_stop, adapter);
}

static void bz_scan_filter_cb ( GDBusConnection *con, GAsyncResult *res,
    BzAdapter *adapter)
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
  {
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("bluez_scan_complete"));
    return;
  }

  g_debug("bluez: scan on");
  g_dbus_connection_call(con, bz_serv, adapter->path,
      adapter->iface, "StartDiscovery", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
      -1, NULL, (GAsyncReadyCallback)bz_scan_cb, adapter);
  g_variant_unref(result);
}

static void bz_scan ( GDBusConnection *con, guint timeout )
{
  BzAdapter *adapter;
  GVariantBuilder *builder;
  GVariant *dict;

  adapter = bz_adapter_get();
  if(!adapter || adapter->timeout_handle)
    return;

  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("bluez_scan"));
  builder = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(builder, "{sv}", "Transport",
      g_variant_new_string("bredr"));
  dict = g_variant_builder_end(builder);
  g_variant_builder_unref(builder);

  adapter->scan_timeout = timeout;
  g_dbus_connection_call(con, bz_serv, adapter->path,
      adapter->iface, "SetDiscoveryFilter", g_variant_new_tuple(&dict,1),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)bz_scan_filter_cb, adapter);
}


static void bz_connect_cb ( GDBusConnection *con, GAsyncResult *res,
    BzDevice *device)
{
  GVariant *result;

  device->connecting =  FALSE;
  module_queue_append(&update_q, device);
  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
    return;

  g_debug("bluez: connected %s (%s)", device->addr, device->name);
  g_variant_unref(result);
}

static void bz_connect ( BzDevice *device )
{
  if(!device->connecting)
  {
    device->connecting = TRUE;
    module_queue_append(&update_q, device);
  }
  g_debug("bluez: attempting to connect %s (%s)", device->addr, device->name);
  g_dbus_connection_call(bz_con, bz_serv, device->path,
      "org.bluez.Device1", "Connect", NULL,
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)bz_connect_cb, device);
}

static void bz_disconnect_cb ( GDBusConnection *con, GAsyncResult *res,
    BzDevice *device)
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
    return;

  g_debug("bluez: disconnected %s (%s)", device->addr, device->name);
  g_variant_unref(result);
}

static void bz_disconnect ( BzDevice *device )
{
  if(!device->connected)
    return;
  g_debug("bluez: attempting to disconnect %s (%s)", device->addr, device->name);
  g_dbus_connection_call(bz_con, bz_serv, device->path,
      "org.bluez.Device1", "Disconnect", NULL,
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)bz_disconnect_cb, device);
}

static void bz_trust_cb ( GDBusConnection *con, GAsyncResult *res,
    BzDevice *device)
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
  {
    device->connecting =  FALSE;
    module_queue_append(&update_q, device);
    return;
  }

  g_debug("bluez: trusted %s (%s)", device->addr, device->name);
  bz_connect(device);
  g_variant_unref(result);
}

static void bz_trust ( BzDevice *device )
{
  if(device->trusted)
  {
    bz_connect(device);
    return;
  }

  g_debug("bluez: attempting to trust %s (%s)", device->addr, device->name);
  g_dbus_connection_call(bz_con, bz_serv, device->path,
      "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)",
        "org.bluez.Device1","Trusted",g_variant_new_boolean(TRUE)),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)bz_trust_cb, device);
}

static void bz_pair_cb ( GDBusConnection *con, GAsyncResult *res,
    BzDevice *device)
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
  {
    device->connecting =  FALSE;
    module_queue_append(&update_q, device);
    return;
  }

  g_debug("bluez: paired %s (%s)", device->addr, device->name);
  bz_trust(device);
  g_variant_unref(result);
}

static void bz_pair ( BzDevice *device )
{
  device->connecting =  TRUE;
  module_queue_append(&update_q, device);

  if(device->paired)
  {
    bz_trust(device);
    return;
  }

  g_debug("bluez: attempting to pair %s (%s)", device->addr, device->name);
  g_dbus_connection_call(bz_con, bz_serv, device->path,
      "org.bluez.Device1", "Pair", NULL,
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)bz_pair_cb, device);
}

static void bz_remove_cb ( GDBusConnection *con, GAsyncResult *res,
    gchar *name)
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result)
  {
    g_debug("bluez: removed %s", name);
    g_variant_unref(result);
  }
  g_free(name);
}

static void bz_remove ( BzDevice *device )
{
  BzAdapter *adapter;

  adapter = bz_adapter_get();
  if(!adapter)
    return;

  g_debug("bluez: attempting to remove %s (%s)", device->addr, device->name);
  g_dbus_connection_call(bz_con, bz_serv, adapter->path,
      "org.bluez.Adapter1", "RemoveDevice", g_variant_new("(o)",device->path),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)bz_remove_cb, g_strdup(device->name));
  g_dbus_connection_call(bz_con, bz_serv, device->path,
      "org.freedesktop.DBus.Properties", "Set", g_variant_new("(ssv)",
        "org.bluez.Device1","Trusted",g_variant_new_boolean(FALSE)),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static gboolean bz_device_property_string ( gchar **prop, GVariant *val )
{
  gchar *result;

  g_variant_get(val, "s", &result);
  if(!g_strcmp0(*prop, result))
  {
    g_free(result);
    return FALSE;
  }
  g_clear_pointer(prop, g_free);
  *prop = result;
  return TRUE;
}

static gboolean bz_device_properties ( BzDevice *device, GVariantIter *piter )
{
  gboolean changed = FALSE;
  gchar *prop;
  GVariant *val;

  while(g_variant_iter_next(piter, "{&sv}", &prop, &val))
  {
    if(!g_strcmp0(prop, "Name"))
      changed |= bz_device_property_string(&device->name, val);
    else if(!g_strcmp0(prop, "Icon"))
      changed |= bz_device_property_string(&device->icon, val);
    else if(!g_strcmp0(prop, "Address"))
      changed |= bz_device_property_string(&device->addr, val);
    else if(!g_strcmp0(prop, "Paired") &&
        device->paired != g_variant_get_boolean(val))
    {
      device->paired = g_variant_get_boolean(val);
      changed = TRUE;
    }
    else if(!g_strcmp0(prop, "Trusted") &&
        device->trusted != g_variant_get_boolean(val))
    {
      device->trusted = g_variant_get_boolean(val);
      changed = TRUE;
    }
    else if(!g_strcmp0(prop, "Connected") &&
        device->connected != g_variant_get_boolean(val))
    {
      device->connected = g_variant_get_boolean(val);
      changed = TRUE;
    }
    g_variant_unref(val);
  }

  return changed;
}

static void bz_device_handle ( gchar *path, gchar *iface, GVariantIter *piter )
{
  BzDevice *device;

  device = devices?g_hash_table_lookup(devices, path):NULL;

  if(!device)
  {
    device = g_malloc0(sizeof(BzDevice));
    device->path = g_strdup(path);
    if(!devices)
      devices = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
          (GDestroyNotify)bz_device_free);

    g_hash_table_insert(devices, device->path, device);
  }

  bz_device_properties (device, piter);
  module_queue_append(&update_q, device);

  g_debug("bluez: device added: %d %d %s %s on %s",device->paired,
      device->connected, device->addr, device->name, device->path);
}

static void bz_adapter_handle ( gchar *object, gchar *iface )
{
  BzAdapter *adapter;
  GList *iter;


  for(iter=adapters; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((BzAdapter *)(iter->data))->path, object))
      return;

  adapter = g_malloc0(sizeof(BzAdapter));
  adapter->path = g_strdup(object);
  adapter->iface = g_strdup(iface);

  adapters = g_list_append(adapters, adapter);
  if(adapters && !g_list_next(adapters))
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("bluez_running"));
}

static void bz_object_handle ( gchar *object, GVariantIter *iiter )
{
  GVariantIter *dict;
  gchar *iface;

  while(g_variant_iter_next(iiter, "{&sa{sv}}", &iface, &dict))
  {
    if(strstr(iface,"Device"))
      bz_device_handle(object, iface, dict);
    else if(strstr(iface,"Adapter"))
      bz_adapter_handle(object, iface);
    g_variant_iter_free(dict);
  }
  g_variant_iter_free(iiter);
}

static void bz_device_new ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariantIter *iiter;
  gchar *object;

  g_variant_get(params, "(&oa{sa{sv}})", &object, &iiter);
  bz_object_handle(object, iiter);
}

static void bz_device_removed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  gchar *object;
  BzDevice *device;

  g_variant_get(params, "(&o@as)", &object, NULL);
  bz_adapter_free(object);

  device = g_hash_table_lookup(devices, object);
  if(device)
  {
    g_debug("bluez: device removed: %d %d %s %s on %s",device->paired,
        device->connected, device->addr, device->name, device->path);
    module_queue_append(&remove_q, device->path);
  }
}

static void bz_device_changed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  BzDevice *device;
  GVariantIter *piter;

  if(!devices)
    return;
  device = g_hash_table_lookup(devices, path);
  if(!device)
    return;

  g_variant_get(params,"(&sa{sv}@as)", NULL, &piter, NULL);
  if(bz_device_properties(device, piter))
  {
    g_debug("bluez: device changed: %d %d %s %s on %s",device->paired,
        device->connected, device->addr, device->name, device->path);
    module_queue_append(&update_q, device);
  }
  g_variant_iter_free(piter);
}

static void bz_init_cb ( GDBusConnection *con, GAsyncResult *res, gpointer data )
{
  GVariant *result;
  GVariantIter *miter, *iiter;
  gchar *path;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
    return;

  g_variant_get(result, "(a{oa{sa{sv}}})", &miter);
  while(g_variant_iter_next(miter, "{&oa{sa{sv}}}", &path, &iiter))
    bz_object_handle(path, iiter);
  g_variant_iter_free(miter);
  g_variant_unref(result);
}

static void bz_name_appeared_cb (GDBusConnection *con, const gchar *name,
    const gchar *owner, gpointer d)
{
  sub_add = g_dbus_connection_signal_subscribe(con, owner,
      "org.freedesktop.DBus.ObjectManager", "InterfacesAdded", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, bz_device_new, NULL, NULL);
  sub_del = g_dbus_connection_signal_subscribe(con, owner,
      "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, bz_device_removed, NULL, NULL);
  sub_chg = g_dbus_connection_signal_subscribe(con, owner,
      "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL,
      "org.bluez.Device1",  G_DBUS_SIGNAL_FLAGS_NONE, bz_device_changed,
      NULL, NULL);
  g_dbus_connection_call(bz_con, bz_serv, "/org/bluez",
      "org.bluez.AgentManager1", "RegisterAgent",
      g_variant_new("(os)", "/org/hosers/sfwbar", "NoInputNoOutput"),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  g_dbus_connection_call(con, bz_serv,"/",
      "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL,
      G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)bz_init_cb, NULL);
}

static void bz_name_disappeared_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
  while(adapters)
    bz_adapter_free(adapters->data);
  g_hash_table_remove_all(devices);
  g_dbus_connection_signal_unsubscribe(bz_con, sub_add);
  g_dbus_connection_signal_unsubscribe(bz_con, sub_del);
  g_dbus_connection_signal_unsubscribe(bz_con, sub_chg);
}

gboolean sfwbar_module_init ( void )
{
  if( !(bz_con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL)) )
    return FALSE;

  update_q.trigger = g_intern_static_string("bluez_updated");
  remove_q.trigger = g_intern_static_string("bluez_removed");

  g_bus_watch_name(G_BUS_TYPE_SYSTEM, bz_serv, G_BUS_NAME_WATCHER_FLAGS_NONE,
      bz_name_appeared_cb, bz_name_disappeared_cb, NULL, NULL);

  return TRUE;
}

static void *bz_expr_get ( void **params, void *widget, void *event )
{
  gchar *result = NULL;

  if(!params || !params[0])
    return g_strdup("");

  if( (result = module_queue_get_string(&update_q, params[0])) )
    return result;
  if( (result = module_queue_get_string(&remove_q, params[0])) )
    return result;

  return strdup("");
}

static void *bz_expr_state ( void **params, void *widget, void *event )
{
  gdouble *result;

  if(params && params[0])
  {
    if(!g_ascii_strcasecmp(params[0], "Running"))
    {
      result = g_malloc0(sizeof(gdouble));
      *result = !!g_atomic_pointer_get(&adapters);
      return result;
    }

    if( (result = module_queue_get_numeric(&update_q, params[0])) )
      return result;
  }

  return g_malloc0(sizeof(double));
}

static void bz_action_ack ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  module_queue_remove(&update_q);
}

static void bz_action_ack_removed ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  module_queue_remove(&remove_q);
}

static void bz_action_scan ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  bz_scan(bz_con, 10000);
}

static void bz_action_connect ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  BzDevice *device;

  if(!devices)
    return;

  device = g_hash_table_lookup(devices, cmd);
  if(!device || device->connected)
    return;
  bz_connect(device);
}

static void bz_action_pair ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  BzDevice *device;

  if(!devices)
    return;

  device = g_hash_table_lookup(devices, cmd);
  if(device)
    bz_pair(device);
}

static void bz_action_disconnect ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  BzDevice *device;

  if(!devices)
    return;

  device = g_hash_table_lookup(devices, cmd);
  if(device)
    bz_disconnect(device);
}

static void bz_action_remove ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  BzDevice *device;

  if(!devices)
    return;

  device = g_hash_table_lookup(devices, cmd);
  if(device)
    bz_remove(device);
}

ModuleExpressionHandlerV1 get_handler = {
  .flags = 0,
  .name = "BluezGet",
  .parameters = "S",
  .function = bz_expr_get
};

ModuleExpressionHandlerV1 state_handler = {
  .flags = MODULE_EXPR_NUMERIC,
  .name = "BluezState",
  .parameters = "S",
  .function = bz_expr_state
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &get_handler,
  &state_handler,
  NULL
};

ModuleActionHandlerV1 ack_handler = {
  .name = "BluezAck",
  .function = bz_action_ack
};

ModuleActionHandlerV1 ack_removed_handler = {
  .name = "BluezAckRemoved",
  .function = bz_action_ack_removed
};

ModuleActionHandlerV1 scan_handler = {
  .name = "BluezScan",
  .function = bz_action_scan
};

ModuleActionHandlerV1 connect_handler = {
  .name = "BluezConnect",
  .function = bz_action_connect
};

ModuleActionHandlerV1 pair_handler = {
  .name = "BluezPair",
  .function = bz_action_pair
};

ModuleActionHandlerV1 remove_handler = {
  .name = "BluezRemove",
  .function = bz_action_remove
};

ModuleActionHandlerV1 disconnect_handler = {
  .name = "BluezDisconnect",
  .function = bz_action_disconnect
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &ack_handler,
  &ack_removed_handler,
  &scan_handler,
  &connect_handler,
  &disconnect_handler,
  &pair_handler,
  &remove_handler,
  NULL
};
