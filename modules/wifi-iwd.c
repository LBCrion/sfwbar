#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include "../src/module.h"
#include "../src/basewidget.h"
#include "../src/popup.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;

extern ModuleInterfaceV1 sfwbar_interface;

static void iw_signal_level_agent_init ( gchar *path );
static void iw_scan_start ( gchar *path );

static const gchar *iw_serv = "net.connman.iwd";
static GDBusConnection *iw_con;
static GList *iw_devices;
static GHashTable *iw_networks, *iw_known_networks;
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

  if(g_variant_check_format_string(dict,"a{sv}",FALSE))
  {
    if(!g_variant_lookup(dict, key, format, &tmp) || !g_strcmp0(*str, tmp))
      return FALSE;
  }
  else if(g_variant_check_format_string(dict,"as",FALSE))
  {
    if(!iw_array_lookup(dict, key) || !*str)
      return FALSE;
    tmp = NULL;
  }
  else
    return FALSE;
  
  g_free(*str);
  *str = g_strdup(tmp);
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

  if(iw_known_networks && known->path)
    g_hash_table_remove(iw_known_networks, known);
}

static iw_known_t *iw_known_network_get ( gchar *path, gboolean create )
{
  iw_known_t *known;

  if(!path)
    return NULL;

  if(iw_known_networks && (known=g_hash_table_lookup(iw_known_networks, path)))
    return known;

  if(!create)
    return NULL;

  known = g_malloc0(sizeof(iw_known_t));
  known->path = g_strdup(path);

  if(!iw_known_networks)
    iw_known_networks = g_hash_table_new_full(g_str_hash, g_str_equal,
        NULL, (GDestroyNotify)iw_known_network_free);

  g_hash_table_insert(iw_known_networks, known->path, known);
  return known;
}

static iw_network_t *iw_network_dup ( iw_network_t *net )
{
  iw_network_t *copy;

  copy = g_malloc0(sizeof(iw_network_t));
  copy->path = g_strdup(net->path);
  copy->ssid = g_strdup(net->ssid);
  copy->type = g_strdup(net->type);
  copy->known = g_strdup(net->known);
  copy->device = g_strdup(net->device);
  copy->strength = net->strength;
  copy->connected = net->connected;

  return copy;
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

static gboolean iw_network_compare ( iw_network_t *n1, iw_network_t *n2 )
{
  return g_strcmp0(n1->path, n2->path);
}

static void *iw_network_get_str ( iw_network_t *net, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "ssid"))
    return g_strdup(net->ssid?net->ssid:"");
  if(!g_ascii_strcasecmp(prop, "path"))
    return g_strdup(net->path?net->path:"");
  if(!g_ascii_strcasecmp(prop, "type"))
    return g_strdup(net->type?net->type:"");
  if(!g_ascii_strcasecmp(prop, "known"))
    return g_strdup(net->known?net->known:"");
  if(!g_ascii_strcasecmp(prop, "strength"))
    return g_strdup_printf("%d", CLAMP(2*(net->strength/100+100),0,100));
  if(!g_ascii_strcasecmp(prop, "connected"))
    return g_strdup_printf("%d", net->connected);

  return NULL;
}

static module_queue_t update_q = {
  .free = (void (*)(void *))iw_network_free,
  .duplicate = (void *(*)(void *))iw_network_dup,
  .get_str = (void *(*)(void *, gchar *))iw_network_get_str,
  .get_num = (void *(*)(void *, gchar *))NULL,
  .compare = (gboolean (*)(const void *, const void *))iw_network_compare,
};

static void *iw_remove_get_str ( gchar *name, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "RemovedPath"))
    return g_strdup(name);
  else
    return NULL;
}

static module_queue_t remove_q = {
  .free = g_free,
  .duplicate = (void *(*)(void *))g_strdup,
  .get_str = (void *(*)(void *, gchar *))iw_remove_get_str,
  .compare = (gboolean (*)(const void *, const void *))g_strcmp0,
};

static void iw_network_updated ( iw_network_t *net )
{
  if(net)
  {
    module_queue_append(&update_q, net);
    g_debug("iwd: network: %s, type: %s, conn: %d, known: %s, strength: %d",
        net->ssid, net->type, net->connected, net->known, net->strength);
  }
}

static void iw_network_remove ( iw_network_t *network )
{
  if(!network)
    return;

  g_debug("iwd: remove network: %s", network->ssid);
  module_queue_append(&remove_q, network->path);

  if(iw_networks && network->path)
    g_hash_table_remove(iw_networks, network->path);
}

static iw_network_t *iw_network_get ( gchar *path, gboolean create )
{
  iw_network_t *net;

  if(path && iw_networks && (net = g_hash_table_lookup(iw_networks, path)))
    return net;

  if(!create)
    return NULL;

  net = g_malloc0(sizeof(iw_network_t));
  net->path = g_strdup(path);

  if(!iw_networks)
    iw_networks = g_hash_table_new_full(g_str_hash, g_str_equal,
        NULL, (GDestroyNotify)iw_network_free);

  g_hash_table_insert(iw_networks, net->path, net);
  return net;
}

static iw_device_t *iw_device_get ( gchar *path, gboolean create )
{
  iw_device_t *device;
  GList *iter;

  for(iter=iw_devices; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((iw_device_t *)iter->data)->path, path))
      return iter->data;

  if(!create)
    return NULL;

  iw_signal_level_agent_init(path);
  iw_scan_start(path);
  device = g_malloc0(sizeof(iw_device_t));
  device->path = g_strdup(path);
  iw_devices = g_list_prepend(iw_devices, device);
  return device;
}

static void iw_device_free ( iw_device_t *device )
{
  if(!device)
    return;

  g_debug("iwd: remove device: %s", device->name);

  iw_devices = g_list_remove(iw_devices, device);
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
    if( (device = iw_device_get(object, FALSE)) )
        device->strength = level;
    
    g_debug("iwd: level %d on %s", level, device?device->name:object);
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("wifi_level"));
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
}

static void iw_scan_start ( gchar *path )
{
  iw_device_t *device;

  device = iw_device_get(path, FALSE);
  if(device && device->scanning)
    return;
  g_debug("iwd: initiating scan");
  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("wifi_scan"));
  g_dbus_connection_call(iw_con, iw_serv, path, iw_iface_station, "Scan",
      NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void iw_network_connect_cb ( GDBusConnection *con, GAsyncResult *res,
    gchar *path )
{
  GVariant *result;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result)
    g_variant_unref(result);

  iw_network_updated(iw_network_get(path, FALSE));
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

  for(iter=iw_devices; iter; iter=g_list_next(iter))
  {
    device = iter->data;
    if(!g_strcmp0(device->conn_net, path))
      g_dbus_connection_call(iw_con, iw_serv, device->path, iw_iface_station,
          "Disconnect", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL,
          NULL);
  }
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
  while(g_variant_iter_next(iter, "(&on)", &path, &strength))
    if( (net = iw_network_get(path, FALSE)) && net->strength!=strength)
    {
      net->strength = strength;
      iw_network_updated(net);
    }

  g_variant_iter_free(iter);
  g_variant_unref(result);
}

static void iw_device_handle ( gchar *path, gchar *iface, GVariant *dict )
{
  iw_device_t *device;
  gboolean change = FALSE;

  device = iw_device_get(path, TRUE);
  change |= iw_string_from_dict( dict, "Name", "&s", &device->name);

  if(change)
    g_debug("iwd: device: %s, state: %s", device->name, device->state);
}

static void iw_station_handle ( gchar *path, gchar *iface, GVariant *dict )
{
  iw_device_t *device;
  gboolean change = FALSE, scan;

  device = iw_device_get(path, TRUE);
  change |= iw_string_from_dict(dict, "State", "&s", &device->state);
  change |= iw_string_from_dict(dict, "ConnectedNetwork", "&o",
      &device->conn_net);
  scan = iw_bool_from_dict(dict, "Scanning", &device->scanning);
  change |= scan;
  if(scan && !device->scanning)
  {
    g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
        (gpointer)g_intern_static_string("wifi_scan_complete"));
    g_dbus_connection_call(iw_con, iw_serv, path, iw_iface_station,
        "GetOrderedNetworks", NULL, G_VARIANT_TYPE("(a(on))"),
        G_DBUS_CALL_FLAGS_NONE, -1, NULL,
        (GAsyncReadyCallback)iw_network_strength_cb, NULL);
  }
  if(change)
    g_debug("iwd: device: %s, state: %s, scanning: %d", device->name,
        device->state, device->scanning);
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

  net = iw_network_get(path, TRUE);
  change |= iw_string_from_dict(dict, "Name", "&s", &net->ssid);
  change |= iw_string_from_dict(dict, "Type", "&s", &net->type);
  change |= iw_string_from_dict(dict, "Device", "&s", &net->device);
  change |= iw_string_from_dict(dict, "KnownNetwork", "&o", &net->known);
  change |= iw_bool_from_dict(dict, "Connected", &net->connected);

  if(change)
    iw_network_updated(net);
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
      iw_network_remove(iw_network_get(object, FALSE));
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

static void iw_activate ( void )
{
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

static void iw_name_appeared_cb (GDBusConnection *con, const gchar *name,
    const gchar *owner, gpointer d)
{
  g_free(iw_owner);
  iw_owner = g_strdup(owner);
  module_interface_activate(&sfwbar_interface);
}

static void iw_deactivate ( void )
{
  g_debug("iwd: daemon disappeared");

  while(iw_devices)
    iw_device_free(iw_devices->data);
  if(iw_networks)
    g_hash_table_remove_all(iw_networks);
  if(iw_known_networks)
    g_hash_table_remove_all(iw_known_networks);

  sfwbar_interface.active = !!update_q.list | !!remove_q.list;
}

static void iw_name_disappeared_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
  module_interface_deactivate(&sfwbar_interface);
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

  update_q.trigger = g_intern_static_string("wifi_updated");
  remove_q.trigger = g_intern_static_string("wifi_removed");

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

static void *iw_expr_get ( void **params, void *widget, void *event )
{
  gchar *result;
  iw_device_t *device;

  if(!params || !params[0])
    return NULL;

  if( (result = module_queue_get_string(&update_q, params[0])) )
    return result;

  if(iw_devices && !g_ascii_strcasecmp(params[0], "DeviceStrength"))
  {
    device = params[1]?iw_device_get(params[1], FALSE):iw_devices->data;
    return g_strdup_printf("%d",
        device?CLAMP((device->strength*-10+100),0,100):0);
  }
  if( (result = module_queue_get_string(&remove_q, params[0])) )
    return result;

  return NULL;
}

static void iw_action_ack ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  module_queue_remove(&update_q);
  if(!sfwbar_interface.ready)
  {
    sfwbar_interface.active = !!update_q.list | !!remove_q.list;
    module_interface_select(sfwbar_interface.interface);
  }
}

static void iw_action_ack_removed ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  module_queue_remove(&remove_q);
  if(!sfwbar_interface.ready)
  {
    sfwbar_interface.active = !!update_q.list | !!remove_q.list;
    module_interface_select(sfwbar_interface.interface);
  }
}

static void iw_action_scan ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  iw_device_t *device;
  GList *iter;
  
  for(iter=iw_devices; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((iw_device_t *)iter->data)->name, cmd))
      break;
  if(iter)
    device = iter->data;
  else if(iw_devices)
    device = iw_devices->data;
  else
    return;

  iw_scan_start(device->path);
}

static void iw_action_connect ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(cmd)
    iw_network_connect(cmd);
}

static void iw_action_disconnect ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(cmd)
    iw_network_disconnect(cmd);
}

static void iw_action_forget ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(cmd)
    iw_network_forget(cmd);
}

static ModuleExpressionHandlerV1 get_handler = {
  .flags = 0,
  .name = "WifiGet",
  .parameters = "Ss",
  .function = iw_expr_get
};

static ModuleExpressionHandlerV1 *iw_expr_handlers[] = {
  &get_handler,
  NULL
};

static ModuleActionHandlerV1 ack_handler = {
  .name = "WifiAck",
  .function = iw_action_ack
};

static ModuleActionHandlerV1 ack_removed_handler = {
  .name = "WifiAckRemoved",
  .function = iw_action_ack_removed
};

static ModuleActionHandlerV1 scan_handler = {
  .name = "WifiScan",
  .function = iw_action_scan
};

static ModuleActionHandlerV1 connect_handler = {
  .name = "WifiConnect",
  .function = iw_action_connect
};

static ModuleActionHandlerV1 disconnect_handler = {
  .name = "WifiDisconnect",
  .function = iw_action_disconnect
};

static ModuleActionHandlerV1 forget_handler = {
  .name = "WifiForget",
  .function = iw_action_forget
};

static ModuleActionHandlerV1 *iw_action_handlers[] = {
  &ack_handler,
  &ack_removed_handler,
  &scan_handler,
  &connect_handler,
  &disconnect_handler,
  &forget_handler,
  NULL
};

ModuleInterfaceV1 sfwbar_interface = {
  .interface = "wifi",
  .provider = "IWD",
  .expr_handlers = iw_expr_handlers,
  .act_handlers = iw_action_handlers,
  .activate = iw_activate,
  .deactivate = iw_deactivate
};
