#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include "module.h"
#include "trigger.h"
#include "gui/popup.h"
#include "util/hash.h"
#include "util/string.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;

extern ModuleInterfaceV1 sfwbar_interface;

static void iw_signal_level_agent_init ( gchar *path );
static void iw_scan_start ( gchar *path );

static const gchar *iw_serv = "net.connman.iwd";
static GDBusConnection *iw_con;
static GList *iw_devices;
static hash_table_t *iw_networks, *iw_known_networks;
static GRecMutex device_mutex;
static gint sub_add, sub_del, sub_chg;
static gchar *iw_owner;

static const gchar *iw_iface_device = "net.connman.iwd.Device";
static const gchar *iw_iface_station = "net.connman.iwd.Station";
static const gchar *iw_iface_network = "net.connman.iwd.Network";
static const gchar *iw_iface_known = "net.connman.iwd.KnownNetwork";

static const gchar iw_signal_level_agent_xml[] =
  "<node>"
  " <interface name='net.connman.iwd.SignalLevelAgent'>"
  "  <method name='Release'>"
  "   <arg type='o' name='device' direction='in'/>"
  "  </method>"
  "  <method name='Changed'>"
  "   <arg type='o' name='device' direction='in'/>"
  "   <arg type='y' name='level' direction='in'/>"
  "  </method>"
  " </interface>"
  "</node>";

static const gchar iw_agent_xml[] =
  "<node>"
  " <interface name='net.connman.iwd.Agent'>"
  "  <method name='Release'/>"
  "  <method name='RequestPassphrase'>"
  "   <arg type='o' name='network' direction='in'/>"
  "   <arg type='s' name='passphrase' direction='out'/>"
  "  </method>"
  "  <method name='RequestPrivateKeyPassphrase'>"
  "   <arg type='o' name='network' direction='in'/>"
  "   <arg type='s' name='passphrase' direction='out'/>"
  "  </method>"
  "  <method name='RequestUserNameAndPassword'>"
  "   <arg type='o' name='network' direction='in'/>"
  "   <arg type='s' name='username' direction='out'/>"
  "   <arg type='s' name='password' direction='out'/>"
  "  </method>"
  "  <method name='RequestUserPassword'>"
  "   <arg type='o' name='network' direction='in'/>"
  "   <arg type='s' name='user' direction='in'/>"
  "   <arg type='s' name='password' direction='out'/>"
  "  </method>"
  "  <method name='Cancel'>"
  "   <arg type='s' name='reason' direction='in'/>"
  "  </method>"
  " </interface>"
  "</node>";

typedef struct _iw_dialog {
  GDBusMethodInvocation *invocation;
  GtkWidget *win, *title, *user, *pass, *ok, *cancel;
} iw_dialog_t;

typedef struct _iw_device {
  gchar *path;
  gchar *name;
  gchar *state;
  gchar *conn_net;
  gint strength;
  gboolean scanning;
} iw_device_t;

typedef struct _iw_known {
  gchar *path;
  gchar *ssid;
  gchar *type;
  gchar *last_time;
  gboolean hidden;
  gboolean auto_conn;
} iw_known_t;

typedef struct _iw_network {
  gchar *path;
  gchar *ssid;
  gchar *type;
  gchar *known;
  gchar *device;
  gint16 strength;
  gboolean connected;
} iw_network_t;

static gboolean iw_array_lookup ( GVariant *dict, gchar *key )
{
  GVariantIter iter;
  gchar *name;

  g_variant_iter_init(&iter, dict);
  while(g_variant_iter_next(&iter, "&s", &name))
    if(!g_strcmp0(key, name))
      return TRUE;

  return FALSE;
}

static gboolean iw_string_from_dict ( GVariant *dict, gchar *key,
    gchar *format, gchar **str )
{
  gchar *tmp;

  if(!str || !dict || !key || !format)
    return FALSE;

  if(g_variant_check_format_string(dict, "a{sv}", FALSE))
  {
    if(!g_variant_lookup(dict, key, format, &tmp) || !g_strcmp0(*str, tmp))
      return FALSE;
  }
  else if(g_variant_check_format_string(dict, "as", FALSE))
  {
    if(!iw_array_lookup(dict, key) || !*str)
      return FALSE;
    tmp = NULL;
  }
  else
    return FALSE;

  str_assign(str, g_strdup(tmp));
  return TRUE;
}

static gboolean iw_bool_from_dict ( GVariant *dict, gchar *key, gboolean *v )
{
  gboolean tmp;

  if(!dict || !key || !v)
    return FALSE;

  if(g_variant_check_format_string(dict,"a{sv}",FALSE))
  {
    if(!g_variant_lookup(dict, key, "b", &tmp) || *v==tmp)
      return FALSE;
  }
  else if(g_variant_check_format_string(dict,"as",FALSE))
  {
    if(!iw_array_lookup(dict, key) || !*v)
      return FALSE;
    tmp = FALSE;
  }
  else
    return FALSE;

  *v = tmp;
  return TRUE;
}

static void iw_known_network_free ( iw_known_t *known )
{
  g_free(known->path);
  g_free(known->ssid);
  g_free(known->type);
  g_free(known->last_time);
  g_free(known);
}

static void iw_known_network_remove ( iw_known_t *known )
{
  if(!known)
    return;

  g_debug("iwd: remove known network: %s", known->ssid);

  if(!known->path)
    return;

  hash_table_remove(iw_known_networks, known);
}

static iw_known_t *iw_known_network_get ( gchar *path, gboolean create )
{
  iw_known_t *known;

  if(!path)
    return NULL;

  known = hash_table_lookup(iw_known_networks, path);

  if(known)
    return known;

  if(!create)
    return NULL;

  known = g_malloc0(sizeof(iw_known_t));
  known->path = g_strdup(path);

  hash_table_insert(iw_known_networks, known->path, known);
  return known;
}

static void iw_network_free ( iw_network_t *network )
{
  g_free(network->path);
  g_free(network->ssid);
  g_free(network->type);
  g_free(network->device);
  g_free(network->known);
  g_free(network);
}

static void iw_network_updated ( iw_network_t *net )
{
  if(net)
  {
    trigger_emit_with_string("wifi_updated", "id", g_strdup(net->path));
    g_debug("iwd: network: %s, type: %s, conn: %d, known: %s, strength: %d",
        net->ssid, net->type, net->connected, net->known, net->strength);
  }
}

static void iw_network_remove ( iw_network_t *network )
{
  if(!network)
    return;

  g_debug("iwd: remove network: %s", network->ssid);
  trigger_emit_with_string("wifi_removed", "id", g_strdup(network->path));

  if(network->path)
    hash_table_remove(iw_networks, network->path);
}

static iw_network_t *iw_network_get ( gchar *path, gboolean create )
{
  iw_network_t *net;

  if(!create && !path)
    return NULL;

  if( (net = hash_table_lookup(iw_networks, path)) )
    return net;

  net = g_malloc0(sizeof(iw_network_t));
  net->path = g_strdup(path);

  hash_table_insert(iw_networks, net->path, net);
  return net;
}

static iw_device_t *iw_device_get ( gchar *path, gboolean create )
{
  iw_device_t *device;
  GList *iter;

  g_rec_mutex_lock(&device_mutex);
  for(iter=iw_devices; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((iw_device_t *)iter->data)->path, path))
      break;
  g_rec_mutex_unlock(&device_mutex);

  if(iter)
    return iter->data;

  if(!create)
    return NULL;

  iw_signal_level_agent_init(path);
  iw_scan_start(path);
  device = g_malloc0(sizeof(iw_device_t));
  device->path = g_strdup(path);
  g_rec_mutex_lock(&device_mutex);
  iw_devices = g_list_prepend(iw_devices, device);
  g_rec_mutex_unlock(&device_mutex);
  return device;
}

static void iw_device_free ( iw_device_t *device )
{
  if(!device)
    return;

  g_debug("iwd: remove device: %s", device->name);

  g_rec_mutex_lock(&device_mutex);
  iw_devices = g_list_remove(iw_devices, device);
  g_rec_mutex_unlock(&device_mutex);
  g_free(device->path);
  g_free(device->name);
  g_free(device->state);
  g_free(device->conn_net);
  g_free(device);
}

static void iw_signal_level_agent_method(GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *method, GVariant *parameters,
    GDBusMethodInvocation *invocation, gpointer data)
{
  iw_device_t *device;
  gchar *object;
  guint8 level;

  if(!g_strcmp0(method, "Release"))
    g_dbus_method_invocation_return_value(invocation, NULL);
  else if(!g_strcmp0(method, "Changed"))
  {
    g_variant_get (parameters, "(&oy)", &object, &level);
    g_rec_mutex_lock(&device_mutex);
    if( (device = iw_device_get(object, FALSE)) )
        device->strength = level;
    
    g_debug("iwd: level %d on %s", level, device?device->name:object);
    g_rec_mutex_unlock(&device_mutex);
    trigger_emit("wifi_level");
    g_dbus_method_invocation_return_value(invocation, NULL);
  }
}

static void iw_signal_level_agent_init ( gchar *path )
{
  GVariantBuilder *builder;
  gint16 i;

  builder = g_variant_builder_new(G_VARIANT_TYPE("an"));
  for(i=-55; i>-100; i-=5)
    g_variant_builder_add(builder, "n", i);

  g_dbus_connection_call(iw_con, iw_serv, path, iw_iface_station,
      "RegisterSignalLevelAgent",
      g_variant_new("(oan)", "/org/hosers/sfwbar", builder),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  g_variant_builder_unref(builder);
}

static void iw_passphrase_cb ( GtkEntry *entry, iw_dialog_t *dialog )
{
  const gchar *pass;

  if(gtk_entry_get_text_length(GTK_ENTRY(dialog->pass))<8 ||
      (dialog->user && gtk_entry_get_text_length(GTK_ENTRY(dialog->user))<1))
    return;
  pass = gtk_entry_get_text(GTK_ENTRY(dialog->pass));
  g_dbus_method_invocation_return_value(dialog->invocation,
      g_variant_new("(s)", pass));
  gtk_widget_destroy(dialog->win);
  g_free(dialog);
}

static void iw_button_clicked ( GtkWidget *button, iw_dialog_t *dialog )
{
  if(button==dialog->ok)
    g_dbus_method_invocation_return_value(dialog->invocation,
        g_variant_new("(s)", gtk_entry_get_text(GTK_ENTRY(dialog->pass))));
  else
    g_dbus_method_invocation_return_dbus_error(dialog->invocation,
        "net.connman.iwd.Agent.Error.Canceled", "");
  gtk_widget_destroy(dialog->win);
  g_free(dialog);
}

static void iw_passphrase_changed_cb ( gpointer *w, iw_dialog_t *dialog )
{
  gtk_widget_set_sensitive(dialog->ok,
      gtk_entry_get_text_length(GTK_ENTRY(dialog->pass))>7 &&
      (!dialog->user || gtk_entry_get_text_length(GTK_ENTRY(dialog->user))>0));
}

static gboolean iw_passphrase_delete_cb ( GtkWidget *w, GdkEvent *ev,
    iw_dialog_t *dialog )
{
  g_dbus_method_invocation_return_dbus_error(dialog->invocation,
      "net.connman.iwd.Agent.Error.Canceled", "");
  g_free(dialog);
  return FALSE;
}

static void iw_passphrase_prompt ( gchar *title, gboolean user, gpointer inv )
{
  iw_dialog_t *dialog;
  GtkWidget *grid, *label;

  dialog = g_malloc0(sizeof(iw_dialog_t));

  dialog->invocation = inv;
  dialog->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint(GTK_WINDOW(dialog->win),
      GDK_WINDOW_TYPE_HINT_DIALOG);
  g_signal_connect(G_OBJECT(dialog->win), "delete-event",
    G_CALLBACK(iw_passphrase_delete_cb), dialog);
  grid = gtk_grid_new();
  gtk_widget_set_name(grid, "wifi_dialog_grid");
  gtk_container_add(GTK_CONTAINER(dialog->win), grid);
  label = gtk_label_new(title);
  g_free(title);
  gtk_widget_set_name(label, "wifi_dialog_title");
  gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 2, 1);

  if(user)
  {
    label = gtk_label_new("Username:");
    gtk_widget_set_name(label, "wifi_user_label");
    gtk_grid_attach(GTK_GRID(grid),label, 1, 2, 1, 1);
    dialog->user = gtk_entry_new();
    gtk_widget_set_name(dialog->user, "wifi_user_entry");
    gtk_grid_attach(GTK_GRID(grid), dialog->user, 2, 2, 1, 1);
    g_signal_connect(G_OBJECT(dialog->user), "changed",
        G_CALLBACK(iw_passphrase_changed_cb), dialog);
  }

  label = gtk_label_new("Passphrase:");
  gtk_widget_set_name(label, "wifi_passphrase_label");
  gtk_grid_attach(GTK_GRID(grid), label, 1, 3, 1, 1);
  dialog->pass = gtk_entry_new();
  gtk_widget_set_name(dialog->pass, "wifi_passphrase_entry");
  gtk_entry_set_visibility(GTK_ENTRY(dialog->pass), FALSE);
  g_signal_connect(G_OBJECT(dialog->pass), "activate",
      G_CALLBACK(iw_passphrase_cb), dialog);
    g_signal_connect(G_OBJECT(dialog->pass), "changed",
        G_CALLBACK(iw_passphrase_changed_cb), dialog);
  gtk_grid_attach(GTK_GRID(grid), dialog->pass, 2, 3, 1, 1);

  dialog->ok = gtk_button_new_with_label("Ok");
  gtk_widget_set_name(dialog->ok, "wifi_button_ok");
  gtk_grid_attach(GTK_GRID(grid), dialog->ok, 1, 4, 1, 1);
  gtk_widget_set_sensitive(dialog->ok, FALSE);
  g_signal_connect(G_OBJECT(dialog->ok), "clicked",
      G_CALLBACK(iw_button_clicked), dialog);

  dialog->cancel = gtk_button_new_with_label("Cancel");
  gtk_widget_set_name(dialog->cancel, "wifi_button_cancel");
  gtk_grid_attach(GTK_GRID(grid), dialog->cancel, 2, 4, 1, 1);
  g_signal_connect(G_OBJECT(dialog->cancel), "clicked",
      G_CALLBACK(iw_button_clicked), dialog);

  g_object_ref_sink(G_OBJECT(dialog->win));
  popup_popdown_autoclose();
  gtk_widget_show_all(dialog->win);
}

static void iw_agent_method(GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *method, GVariant *parameters,
    GDBusMethodInvocation *invocation, gpointer data)
{
  iw_network_t *net;
  gchar *object;

  hash_table_lock(iw_networks);
  if(!g_strcmp0(method, "Release"))
    g_dbus_method_invocation_return_value(invocation, NULL);
  else if(!g_strcmp0(method, "RequestPassphrase"))
  {
    g_variant_get(parameters, "(&o)", &object);
    if( (net = iw_network_get(object, FALSE)) )
      iw_passphrase_prompt(
          g_strdup_printf("Passphrase for network %s", net->ssid),
          FALSE, invocation);
  }
  else if(!g_strcmp0(method, "RequestPrivateKeyPassphrase"))
  {
    g_variant_get(parameters, "(&o)", &object);
    if( (net = iw_network_get(object, FALSE)) )
      iw_passphrase_prompt(
          g_strdup_printf("Passphrase for private key for network %s",
            net->ssid), FALSE, invocation);
  }
  else if(!g_strcmp0(method, "RequestUserNameAndPassword"))
  {
    g_variant_get(parameters, "(&o)", &object);
    if( (net = iw_network_get(object, FALSE)) )
      iw_passphrase_prompt(
          g_strdup_printf("Credentials for network %s", net->ssid),
          TRUE, invocation);
  }
  hash_table_unlock(iw_networks);
}

static void iw_scan_start ( gchar *path )
{
  iw_device_t *device;

  g_rec_mutex_lock(&device_mutex);
  device = iw_device_get(path, FALSE);
  if(device && !device->scanning)
  {
    g_debug("iwd: initiating scan");
    trigger_emit("wifi_scan");
    g_dbus_connection_call(iw_con, iw_serv, path, iw_iface_station, "Scan",
      NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  }
  g_rec_mutex_unlock(&device_mutex);
}

static void iw_network_connect_cb ( GDBusConnection *con, GAsyncResult *res,
    gchar *path )
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result)
    g_variant_unref(result);

  hash_table_lock(iw_networks);
  iw_network_updated(iw_network_get(path, FALSE));
  hash_table_unlock(iw_networks);
  g_free(path);
}

static void iw_network_connect ( gchar *path )
{
  if(!path)
    return;

  g_dbus_connection_call(iw_con, iw_serv, path, iw_iface_network, "Connect",
      NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)iw_network_connect_cb, g_strdup(path));
}

static void iw_network_disconnect ( gchar *path )
{
  GList *iter;
  iw_device_t *device;

  g_rec_mutex_lock(&device_mutex);
  for(iter=iw_devices; iter; iter=g_list_next(iter))
  {
    device = iter->data;
    if(!g_strcmp0(device->conn_net, path))
      g_dbus_connection_call(iw_con, iw_serv, device->path, iw_iface_station,
          "Disconnect", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL,
          NULL);
  }
  g_rec_mutex_unlock(&device_mutex);
}

static void iw_network_forget ( gchar *path )
{
  iw_network_t *net;
  iw_known_t *known;

  if( !(net = iw_network_get(path, FALSE)) )
    return;

  if( (known = iw_known_network_get(net->known, FALSE)) )
    g_dbus_connection_call(iw_con, iw_serv, known->path, iw_iface_known,
        "Forget", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);

  iw_network_disconnect(path);
}

static void iw_network_strength_cb ( GDBusConnection *con, GAsyncResult *res,
    gpointer data )
{
  iw_network_t *net;
  GVariant *result;
  GVariantIter *iter;
  gchar *path;
  gint16 strength;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
    return;

  g_variant_get(result, "(a(on))", &iter);
  hash_table_lock(iw_networks);
  while(g_variant_iter_next(iter, "(&on)", &path, &strength))
    if( (net = iw_network_get(path, FALSE)) && net->strength!=strength)
    {
      net->strength = strength;
      iw_network_updated(net);
    }
  hash_table_unlock(iw_networks);

  g_variant_iter_free(iter);
  g_variant_unref(result);
}

static void iw_device_handle ( gchar *path, gchar *iface, GVariant *dict )
{
  iw_device_t *device;
  gboolean change = FALSE;

  g_rec_mutex_lock(&device_mutex);
  device = iw_device_get(path, TRUE);
  change |= iw_string_from_dict( dict, "Name", "&s", &device->name);

  if(change)
    g_debug("iwd: device: %s, state: %s", device->name, device->state);
  g_rec_mutex_unlock(&device_mutex);
}

static void iw_station_handle ( gchar *path, gchar *iface, GVariant *dict )
{
  iw_device_t *device;
  gboolean change = FALSE, scan;

  g_rec_mutex_lock(&device_mutex);
  device = iw_device_get(path, TRUE);
  change |= iw_string_from_dict(dict, "State", "&s", &device->state);
  change |= iw_string_from_dict(dict, "ConnectedNetwork", "&o",
      &device->conn_net);
  scan = iw_bool_from_dict(dict, "Scanning", &device->scanning);
  change |= scan;
  if(scan && !device->scanning)
  {
    trigger_emit("wifi_scan_complete");
    g_dbus_connection_call(iw_con, iw_serv, path, iw_iface_station,
        "GetOrderedNetworks", NULL, G_VARIANT_TYPE("(a(on))"),
        G_DBUS_CALL_FLAGS_NONE, -1, NULL,
        (GAsyncReadyCallback)iw_network_strength_cb, NULL);
  }
  if(change)
    g_debug("iwd: device: %s, state: %s, scanning: %d", device->name,
        device->state, device->scanning);
  g_rec_mutex_unlock(&device_mutex);
}

static void iw_known_network_handle ( gchar *path, gchar *iface, GVariant *dict )
{
  iw_known_t *known;
  gboolean change = FALSE;

  known = iw_known_network_get(path, TRUE);
  change |= iw_string_from_dict(dict, "Name", "&s", &known->ssid);
  change |= iw_string_from_dict(dict, "Type", "&s", &known->type);
  change |= iw_string_from_dict(dict, "LastConnectedTime", "&s",
      &known->last_time);
  change |= iw_bool_from_dict(dict, "AutoConnected", &known->auto_conn);
  change |= iw_bool_from_dict(dict, "Hidden", &known->hidden);

  if(change)
    g_debug("iwd: known: %s, type: %s, last conn: %s, hidden: %d, auto: %d",
        known->ssid, known->type, known->last_time, known->hidden,
        known->auto_conn);
}

static void iw_network_handle ( gchar *path, gchar *iface, GVariant *dict )
{
  iw_network_t *net;
  gboolean change = FALSE;

  hash_table_lock(iw_networks);
  net = iw_network_get(path, TRUE);
  change |= iw_string_from_dict(dict, "Name", "&s", &net->ssid);
  change |= iw_string_from_dict(dict, "Type", "&s", &net->type);
  change |= iw_string_from_dict(dict, "Device", "&s", &net->device);
  change |= iw_string_from_dict(dict, "KnownNetwork", "&o", &net->known);
  change |= iw_bool_from_dict(dict, "Connected", &net->connected);

  if(change)
    iw_network_updated(net);
  hash_table_unlock(iw_networks);
}

static void iw_object_handle ( gchar *path, gchar *iface, GVariant *dict )
{
  if(strstr(iface, iw_iface_device))
    iw_device_handle(path, iface, dict);
  else if(strstr(iface, iw_iface_station))
    iw_station_handle(path, iface, dict);
  else if(strstr(iface, iw_iface_network))
    iw_network_handle(path, iface, dict);
  else if(strstr(iface, iw_iface_known))
    iw_known_network_handle(path, iface, dict);
  else if(strstr(iface,"net.connman.iwd.AgentManager"))
    g_dbus_connection_call(iw_con, iw_serv, path,
        "net.connman.iwd.AgentManager", "RegisterAgent",
        g_variant_new("(o)", "/org/hosers/sfwbar"),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);

  g_variant_unref(dict);
}

static void iw_object_new_cb ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *siface, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariantIter *iter;
  GVariant *dict;
  gchar *object, *iface;

  g_variant_get(params, "(&oa{sa{sv}})", &object, &iter);
  while(g_variant_iter_next(iter, "{&s@a{sv}}", &iface, &dict))
    iw_object_handle(object, iface, dict);
  g_variant_iter_free(iter);
}

static void iw_object_prop_cb ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *siface, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariant *dict, *invalid;
  gchar *iface;

  g_variant_get(params,"(&s@a{sv}@as)", &iface, &dict, &invalid);
  iw_object_handle((gchar *)path, iface, dict);
  iw_object_handle((gchar *)path, iface, invalid);
}

static void iw_object_removed_cb ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *siface, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariantIter *iter;
  gchar *object, *iface;

  g_variant_get(params, "(&oas)", &object, &iter);
  while(g_variant_iter_next(iter, "&s", &iface))
  {
    if(!g_strcmp0(iface, iw_iface_network))
    {
      hash_table_lock(iw_networks);
      iw_network_remove(iw_network_get(object, FALSE));
      hash_table_unlock(iw_networks);
    }
    else if(!g_strcmp0(iface, iw_iface_known))
      iw_known_network_remove(iw_known_network_get(object, FALSE));
    else if(!g_strcmp0(iface, iw_iface_device))
      iw_device_free(iw_device_get(object, FALSE));
  }
  g_variant_iter_free(iter);
}

static void iw_init_cb ( GDBusConnection *con, GAsyncResult *res, gpointer data )
{
  GVariant *result, *dict;
  GVariantIter *miter, *iiter;
  gchar *path, *iface;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
    return;

  g_variant_get(result, "(a{oa{sa{sv}}})", &miter );
  while(g_variant_iter_next(miter, "{&oa{sa{sv}}}", &path, &iiter))
  {
    while(g_variant_iter_next(iiter, "{&s@a{sv}}", &iface, &dict))
      iw_object_handle(path, iface, dict);
    g_variant_iter_free(iiter);
  }
  g_variant_iter_free(miter);
  g_variant_unref(result);
}

static void iw_name_appeared_cb (GDBusConnection *con, const gchar *name,
    const gchar *owner, gpointer d)
{
  g_free(iw_owner);
  iw_owner = g_strdup(owner);
  module_interface_activate(&sfwbar_interface);
}

static void iw_name_disappeared_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
  module_interface_deactivate(&sfwbar_interface);
}

static value_t iw_expr_get ( vm_t *vm, value_t p[], gint np )
{
  iw_device_t *device;
  iw_network_t *net;
  value_t val;
  gchar *prop;

  vm_param_check_np_range(vm, np, 1, 2, "WifiGet");
  vm_param_check_string(vm, p, 0, "WifiGet");
  if(np==2)
    vm_param_check_string(vm, p, 1, "WifiGet");

//  hash_table_lock(iw_networks);
  if(np==2 && (net = iw_network_get(value_get_string(p[0]), FALSE)) )
  {
    prop = value_get_string(p[1]);
    if(!g_ascii_strcasecmp(prop, "ssid"))
      return value_new_string(g_strdup(net->ssid?net->ssid:""));
    if(!g_ascii_strcasecmp(prop, "path"))
      return value_new_string(g_strdup(net->path?net->path:""));
    if(!g_ascii_strcasecmp(prop, "type"))
      return value_new_string(g_strdup(net->type?net->type:""));
    if(!g_ascii_strcasecmp(prop, "known"))
      return value_new_string(g_strdup(net->known?net->known:""));
    if(!g_ascii_strcasecmp(prop, "strength"))
      return value_new_string(
          g_strdup_printf("%d", CLAMP(2*(net->strength/100+100),0,100)));
    if(!g_ascii_strcasecmp(prop, "connected"))
      return value_new_string(g_strdup_printf("%d", net->connected));
  }
//  hash_table_unlock(iw_networks);

  if(iw_devices &&
      !g_ascii_strcasecmp(value_get_string(p[0]), "DeviceStrength"))
  {
    g_rec_mutex_lock(&device_mutex);
    device = value_get_string(p[1])?
      iw_device_get(value_get_string(p[1]), FALSE) : iw_devices->data;
    val = value_new_string(g_strdup_printf("%d",
        device? CLAMP((device->strength*-10+100), 0, 100) : 0));
    g_rec_mutex_unlock(&device_mutex);

    return val;
  }

  return value_na;
}

static value_t iw_action_scan ( vm_t *vm, value_t p[], gint np )
{
  iw_device_t *device = NULL;
  GList *iter;

  vm_param_check_np_range(vm, np, 0, 1, "WifiScan");

  if(np)
  {
    vm_param_check_string(vm, p, 0, "WifiScan");
    g_rec_mutex_lock(&device_mutex);
    for(iter=iw_devices; iter; iter=g_list_next(iter))
      if(!g_strcmp0(((iw_device_t *)iter->data)->name, value_get_string(p[0])))
        break;
    g_rec_mutex_unlock(&device_mutex);

    if(iter)
      device = iter->data;
  }

  if(!device)
  {
    if(iw_devices)
      device = iw_devices->data;
    else
      return value_na;
  }

  iw_scan_start(device->path);

  return value_na;
}

static value_t iw_action_connect ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WifiConnect");
  vm_param_check_string(vm, p, 0, "WifiConnect");

  iw_network_connect(value_get_string(p[0]));

  return value_na;
}

static value_t iw_action_disconnect ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WifiConnect");
  vm_param_check_string(vm, p, 0, "WifiConnect");

  iw_network_disconnect(value_get_string(p[0]));

  return value_na;
}

static value_t iw_action_forget ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WifiForget");
  vm_param_check_string(vm, p, 0, "WifiForget");

  iw_network_forget(value_get_string(p[0]));

  return value_na;
}

static void iw_activate ( void )
{
  vm_func_add("wifiget", iw_expr_get, FALSE, TRUE);
  vm_func_add("wifiscan", iw_action_scan, TRUE, TRUE);
  vm_func_add("wificonnect", iw_action_connect, TRUE, TRUE);
  vm_func_add("wifidisconnect", iw_action_disconnect, TRUE, TRUE);
  vm_func_add("wififorget", iw_action_forget, TRUE, TRUE);
  sub_add = g_dbus_connection_signal_subscribe(iw_con, iw_owner,
      "org.freedesktop.DBus.ObjectManager", "InterfacesAdded", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, iw_object_new_cb, NULL, NULL);
  sub_del = g_dbus_connection_signal_subscribe(iw_con, iw_owner,
      "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, iw_object_removed_cb, NULL, NULL);
  sub_chg = g_dbus_connection_signal_subscribe(iw_con, iw_owner,
      "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, iw_object_prop_cb, NULL, NULL);

  g_dbus_connection_call(iw_con, iw_serv,"/",
      "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL,
      G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)iw_init_cb, NULL);
}

static void iw_deactivate ( void )
{
  g_debug("iwd: daemon disappeared");

  g_rec_mutex_lock(&device_mutex);
  while(iw_devices)
    iw_device_free(iw_devices->data);
  g_rec_mutex_unlock(&device_mutex);
  g_clear_pointer(&iw_networks, hash_table_remove_all);
  g_clear_pointer(&iw_known_networks, hash_table_remove_all);

  sfwbar_interface.active = FALSE;
}

static void iw_finalize ( void )
{
  vm_func_remove("wifiget");
  vm_func_remove("wifiack");
  vm_func_remove("wifiackremoved");
  vm_func_remove("wifiscan");
  vm_func_remove("wificonnect");
  vm_func_remove("wifidisconnect");
  vm_func_remove("wififorget");
}

gboolean sfwbar_module_init ( void )
{
  GDBusNodeInfo *node;
  static GDBusInterfaceVTable iw_agent_vtable = {
    (GDBusInterfaceMethodCallFunc)iw_agent_method, NULL, NULL };
  static GDBusInterfaceVTable iw_signal_level_agent_vtable = {
    (GDBusInterfaceMethodCallFunc)iw_signal_level_agent_method, NULL, NULL };

  if( !(iw_con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL)) )
    return FALSE;

  iw_networks = hash_table_new_full(g_str_hash, g_str_equal, NULL,
      (GDestroyNotify)iw_network_free);
  iw_known_networks = hash_table_new_full(g_str_hash, g_str_equal, NULL,
      (GDestroyNotify)iw_known_network_free);

  node = g_dbus_node_info_new_for_xml(iw_agent_xml, NULL);
  g_dbus_connection_register_object (iw_con,
      "/org/hosers/sfwbar", node->interfaces[0],
      &iw_agent_vtable, NULL, NULL, NULL);
  g_dbus_node_info_unref(node);

  node = g_dbus_node_info_new_for_xml(iw_signal_level_agent_xml, NULL);
  g_dbus_connection_register_object (iw_con,
      "/org/hosers/sfwbar", node->interfaces[0],
      &iw_signal_level_agent_vtable, NULL, NULL, NULL);
  g_dbus_node_info_unref(node);

  g_bus_watch_name(G_BUS_TYPE_SYSTEM, iw_serv, G_BUS_NAME_WATCHER_FLAGS_NONE,
      iw_name_appeared_cb, iw_name_disappeared_cb, NULL, NULL);
  return TRUE;
}

ModuleInterfaceV1 sfwbar_interface = {
  .interface = "wifi",
  .provider = "IWD",
  .activate = iw_activate,
  .deactivate = iw_deactivate,
  .finalize = iw_finalize
};
