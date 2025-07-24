/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include <glib.h>
#include <gio/gio.h>
#include "module.h"
#include "trigger.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;

static GHashTable *devices;
static GList *adapters;
static GDBusConnection *bz_con;
static const gchar *bz_serv = "org.bluez";
static guint sub_add, sub_del, sub_chg, sub_adapter_chg;

typedef struct _bz_device {
  gchar *path;
  gchar *addr;
  gchar *name;
  gchar *icon;
  guint32 class;
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
  gboolean discoverable;
  gboolean discovering;
  gboolean powered;
  GCancellable *cancel;
} BzAdapter;

static gchar *bz_major_class[] = {
  "Miscellaneous",
  "Computer",
  "Phone",
  "LAN / Network Access Point",
  "Audio/Video",
  "Peripheral",
  "Imaging",
  "Wearable",
  "Toy",
  "Health"
};

typedef struct _bz_minor_class {
  guint32 mask, id;
  gchar *name;
} bz_minor_class_t;

static bz_minor_class_t bz_minor_class[] = {
  { 0b01111111111100, 0b00000100000100, "Desktop" },
  { 0b01111111111100, 0b00000100001000, "Server" },
  { 0b01111111111100, 0b00000100001100, "Laptop" },
  { 0b01111111111100, 0b00000100010000, "Handheld" },
  { 0b01111111111100, 0b00000100010100, "Palm" },
  { 0b01111111111100, 0b00000100011000, "Wearable" },

  { 0b01111111111100, 0b00001000000100, "Cellular" },
  { 0b01111111111100, 0b00001000001000, "Cordless" },
  { 0b01111111111100, 0b00001000001100, "Smartphone" },
  { 0b01111111111100, 0b00001000010000, "Modem" },
  { 0b01111111111100, 0b00001000010100, "ISDN" },

  { 0b01111111100000, 0b00001100000000, "Full" },
  { 0b01111111100000, 0b00001100100000, "1-17%" },
  { 0b01111111100000, 0b00001101100000, "17-33%" },
  { 0b01111111100000, 0b00001110000000, "33-50%" },
  { 0b01111111100000, 0b00001110000000, "50-67%" },
  { 0b01111111100000, 0b00001110100000, "67-83%" },
  { 0b01111111100000, 0b00001111000000, "83-99%" },
  { 0b01111111100000, 0b00001111100000, "No Service" },

  { 0b01111111111100, 0b00010000000100, "Headset" },
  { 0b01111111111100, 0b00010000001000, "Hands-free" },
  { 0b01111111111100, 0b00010000001100, "Unknown" },
  { 0b01111111111100, 0b00010000010000, "Microphone" },
  { 0b01111111111100, 0b00010000010100, "Loudspeakers" },
  { 0b01111111111100, 0b00010000011000, "Headphones" },
  { 0b01111111111100, 0b00010000011100, "Portable Audio" },
  { 0b01111111111100, 0b00010000100000, "Car Audio" },
  { 0b01111111111100, 0b00010000100100, "Set-top box" },
  { 0b01111111111100, 0b00010000101000, "HiFi" },
  { 0b01111111111100, 0b00010000101100, "VCR" },
  { 0b01111111111100, 0b00010000110000, "Video Camera" },
  { 0b01111111111100, 0b00010000110100, "Camcoder" },
  { 0b01111111111100, 0b00010000111000, "Video Monitor" },
  { 0b01111111111100, 0b00010000111100, "Video display and loudspeaker" },
  { 0b01111111111100, 0b00010001000000, "Video Conferencing" },
  { 0b01111111111100, 0b00010001001000, "Gaming/Toy" },

  { 0b01111111000000, 0b00010101000000, "Keyboard" },
  { 0b01111111000000, 0b00010110000000, "Pointer" },
  { 0b01111111000000, 0b00010111000000, "Combo" },

  { 0b01111100000000, 0b00011000000000, "Imaging" },

  { 0b01111111111100, 0b00011100000100, "Watch" },
  { 0b01111111111100, 0b00011100001000, "Jacket" },
  { 0b01111111111100, 0b00011100001100, "Helmet" },
  { 0b01111111111100, 0b00011100010000, "Glasses" },

  { 0b01111111111100, 0b00100000000100, "Robot" },
  { 0b01111111111100, 0b00100000001000, "Vehicle" },
  { 0b01111111111100, 0b00100000001100, "Doll" },
  { 0b01111111111100, 0b00100000010000, "Controller" },
  { 0b01111111111100, 0b00100000010100, "Game" },

  { 0b01111111111100, 0b00100100000100, "Blood pressure monitor" },
  { 0b01111111111100, 0b00100100001000, "Thermometer" },
  { 0b01111111111100, 0b00100100001100, "Scale" },
  { 0b01111111111100, 0b00100100010000, "Glucose Meter" },
  { 0b01111111111100, 0b00100100010100, "Oxymeter" },
  { 0b01111111111100, 0b00100100011000, "Heart rate monitor" },
  { 0b01111111111100, 0b00100100011100, "Health data display" },

  { 0, 0, NULL },

};

static gchar *bz_get_major_class ( guint class )
{
  guint i =(class>>8) & 0x1F;

  if(i<10)
    return bz_major_class[i];
  else
    return "Unknown";
}

static gchar *bz_get_minor_class ( guint32 class )
{
  gint i;

  for(i=0; bz_minor_class[i].name; i++)
    if((class & bz_minor_class[i].mask) == bz_minor_class[i].id)
      return bz_minor_class[i].name;

  return "Unknown";
}

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
    trigger_emit("bluez_running");
  g_cancellable_cancel(adapter->cancel);
  if(adapter->timeout_handle)
    g_source_remove(adapter->timeout_handle);
  g_free(adapter->path);
  g_free(adapter->iface);
  g_free(adapter);
}

static void bz_device_free ( BzDevice *device )
{
  g_debug("bluez: device removed: %d %d %s %s on %s",device->paired,
      device->connected, device->addr, device->name, device->path);
  trigger_emit_with_string("bluez_removed", "path", g_strdup(device->path));
  g_free(device->path);
  g_free(device->addr);
  g_free(device->name);
  g_free(device->icon);
  g_free(device);
}

static BzAdapter *bz_adapter_get ( void )
{
  if(!adapters)
    return NULL;
  return adapters->data;
}

static gboolean bz_scan_stop ( BzAdapter *adapter )
{
  g_debug("bluez: scan off");
  g_dbus_connection_call(bz_con, bz_serv, adapter->path,
      adapter->iface, "StopDiscovery", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
      -1, NULL, NULL, NULL);
  adapter->timeout_handle = 0;
  return FALSE;
}

static void bz_scan_cb ( GDBusConnection *con, GAsyncResult *res,
    BzAdapter *adapter)
{
  GVariant *result;

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
    return;

  g_variant_unref(result);

  if(adapter->scan_timeout)
    adapter->timeout_handle = g_timeout_add(adapter->scan_timeout,
        (GSourceFunc)bz_scan_stop, adapter);
}

static void bz_scan ( GDBusConnection *con, guint timeout )
{
  BzAdapter *adapter;

  if( !(adapter = bz_adapter_get()) || adapter->timeout_handle)
    return;

  adapter->scan_timeout = timeout;

  g_debug("bluez: scan on");
  g_dbus_connection_call(con, bz_serv, adapter->path,
      adapter->iface, "StartDiscovery", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
      -1, NULL, (GAsyncReadyCallback)bz_scan_cb, adapter);
}

static void bz_set_boolean  ( GDBusConnection *con, gchar *prop, gboolean val )
{
  BzAdapter *adapter;

  if( !(adapter = bz_adapter_get()) )
    return;

  g_debug("bluez: set %s to %s", prop, val? "True" : "False");
  g_dbus_connection_call(con, bz_serv, adapter->path,
      "org.freedesktop.DBus.Properties", "Set",
      g_variant_new("(ssv)", adapter->iface, prop, g_variant_new_boolean(val)),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, adapter);
}

static void bz_connect_cb ( GDBusConnection *con, GAsyncResult *res,
    BzDevice *device)
{
  GVariant *result;

  device->connecting =  FALSE;
  trigger_emit_with_string("bluez_updated", "path", g_strdup(device->path));
  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
    return;

  g_debug("bluez: connected %s (%s)", device->addr, device->name);
  g_variant_unref(result);
}

static void bz_connect ( BzDevice *device )
{
  if(!device->connecting)
  {
    device->connecting = TRUE;
    trigger_emit_with_string("bluez_updated", "path", g_strdup(device->path));
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

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
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

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    device->connecting =  FALSE;
    trigger_emit_with_string("bluez_updated", "path", g_strdup(device->path));
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

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    device->connecting =  FALSE;
    trigger_emit_with_string("bluez_updated", "path", g_strdup(device->path));
    return;
  }

  g_debug("bluez: paired %s (%s)", device->addr, device->name);
  bz_trust(device);
  g_variant_unref(result);
}

static void bz_pair ( BzDevice *device )
{
  device->connecting =  TRUE;
  trigger_emit_with_string("bluez_updated", "path", g_strdup(device->path));

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

  if( (result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    g_debug("bluez: removed %s", name);
    g_variant_unref(result);
  }
  g_free(name);
}

static void bz_remove ( BzDevice *device )
{
  BzAdapter *adapter;

  if( !(adapter = bz_adapter_get()) )
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
    else if(!g_strcmp0(prop, "Class") &&
        device->class != g_variant_get_uint32(val))
    {
      device->class = g_variant_get_uint32(val);
      changed = TRUE;
    }
    g_variant_unref(val);
  }

  return changed;
}

static void bz_device_handle ( gchar *path, gchar *iface, GVariantIter *piter )
{
  BzDevice *device;

  if( !(device = devices? g_hash_table_lookup(devices, path) : NULL) )
  {
    device = g_malloc0(sizeof(BzDevice));
    device->path = g_strdup(path);
    if(!devices)
      devices = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
          (GDestroyNotify)bz_device_free);

    g_hash_table_insert(devices, device->path, device);
  }

  bz_device_properties (device, piter);
  trigger_emit_with_string("bluez_updated", "path", g_strdup(device->path));

  g_debug("bluez: device added: %d %d %s %s on %s",device->paired,
      device->connected, device->addr, device->name, device->path);
}

static void bz_device_removed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  gchar *object;

  g_variant_get(params, "(&o@as)", &object, NULL);
  bz_adapter_free(object);
  if(devices)
    g_hash_table_remove(devices, object);
}

static void bz_device_changed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  BzDevice *device;
  GVariantIter *piter;

  if( !devices || !(device = g_hash_table_lookup(devices, path)) )
    return;

  g_variant_get(params,"(&sa{sv}@as)", NULL, &piter, NULL);
  if(bz_device_properties(device, piter))
  {
    g_debug("bluez: device changed: %d %d %s %s on %s",device->paired,
        device->connected, device->addr, device->name, device->path);
    trigger_emit_with_string("bluez_updated", "path", g_strdup(device->path));
  }
  g_variant_iter_free(piter);
}

static void bz_adapter_update ( const gchar *path, GVariant *dict )
{
  BzAdapter *adapter;
  gboolean state;

  if( !(adapter = bz_adapter_get()) || g_strcmp0(adapter->path, path) )
    return;

  if(g_variant_lookup(dict, "Discovering", "b", &state))
    adapter->discovering = state;
  if(g_variant_lookup(dict, "Discoverable", "b", &state))
    adapter->discoverable = state;
  if(g_variant_lookup(dict, "Powered", "b", &state))
    adapter->powered = state;

  trigger_emit("bluez_adapter");
}

static void bz_adapter_prop_cb ( GDBusConnection *con, GAsyncResult *res,
    BzAdapter *adapter )
{
  GVariant *result, *dict;

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
    return;
  g_variant_get(result,"(@a{sv})", &dict);
  bz_adapter_update(adapter->path, dict);
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
  adapter->cancel = g_cancellable_new();

  g_dbus_connection_call(bz_con, bz_serv, object,
      "org.freedesktop.DBus.Properties", "GetAll", g_variant_new("(s)", iface),
      G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, -1, adapter->cancel,
      (GAsyncReadyCallback)bz_adapter_prop_cb, adapter);

  adapters = g_list_append(adapters, adapter);
  if(adapters && !g_list_next(adapters))
    trigger_emit("bluez_running");
}

static void bz_adapter_changed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariant *dict;

  g_variant_get(params, "(&s@a{sv}@as)", NULL, &dict, NULL);
  bz_adapter_update(path, dict);
  g_variant_unref(dict);
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

static void bz_init_cb ( GDBusConnection *con, GAsyncResult *res, gpointer data )
{
  GVariant *result;
  GVariantIter *miter, *iiter;
  gchar *path;

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
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
  sub_adapter_chg = g_dbus_connection_signal_subscribe(con, owner,
      "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL,
      "org.bluez.Adapter1",  G_DBUS_SIGNAL_FLAGS_NONE, bz_adapter_changed,
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
  g_dbus_connection_signal_unsubscribe(bz_con, sub_add);
  g_dbus_connection_signal_unsubscribe(bz_con, sub_del);
  g_dbus_connection_signal_unsubscribe(bz_con, sub_chg);
  g_dbus_connection_signal_unsubscribe(bz_con, sub_adapter_chg);
  if(devices)
    g_hash_table_remove_all(devices);
  g_list_free_full(adapters,(GDestroyNotify)bz_adapter_free);
  adapters = NULL;
}

static value_t bz_action_scan ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 0, 1, "BluezScan");
  bz_scan(bz_con, np? value_as_numeric(p[0])*1000 : 10000);

  return value_na;
}

static value_t bz_action_power ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 0, 1, "BluezPower");
  bz_set_boolean(bz_con, "Powered", value_as_numeric(p[0]));

  return value_na;
}

static value_t bz_action_discoverable ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 0, 1, "BluezDiscoverable");
  bz_set_boolean(bz_con, "Discoverable", value_as_numeric(p[0]));

  return value_na;
}

static value_t bz_action_connect ( vm_t *vm, value_t p[], gint np )
{
  BzDevice *device;

  vm_param_check_np(vm, np, 1, "BluezConnect");
  vm_param_check_string(vm, p, 0, "BluezConnect");

  if(devices)
  {
    device = g_hash_table_lookup(devices, value_get_string(p[0]));
    if(device && !device->connected)
      bz_connect(device);
  }

  return value_na;
}

static value_t bz_action_pair ( vm_t *vm, value_t p[], gint np )
{
  BzDevice *device;

  vm_param_check_np(vm, np, 1, "BluezPair");
  vm_param_check_string(vm, p, 0, "BluezPair");

  if(devices && (device=g_hash_table_lookup(devices, value_get_string(p[0]))) )
    bz_pair(device);

  return value_na;
}

static value_t bz_action_disconnect ( vm_t *vm, value_t p[], gint np )
{
  BzDevice *device;

  vm_param_check_np(vm, np, 1, "BluezDisconnect");
  vm_param_check_string(vm, p, 0, "BluezDisconnect");

  if(devices && (device=g_hash_table_lookup(devices, value_get_string(p[0]))) )
    bz_disconnect(device);

  return value_na;
}

static value_t bz_action_remove ( vm_t *vm, value_t p[], gint np )
{
  BzDevice *device;

  vm_param_check_np(vm, np, 1, "BluezRemove");
  vm_param_check_string(vm, p, 0, "BluezRemove");

  if(devices && (device=g_hash_table_lookup(devices, value_get_string(p[0]))) )
    bz_remove(device);

  return value_na;
}

static value_t bz_expr_adapter ( vm_t *vm, value_t p[], gint np )
{
  BzAdapter *adapter;

  vm_param_check_np(vm, np, 1, "BluezAdapter");
  vm_param_check_string(vm, p, 0, "BluezAdapter");

  if( !(adapter = bz_adapter_get()) )
    return value_na;

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "count"))
    return value_new_numeric(g_list_length(adapters));
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "discovering"))
    return value_new_numeric(adapter->discovering);
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "discoverable"))
    return value_new_numeric(adapter->discoverable);
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "powered"))
    return value_new_numeric(adapter->powered);

  return value_na;
}

static value_t bz_expr_device ( vm_t *vm, value_t p[], gint np )
{
  BzDevice *device;

  vm_param_check_np(vm, np, 2, "BluezDevice");
  vm_param_check_string(vm, p, 0, "BluezDevice");
  vm_param_check_string(vm, p, 1, "BluezDevice");

  if(!devices ||
      !(device = g_hash_table_lookup(devices, value_get_string(p[0]))))
    return value_na;

  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Name"))
    return value_new_string(g_strdup(device->name));
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Address"))
    return value_new_string(g_strdup(device->addr));
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Icon"))
    return value_new_string(g_strdup(device->icon));
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Path"))
    return value_new_string(g_strdup(device->path));
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "MajorClass"))
    return value_new_string(g_strdup(bz_get_major_class(device->class)));
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "MinorClass"))
    return value_new_string(g_strdup(bz_get_minor_class(device->class)));
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Paired"))
    return value_new_numeric(device->paired);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Connected"))
    return value_new_numeric(device->connected);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Connecting"))
    return value_new_numeric(device->connecting);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Trusted"))
    return value_new_numeric(device->trusted);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "Visible"))
  return value_new_numeric(device->name || ((device->class & 0x1F40)==0x0540));

  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  if( !(bz_con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL)) )
    return FALSE;

  vm_func_add("bluezscan", bz_action_scan, TRUE, TRUE);
  vm_func_add("bluezpower", bz_action_power, TRUE, TRUE);
  vm_func_add("bluezdiscoverable", bz_action_discoverable, TRUE, TRUE);
  vm_func_add("bluezconnect", bz_action_connect, TRUE, TRUE);
  vm_func_add("bluezpair", bz_action_pair, TRUE, TRUE);
  vm_func_add("bluezdisconnect", bz_action_disconnect, TRUE, TRUE);
  vm_func_add("bluezremove", bz_action_remove, TRUE, TRUE);
  vm_func_add("bluezadapter", bz_expr_adapter, FALSE, TRUE);
  vm_func_add("bluezdevice", bz_expr_device, FALSE, TRUE);

  g_bus_watch_name(G_BUS_TYPE_SYSTEM, bz_serv, G_BUS_NAME_WATCHER_FLAGS_NONE,
      bz_name_appeared_cb, bz_name_disappeared_cb, NULL, NULL);

  return TRUE;
}
