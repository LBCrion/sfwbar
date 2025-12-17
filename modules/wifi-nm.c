#include <gtk/gtk.h>
#include <gio/gio.h>
#include "module.h"
#include "trigger.h"
#include "gui/popup.h"
#include "util/hash.h"
#include "util/string.h"
#include "vm/vm.h"

typedef struct _nm_apoint nm_apoint_t;
typedef struct _nm_ap_node nm_ap_node_t;
typedef struct _nm_conn nm_conn_t;
typedef struct _nm_device nm_device_t;
typedef struct _nm_active nm_active_t;
typedef struct _nm_dialog nm_dialog_t;

#define NM_APOINT(x) ((nm_apoint_t *)x)
#define NM_AP_NODE(x) ((nm_ap_node_t *)x)
#define NM_CONN(x) ((nm_conn_t *)x)
#define NM_ACTIVE(x) ((nm_active_t *)x)
#define NM_DEVICE(x) ((nm_device_t *)x)

struct _nm_ap_node {
  gchar *path;
  guint8 strength;
  nm_active_t *active;
  nm_apoint_t *ap;
};

struct _nm_apoint {
  GList *nodes;
  nm_ap_node_t *active_node;
  gchar *ssid;
  gchar *hash;
  guint flags, wpa, rsn, mode;
  guint8 strength;
  gboolean visible;
  nm_conn_t *conn;
};

struct _nm_conn {
  gchar *path;
  gchar *ssid;
};

struct _nm_active {
  gchar *path;
  gchar *ap_path;
  nm_device_t *device;
  nm_apoint_t *ap;
};

struct _nm_device {
  gchar *path;
  gchar *name;
  nm_active_t *active;
};

struct _nm_dialog {
  GDBusMethodInvocation *invocation;
  GtkWidget *win, *title, *user, *pass, *ok, *cancel;
  gchar *path;
};

static const gchar nm_secret_agent_xml[] =
  "<node>"
    "<interface name='org.freedesktop.NetworkManager.SecretAgent'>"
      "<method name='GetSecrets'>"
        "<arg name='connection' type='a{sa{sv}}' direction='in'/>"
        "<arg name='connection_path' type='o' direction='in'/>"
        "<arg name='setting_name' type='s' direction='in'/>"
        "<arg name='hints' type='as' direction='in'/>"
        "<arg name='flags' type='u' direction='in'/>"
        "<arg name='secrets' type='a{sa{sv}}' direction='out'/>"
      "</method>"
      "<method name='CancelGetSecrets'>"
        "<arg name='connection_path' type='o' direction='in'/>"
        "<arg name='setting_name' type='s' direction='in'/>"
      "</method>"
      "<method name='SaveSecrets'>"
        "<arg name='connection' type='a{sa{sv}}' direction='in'/>"
        "<arg name='connection_path' type='o' direction='in'/>"
      "</method>"
      "<method name='DeleteSecrets'>"
        "<arg name='connection' type='a{sa{sv}}' direction='in'/>"
        "<arg name='connection_path' type='o' direction='in'/>"
      "</method>"
    "</interface>"
  "</node>";

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;

extern ModuleInterfaceV1 sfwbar_interface;

static const gchar *nm_iface = "org.freedesktop.NetworkManager";
static const gchar *nm_iface_device =
  "org.freedesktop.NetworkManager.Device";
static const gchar *nm_iface_wireless =
  "org.freedesktop.NetworkManager.Device.Wireless";
static const gchar *nm_iface_conn =
  "org.freedesktop.NetworkManager.Settings.Connection";
static const gchar *nm_iface_active =
  "org.freedesktop.NetworkManager.Connection.Active";
static const gchar *nm_iface_apoint =
  "org.freedesktop.NetworkManager.AccessPoint";
static const gchar *nm_iface_agtmgr =
  "org.freedesktop.NetworkManager.AgentManager";
static const gchar *nm_iface_objmgr =
  "org.freedesktop.DBus.ObjectManager";
static const gchar *nm_iface_objprop =
  "org.freedesktop.DBus.Properties";
static const gchar *nm_path = "/org/freedesktop/NetworkManager";
static const gchar *nm_path_secret =
  "/org/freedesktop/NetworkManager/SecretAgent";

static GDBusConnection *nm_con;
static hash_table_t *ap_nodes, *dialog_list, *new_conns, *conn_list;
static hash_table_t *active_list, *apoint_list;
static GList *devices;
static GMutex device_lock;
static nm_device_t *default_dev;
static guint sub_add, sub_del, sub_chg;
static guint32 nm_strength_threshold;
static gchar *nm_owner;

static gboolean nm_device_name_cmp ( nm_device_t *device, gchar *name )
{
  return !g_strcmp0(device->name, name);
}

static gboolean nm_conn_ssid_cmp ( gpointer key, nm_conn_t *conn, gchar *ssid )
{
  return !g_strcmp0(conn->ssid, ssid);
}

static gboolean nm_active_ap_cmp ( gchar *k, nm_active_t *active, gchar *path )
{
  return !g_strcmp0(active->ap_path, path);
}

static gchar *nm_ssid_get ( GVariant *dict, gchar *key )
{
  GVariant *vssid;
  gchar *ssid;
  const gchar *tssid;
  gsize len;

  if(!g_variant_lookup(dict, key, "@ay", &vssid))
    return NULL;
  tssid = g_variant_get_fixed_array(vssid, &len, 1);
  if(tssid && len > 0)
    ssid = g_strndup(tssid, len);
  else
    ssid = NULL;
  g_variant_unref(vssid);

  return ssid;
}

static const gchar *nm_apoint_get_type ( nm_apoint_t *ap )
{
  if(!(ap->flags & 0x00000001))
    return "open";
  if(!ap->rsn && !ap->wpa)
    return "wep";
  if((ap->rsn & 0x00000200) || (ap->wpa & 0x00000200))
    return "8021x";
  return "psk";
}

static void nm_apoint_free ( nm_apoint_t *apoint )
{
  g_free(apoint->ssid);
  g_free(apoint->hash);
  g_free(apoint);
}

static void nm_apoint_update ( nm_apoint_t *ap )
{
  if(ap->visible)
    trigger_emit_with_string("wifi-conf", "id", g_strdup(ap->hash));
  else
    trigger_emit_with_string("wifi-conf-removed", "id", g_strdup(ap->hash));

  g_debug("nm: ap: %s, %s, known: %d, conn: %d, strength: %d", ap->ssid,
      nm_apoint_get_type(ap), !!ap->conn, !!ap->active_node, ap->strength);
}

static void nm_apoint_ssid_update ( gchar *key, nm_apoint_t *ap, gchar *ssid )
{
  if(!g_strcmp0(ap->ssid, ssid))
  {
    ap->conn = hash_table_find(conn_list, (GHRFunc)nm_conn_ssid_cmp, ssid);
    nm_apoint_update(ap);
  }
}

static void nm_conn_free ( nm_conn_t *conn )
{
  hash_table_foreach(apoint_list, (GHFunc)nm_apoint_ssid_update, conn->ssid);

  g_free(conn->ssid);
  g_free(conn->path);
  g_free(conn);
}

static void nm_conn_connect ( gchar *path )
{
  g_debug("nm: connecting to: %s", path);
  g_dbus_connection_call(nm_con, nm_iface, nm_path, nm_iface,
      "ActivateConnection", g_variant_new("(ooo)", path, "/", "/"),
      G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void nm_passphrase_cb ( GtkEntry *entry, nm_dialog_t *dialog )
{
  const gchar *pass;
  GVariant *psk, *wsec;

  if(gtk_entry_get_text_length(GTK_ENTRY(dialog->pass))<8 ||
      (dialog->user && gtk_entry_get_text_length(GTK_ENTRY(dialog->user))<1))
    return;
  pass = gtk_entry_get_text(GTK_ENTRY(dialog->pass));
  psk = g_variant_new_dict_entry(g_variant_new_string("psk"),
      g_variant_new_variant(g_variant_new_string(pass)));
  wsec = g_variant_new_dict_entry(
      g_variant_new_string("802-11-wireless-security"),
      g_variant_new_array( G_VARIANT_TYPE("{sv}"), &psk, 1));

  g_dbus_method_invocation_return_value(dialog->invocation,
      g_variant_new("(@a{sa{sv}})",
        g_variant_new_array(G_VARIANT_TYPE("{sa{sv}}"), &wsec, 1)));
  gtk_widget_destroy(dialog->win);
  hash_table_remove(dialog_list, dialog->path);
}

static void nm_button_clicked ( GtkWidget *button, nm_dialog_t *dialog )
{
  if(button==dialog->ok)
    nm_passphrase_cb(NULL, dialog);
  else
  {
    g_dbus_method_invocation_return_dbus_error(dialog->invocation,
        "org.freedesktop.NetworkManager.SecretAgent.UserCanceled", "");
    gtk_widget_destroy(dialog->win);
    hash_table_remove(dialog_list, dialog->path);
  }
}

static void nm_passphrase_changed_cb ( gpointer *w, nm_dialog_t *dialog )
{
  gtk_widget_set_sensitive(dialog->ok,
      gtk_entry_get_text_length(GTK_ENTRY(dialog->pass))>7 &&
      (!dialog->user || gtk_entry_get_text_length(GTK_ENTRY(dialog->user))>0));
}

static gboolean nm_passphrase_delete_cb ( GtkWidget *w, GdkEvent *ev,
    nm_dialog_t *dialog )
{
  g_dbus_method_invocation_return_dbus_error(dialog->invocation,
      "org.freedesktop.NetworkManager.SecretAgent.UserCanceled", "");
  hash_table_remove(dialog_list, dialog->path);
  return FALSE;
}

static gboolean nm_passphrase_prompt ( GVariant *vconn, gpointer inv )
{
  nm_dialog_t *dialog;
  nm_conn_t *conn;
  GtkWidget *grid, *label;
  gchar *title, *path;
  gboolean user = FALSE;

  g_variant_get(vconn,"(@a{sa{sv}}&o&sasu)", NULL, &path, NULL, NULL, NULL);
  hash_table_lock(conn_list);
  if( (conn = hash_table_lookup(conn_list, path)) )
    title = g_strdup_printf("Passphrase for network %s", conn->ssid);
  else
    title = NULL;
  hash_table_unlock(conn_list);
  if(!title)
    return FALSE;
  dialog = g_malloc0(sizeof(nm_dialog_t));

  dialog->invocation = inv;
  dialog->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint(GTK_WINDOW(dialog->win),
      GDK_WINDOW_TYPE_HINT_DIALOG);
  g_signal_connect(G_OBJECT(dialog->win), "delete-event",
    G_CALLBACK(nm_passphrase_delete_cb), dialog);
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
    gtk_widget_set_name(dialog->user, "iwd_user_entry");
    gtk_grid_attach(GTK_GRID(grid), dialog->user, 2, 2, 1, 1);
    g_signal_connect(G_OBJECT(dialog->user), "changed",
        G_CALLBACK(nm_passphrase_changed_cb), dialog);
  }

  label = gtk_label_new("Passphrase:");
  gtk_widget_set_name(label, "wifi_passphrase_label");
  gtk_grid_attach(GTK_GRID(grid), label, 1, 3, 1, 1);
  dialog->pass = gtk_entry_new();
  gtk_widget_set_name(dialog->pass, "wifi_passphrase_entry");
  gtk_entry_set_visibility(GTK_ENTRY(dialog->pass), FALSE);
  g_signal_connect(G_OBJECT(dialog->pass), "activate",
      G_CALLBACK(nm_passphrase_cb), dialog);
    g_signal_connect(G_OBJECT(dialog->pass), "changed",
        G_CALLBACK(nm_passphrase_changed_cb), dialog);
  gtk_grid_attach(GTK_GRID(grid), dialog->pass, 2, 3, 1, 1);

  dialog->ok = gtk_button_new_with_label("Ok");
  gtk_widget_set_name(dialog->ok, "wifi_button_ok");
  gtk_grid_attach(GTK_GRID(grid), dialog->ok, 1, 4, 1, 1);
  gtk_widget_set_sensitive(dialog->ok, FALSE);
  g_signal_connect(G_OBJECT(dialog->ok), "clicked",
      G_CALLBACK(nm_button_clicked), dialog);

  dialog->cancel = gtk_button_new_with_label("Cancel");
  gtk_widget_set_name(dialog->cancel, "wifi_button_cancel");
  gtk_grid_attach(GTK_GRID(grid), dialog->cancel, 2, 4, 1, 1);
  g_signal_connect(G_OBJECT(dialog->cancel), "clicked",
      G_CALLBACK(nm_button_clicked), dialog);

  g_debug("nm: secrets dialog: %s", path);
  g_object_ref_sink(G_OBJECT(dialog->win));
  dialog->path = g_strdup(path);
  hash_table_insert(dialog_list, dialog->path, dialog);
  popup_popdown_autoclose();
  gtk_widget_show_all(dialog->win);

  return TRUE;
}

static void nm_conn_new_cb ( GDBusConnection *con, GAsyncResult *res,
    gpointer d )
{
  GVariant *result;
  gchar *path, *active_path;

  if( (result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    g_variant_get(result, "(oo)", &path, &active_path);
    hash_table_insert(new_conns, active_path, path);
    g_variant_unref(result);
  }
}

static void nm_conn_new ( gchar *path )
{
  GVariant *conn_entry[3], *wifi_entry[2], *wsec_entry[2], *entry[5];
  GVariant *dict, *ipv4_entry, *ipv6_entry;
  nm_apoint_t *ap;

  hash_table_lock(apoint_list);
  if( !(ap = hash_table_lookup(apoint_list, path)) ||
      (ap->rsn & 0x00000200) || (ap->wpa & 0x00000200))
  {
    if(ap)
      trigger_emit_with_string("wifi-conf", "id", g_strdup(ap->hash));
    hash_table_unlock(apoint_list);
    return; // we don't support 802.11x
  }

  if(ap->conn)
  {
    nm_conn_connect(ap->conn->path);
    hash_table_unlock(apoint_list);
    return;
  }

  conn_entry[0] = g_variant_new_dict_entry(g_variant_new_string("type"),
      g_variant_new_variant(g_variant_new_string("802-11-wireless")));
  conn_entry[1] = g_variant_new_dict_entry(g_variant_new_string("uuid"),
      g_variant_new_variant(
        g_variant_new_take_string(g_uuid_string_random())));
  conn_entry[2] = g_variant_new_dict_entry(g_variant_new_string("id"),
      g_variant_new_variant(g_variant_new_take_string(g_strdup(ap->ssid))));
  entry[0] = g_variant_new_dict_entry(g_variant_new_string("connection"),
      g_variant_new_array( G_VARIANT_TYPE("{sv}"), conn_entry, 3));

  wifi_entry[0] = g_variant_new_dict_entry(g_variant_new_string("ssid"),
      g_variant_new_variant(g_variant_new_fixed_array(G_VARIANT_TYPE("y"),
          ap->ssid, strlen(ap->ssid), sizeof(gchar))));
  wifi_entry[1] = g_variant_new_dict_entry(g_variant_new_string("mode"),
      g_variant_new_variant(g_variant_new_string("infrastructure")));
  entry[1] = g_variant_new_dict_entry(g_variant_new_string("802-11-wireless"),
    g_variant_new_array( G_VARIANT_TYPE("{sv}"), wifi_entry, 2));

  ipv4_entry = g_variant_new_dict_entry(g_variant_new_string("method"),
      g_variant_new_variant(g_variant_new_string("auto")));
  entry[2] = g_variant_new_dict_entry(g_variant_new_string("ipv4"),
    g_variant_new_array( G_VARIANT_TYPE("{sv}"), &ipv4_entry, 1));

  ipv6_entry = g_variant_new_dict_entry(g_variant_new_string("method"),
      g_variant_new_variant(g_variant_new_string("auto")));
  entry[3] = g_variant_new_dict_entry(g_variant_new_string("ipv6"),
    g_variant_new_array( G_VARIANT_TYPE("{sv}"), &ipv6_entry, 1));

  if(ap->rsn || ap->wpa)
  {
    wsec_entry[0] = g_variant_new_dict_entry(g_variant_new_string("key-mgmt"),
        g_variant_new_variant(g_variant_new_string("wpa-psk")));
    wsec_entry[1] = g_variant_new_dict_entry(g_variant_new_string("auth-alg"),
        g_variant_new_variant(g_variant_new_string("open")));
    entry[4] = g_variant_new_dict_entry(
        g_variant_new_string("802-11-wireless-security"),
        g_variant_new_array( G_VARIANT_TYPE("{sv}"), wsec_entry, 2));
  }

  dict = g_variant_new_array(G_VARIANT_TYPE("{sa{sv}}"), entry,
      (ap->rsn || ap->wpa)?5:4);

  g_mutex_lock(&device_lock);
  if(default_dev)
    g_dbus_connection_call(nm_con, nm_iface, nm_path, nm_iface,
        "AddAndActivateConnection",
        g_variant_new("(@a{sa{sv}}oo)", dict, default_dev->path, "/"),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
        (GAsyncReadyCallback)nm_conn_new_cb, NULL);
  g_mutex_unlock(&device_lock);
  hash_table_unlock(apoint_list);
}

static void nm_conn_handle_cb ( GDBusConnection *con, GAsyncResult *res,
    gchar *path )
{
  nm_conn_t *conn;
  GVariant *result, *dict, *odict;
  gchar *ssid;

  if( (result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    g_variant_get(result, "(@a{sa{sv}})", &odict);
    if(g_variant_lookup(odict, "802-11-wireless", "@a{sv}", &dict))
    {
      if( (ssid = nm_ssid_get(dict, "ssid")) )
      {
        conn = g_malloc0(sizeof(nm_conn_t));
        conn->path = g_strdup(path);
        conn->ssid = ssid;
        hash_table_insert(conn_list, conn->path, conn);
        hash_table_foreach(apoint_list, (GHFunc)nm_apoint_ssid_update, ssid);
        g_debug("nm: connection: %s (%s)", ssid, path);
      }
      g_variant_unref(dict);
    }
    g_variant_unref(odict);
    g_variant_unref(result);
  }
  g_free(path);
}

static void nm_ap_node_free ( nm_ap_node_t *node )
{
  nm_apoint_t *apoint;

  if(node->ap->active_node == node)
  {
    node->active->ap = NULL;
    node->ap->active_node = NULL;
  }
  if( (apoint = node->ap) )
    if( !(apoint->nodes = g_list_remove(apoint->nodes, node)) )
    {
      g_debug("nm: ap removed: %s", apoint->ssid);
      trigger_emit_with_string("wifi-conf-removed", "id", g_strdup(apoint->hash));
      hash_table_remove(apoint_list, apoint->hash);
    }

  g_free(node->path);
  g_free(node);
}

static gboolean nm_ap_node_active_update ( const gchar *path )
{
  nm_ap_node_t *node;
  nm_active_t *active;
  gboolean change = FALSE;

  if( !(node = hash_table_lookup(ap_nodes, path)) )
    return FALSE;

  active = hash_table_find(active_list, (GHRFunc)nm_active_ap_cmp, node->path);

  if(node->active != active)
    change = TRUE;
  node->active = active;

  if(active && node->ap->active_node != node)
  {
    change = TRUE;
    node->ap->active_node = node;
    active->ap = node->ap;
  }
  else if(!active && node->ap->active_node == node)
  {
    change = TRUE;
    node->ap->active_node = NULL;
  }

  return change;
}

static void nm_ap_node_handle ( const gchar *path, gchar *ifa, GVariant *dict )
{
  nm_apoint_t *apoint;
  nm_ap_node_t *node;
  GList *iter;
  gchar *ssid, *hash;
  guint flags, wpa, rsn, mode;
  guint8 strength, ap_strength;
  gboolean visible, change = FALSE;

  if( !(node = hash_table_lookup(ap_nodes, path)) )
  {
    if( !(ssid = nm_ssid_get(dict, "Ssid")) )
      return;
    
    node = g_malloc0(sizeof(nm_ap_node_t));
    node->path = g_strdup(path);
    if(!g_variant_lookup(dict, "Flags", "u", &flags)) flags = 0;
    if(!g_variant_lookup(dict, "WpaFlags", "u", &wpa)) wpa = 0;
    if(!g_variant_lookup(dict, "RsnFlags", "u", &rsn)) rsn = 0;
    if(!g_variant_lookup(dict, "Mode", "u", &mode)) mode = 0;

    hash = g_strdup_printf("%s-%x-%x-%x-%x", ssid, flags, wpa, rsn, mode);
    if( (apoint = hash_table_lookup(apoint_list, hash)) )
    {
      g_free(ssid);
      g_free(hash);
    }
    else
    {
      apoint = g_malloc0(sizeof(nm_apoint_t));
      apoint->ssid = ssid;
      apoint->wpa = wpa;
      apoint->rsn = rsn;
      apoint->flags = flags;
      apoint->mode = mode;
      apoint->hash = hash;
      hash_table_insert(apoint_list, apoint->hash, apoint);
    }
    apoint->nodes = g_list_prepend(apoint->nodes, node);
    node->ap = apoint;
    hash_table_insert(ap_nodes, node->path, node);
    nm_apoint_ssid_update(NULL, apoint, apoint->ssid);
    change = TRUE;
  }
  apoint = node->ap;

  change |= nm_ap_node_active_update(path);

  if(g_variant_lookup(dict, "Strength", "y", &strength))
  {
    node->strength = MIN(strength, 100);
    ap_strength = 0;
    for(iter=apoint->nodes; iter; iter=g_list_next(iter))
      ap_strength = MAX(ap_strength, NM_AP_NODE(iter->data)->strength);
    if((gint)(apoint->strength/10) != (gint)(ap_strength/10))
    {
      node->ap->strength = ap_strength;
      change = TRUE;
      trigger_emit("wifi");
    }
  }

  visible = node->ap->strength >= nm_strength_threshold;
  if(!visible && !node->ap->visible)
    change = FALSE; // ignore changes while signal strength is below threshold
  else
    node->ap->visible = visible;

  if(change)
    nm_apoint_update(apoint);
}

static void nm_active_disconnect ( gchar *path )
{
  nm_apoint_t *ap;

  hash_table_lock(active_list);
  hash_table_lock(apoint_list);
  if( (ap = hash_table_lookup(apoint_list, path)) && ap->active_node)
  {
    g_debug("nm: deactivating: %s", ap->active_node->active->path);
    g_dbus_connection_call(nm_con, nm_iface, nm_path, nm_iface,
        "DeactivateConnection", g_variant_new("(o)", ap->active_node->active->path),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  }
  hash_table_unlock(apoint_list);
  hash_table_unlock(active_list);
}

static void nm_conn_forget ( gchar *path )
{
  nm_conn_t *conn;

  hash_table_lock(conn_list);
  if( (conn = hash_table_lookup(conn_list, path)) )
  {
    g_debug("nm: forgetting: %s", conn->path);
    g_dbus_connection_call(nm_con, nm_iface, conn->path, nm_iface_conn,
        "Delete", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  }
  hash_table_unlock(conn_list);
}

static void nm_conn_forget_each ( gchar *path, nm_conn_t *conn, gchar *ssid )
{
  if(!g_strcmp0(conn->ssid, ssid))
    nm_conn_forget(path);
}

static void nm_active_handle ( const gchar *path, gchar *ifa, GVariant *dict )
{
  GVariantIter *iter;
  GList *liter;
  nm_active_t *active;
  gchar *dev_path, *ap_path, *type;

  if(g_variant_lookup(dict, "Type", "&s", &type) &&
      g_strcmp0(type, "802-11-wireless"))
    return;

  hash_table_lock(active_list);
  if( !(active = hash_table_lookup(active_list, path)) )
  {
    active = g_malloc0(sizeof(nm_active_t));
    active->path = g_strdup(path);
    hash_table_insert(active_list, active->path, active);
  }
  if(g_variant_lookup(dict, "SpecificObject", "&o", &ap_path) &&
      g_strcmp0(active->ap_path, ap_path))
  {
    str_assign(&active->ap_path, g_strdup(ap_path));
    if(nm_ap_node_active_update(ap_path) && active->ap)
      nm_apoint_update(active->ap);
  }

  if(g_variant_lookup(dict, "Devices", "ao", &iter))
  {
    g_mutex_lock(&device_lock);
    while(g_variant_iter_next(iter, "&o", &dev_path))
      for(liter=devices; liter; liter=g_list_next(liter))
        if(!g_strcmp0(NM_DEVICE(liter->data)->path, dev_path))
        {
          NM_DEVICE(liter->data)->active = active;
          active->device = liter->data;
        }
    g_mutex_unlock(&device_lock);
    g_variant_iter_free(iter);
  }
  hash_table_unlock(active_list);

  g_debug("nm: connected to: %s", path);
}

static void nm_active_free ( nm_active_t *active )
{
  g_debug("nm: disconnected from: %s", active->path);

  if(active->device)
    active->device->active = NULL;
  hash_table_lock(ap_nodes);
  if(active->ap && active->ap->active_node)
    active->ap->active_node->active = NULL;
  hash_table_unlock(ap_nodes);
  if(active->ap)
    active->ap->active_node = NULL;
  g_free(active->path);
  g_free(active);
}

static void nm_device_free ( gchar *path )
{
  GList *iter;
  nm_device_t *device;

  g_mutex_lock(&device_lock);
  for(iter=devices; iter; iter=g_list_next(iter))
    if(!g_strcmp0(path, NM_DEVICE(iter->data)->path))
      break;

  if(!iter)
  {
    g_mutex_unlock(&device_lock);
    return;
  }
  device = iter->data;
  g_debug("nm: removing interface %s", device->name);

  devices = g_list_remove(devices, device);
  if(default_dev==device)
    default_dev = NULL;
  if(!default_dev && devices)
    default_dev = devices->data;

  if(device->active)
    device->active->device = NULL;
  g_free(device->name);
  g_free(device->path);
  g_free(device);
  g_mutex_unlock(&device_lock);
}

static void nm_device_handle ( const gchar *path, gchar *ifa, GVariant *dict )
{
  nm_device_t *iface;
  GList *iter;
  gchar *name;
  gint32 type;

  if(!g_variant_lookup(dict, "DeviceType", "u", &type) || type != 2)
    return; // Ignore non Wi-Fi devices
  if(!g_variant_lookup(dict, "Interface", "&s", &name))
    return;

  g_mutex_lock(&device_lock);
  for(iter=devices; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((nm_device_t *)iter->data)->path, path))
      break;

  if(iter)
    iface = iter->data;
  else
  {
    iface = g_malloc0(sizeof(nm_device_t));
    iface->path = g_strdup(path);
    devices = g_list_append(devices, iface);
    if(!default_dev)
      default_dev = iface;
  }
  if(g_strcmp0(iface->name, name))
  {
    g_free(iface->name);
    iface->name = g_strdup(name);
  }
  g_mutex_unlock(&device_lock);
}

static void nm_object_handle ( const gchar *path, GVariantIter *iter )
{
  GVariant *dict;
  gchar *iface;

  while(g_variant_iter_next(iter, "{&s@a{sv}}", &iface, &dict))
  {
    if(!g_strcmp0(iface, nm_iface_apoint))
      nm_ap_node_handle(path, iface, dict);
    else if(!g_strcmp0(iface, nm_iface_conn))
      g_dbus_connection_call(nm_con, nm_iface, path, nm_iface_conn,
          "GetSettings", NULL, G_VARIANT_TYPE("(a{sa{sv}})"),
          G_DBUS_CALL_FLAGS_NONE, -1, NULL,
          (GAsyncReadyCallback)nm_conn_handle_cb, g_strdup(path));
    else if(!g_strcmp0(iface, nm_iface_device))
      nm_device_handle(path, iface, dict);
    else if(!g_strcmp0(iface, nm_iface_active))
      nm_active_handle(path, iface, dict);
    else if(!g_strcmp0(iface, nm_iface_agtmgr))
      g_dbus_connection_call(nm_con, nm_iface, path, iface, "Register",
          g_variant_new("(s)", "org.hosers.sfwbar"), NULL,
          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    g_variant_unref(dict);
  }
  g_variant_iter_free(iter);
}

static void nm_object_new ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariantIter *iter;
  gchar *object;

  g_variant_get(params, "(&oa{sa{sv}})", &object, &iter);
  nm_object_handle(object, iter);
}

static void nm_object_removed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *oiface, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariantIter *iter;
  gchar *object, *iface;

  g_variant_get(params, "(&oas)", &object, &iter);
  while(g_variant_iter_next(iter, "&s", &iface))
  {
    if(!g_strcmp0(iface, nm_iface_device))
      nm_device_free(object);
    else if(!g_strcmp0(iface, nm_iface_apoint))
      hash_table_remove(ap_nodes, object);
    else if(!g_strcmp0(iface, nm_iface_conn))
      hash_table_remove(conn_list, object);
    else if(!g_strcmp0(iface, nm_iface_active))
      hash_table_remove(active_list, object);
  }
  g_variant_iter_free(iter);
}

static void nm_object_changed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *ifa, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariant *dict;
  gchar *iface;
  guint32 state;

  g_variant_get(params,"(&s@a{sv}@as)", &iface, &dict, NULL);
  if(!g_strcmp0(iface, nm_iface_apoint))
    nm_ap_node_handle(path, NULL, dict);
  if(!g_strcmp0(iface, nm_iface_active))
    nm_active_handle(path, NULL, dict);
  else if(!g_strcmp0(iface, nm_iface_wireless))
  {
    if(g_variant_lookup(dict, "LastScan", "x", NULL))
      trigger_emit("wifi-scan-complete");
  }
  else if(!g_strcmp0(iface, nm_iface_active))
  {
    if(g_variant_lookup(dict, "State", "u", &state))
    {
      if(state == 4)
        nm_conn_forget(hash_table_lookup(new_conns, path));
      if(state == 2 || state == 4)
        hash_table_remove(new_conns, path);
    }
  }
  g_variant_unref(dict);
}

static void nm_object_list_cb ( GDBusConnection *con, GAsyncResult *res,
    gpointer data )
{
  GVariant *result;
  GVariantIter *iter, *iiter;
  gchar *path;

  if( !(result = g_dbus_connection_call_finish(con, res, NULL)) )
    return;

  g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
  while(g_variant_iter_next(iter, "{&oa{sa{sv}}}", &path, &iiter))
    nm_object_handle(path, iiter);
  g_variant_iter_free(iter);

  g_variant_unref(result);
}

static void nm_name_appeared_cb (GDBusConnection *con, const gchar *name,
    const gchar *owner, gpointer d)
{
  g_free(nm_owner);
  nm_owner = g_strdup(owner);
  module_interface_activate(&sfwbar_interface);
}

static void nm_name_disappeared_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
  module_interface_deactivate(&sfwbar_interface);
}

static void nm_secret_agent_method(GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *method, GVariant *parameters,
    GDBusMethodInvocation *inv, gpointer data)
{
  nm_dialog_t *dialog;
  gchar *opath;

  if(!g_strcmp0(method, "GetSecrets") && nm_passphrase_prompt(parameters, inv))
    return;
  if(!g_strcmp0(method, "CancelGetSecrets"))
  {
    g_variant_get(parameters, "(&o&s)", &opath, NULL);
    hash_table_lock(dialog_list);
    if( (dialog = hash_table_lookup(dialog_list, opath)) )
      nm_button_clicked(dialog->cancel, dialog);
    hash_table_unlock(dialog_list);

    if(dialog)
      return;
  }

  g_dbus_method_invocation_return_dbus_error(inv,
        "org.freedesktop.NetworkManager.SecretAgent.UserCanceled", "");
}

static value_t nm_ap_get_info ( nm_apoint_t *ap, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "ssid"))
    return value_new_string(g_strdup(ap->ssid? ap->ssid: ""));
  if(!g_ascii_strcasecmp(prop, "type"))
    return value_new_string(g_strdup(nm_apoint_get_type(ap)));
  if(!g_ascii_strcasecmp(prop, "known"))
    return value_new_numeric(!!ap->conn);
  if(!g_ascii_strcasecmp(prop, "strength"))
    return value_new_numeric(ap->strength);
  if(!g_ascii_strcasecmp(prop, "connected"))
    return value_new_numeric(!!ap->active_node);

  return value_na;
}

static value_t nm_expr_get ( vm_t *vm, value_t p[], gint np )
{
  nm_device_t *device;
  nm_apoint_t *ap;
  value_t v1 = value_na;
  GList *l;

  vm_param_check_np_range(vm, np, 1, 2, "WifiGet");
  vm_param_check_string(vm, p, 0, "WifiGet");
  if(np==2)
    vm_param_check_string(vm, p, 1, "WifiGet");

  hash_table_lock(apoint_list);
  if(np==2 && (ap = hash_table_lookup(apoint_list, value_get_string(p[0]))) )
    v1 = nm_ap_get_info(ap, value_get_string(p[1]));
  hash_table_unlock(apoint_list);
  if(!value_is_na(v1))
    return v1;

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "DeviceStrength"))
  {
    g_mutex_lock(&device_lock);
    l = g_list_find_custom(devices,
        np==2? value_get_string(p[1]) : NULL,
        (GCompareFunc)nm_device_name_cmp);
    device = l? l->data : default_dev;
    v1 = (device && !device->active && !device->active->ap)?
      value_new_string(g_strdup_printf("%d", device->active->ap->strength)) :
      value_na;
    g_mutex_unlock(&device_lock);
    return v1;
  }
  return value_na;
}

static value_t nm_action_scan ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 0, "WifiScan");

  g_mutex_lock(&device_lock);
  if(default_dev)
  {
    trigger_emit("wifi-scan");
    g_dbus_connection_call(nm_con, nm_iface, default_dev->path,
        nm_iface_wireless, "RequestScan", g_variant_new("(a{sv})", NULL),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  }
  g_mutex_unlock(&device_lock);

  return value_na;
}

static value_t nm_action_connect ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WifiConnect");
  vm_param_check_string(vm, p, 0, "WifiConnect");

  nm_conn_new(value_get_string(p[0]));

  return value_na;
}

static value_t nm_action_disconnect ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WifiDisconnect");
  vm_param_check_string(vm, p, 0, "WifiDIsconnect");

  nm_active_disconnect(value_get_string(p[0]));

  return value_na;
}

static value_t nm_action_forget ( vm_t *vm, value_t p[], gint np )
{
  nm_apoint_t *ap;

  vm_param_check_np(vm, np, 1, "WifiForget");
  vm_param_check_string(vm, p, 0, "WifiForget");

  if( (ap = hash_table_lookup(apoint_list, value_get_string(p[0]))) )
    hash_table_foreach(conn_list, (GHFunc)nm_conn_forget_each, ap->ssid);

  return value_na;
}

static void nm_activate ( void )
{
  ap_nodes = hash_table_new_full(g_str_hash, g_str_equal,
      NULL, (GDestroyNotify)nm_ap_node_free);
  dialog_list = hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  new_conns = hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  conn_list = hash_table_new_full(g_str_hash, g_str_equal, NULL,
      (GDestroyNotify)nm_conn_free);
  active_list = hash_table_new_full(g_str_hash, g_str_equal, NULL,
      (GDestroyNotify)nm_active_free);
  apoint_list = hash_table_new_full(g_str_hash, g_str_equal, NULL,
      (GDestroyNotify)nm_apoint_free);

  vm_func_add("wifiget", nm_expr_get, FALSE, TRUE);
  vm_func_add("wifiscan", nm_action_scan, FALSE, TRUE);
  vm_func_add("wificonnect", nm_action_connect, FALSE, TRUE);
  vm_func_add("wifidisconnect", nm_action_disconnect, FALSE, TRUE);
  vm_func_add("wififorget", nm_action_forget, FALSE, TRUE);
  sub_add = g_dbus_connection_signal_subscribe(nm_con, nm_owner,
      nm_iface_objmgr, "InterfacesAdded", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, nm_object_new, NULL, NULL);

  sub_del = g_dbus_connection_signal_subscribe(nm_con, nm_owner,
      nm_iface_objmgr, "InterfacesRemoved", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, nm_object_removed, NULL, NULL);

  sub_chg = g_dbus_connection_signal_subscribe(nm_con, nm_owner,
      nm_iface_objprop, "PropertiesChanged", NULL, NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, nm_object_changed, NULL, NULL);

  g_dbus_connection_call(nm_con, nm_iface, "/org/freedesktop", nm_iface_objmgr,
      "GetManagedObjects", NULL, G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
      G_DBUS_CALL_FLAGS_NONE, -1, NULL, (GAsyncReadyCallback)nm_object_list_cb,
      NULL);
}

static void nm_deactivate ( void )
{
  g_dbus_connection_signal_unsubscribe(nm_con, sub_add);
  g_dbus_connection_signal_unsubscribe(nm_con, sub_del);
  g_dbus_connection_signal_unsubscribe(nm_con, sub_chg);
  g_clear_pointer(&dialog_list, hash_table_remove_all);
  g_clear_pointer(&new_conns, hash_table_remove_all);
  g_clear_pointer(&active_list, hash_table_remove_all);
  g_clear_pointer(&conn_list, hash_table_remove_all);
  g_clear_pointer(&ap_nodes, hash_table_remove_all);

  sfwbar_interface.active = FALSE;
}

static void nm_finalize ( void )
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
  static GDBusInterfaceVTable nm_secret_agent_vtable = {
    (GDBusInterfaceMethodCallFunc)nm_secret_agent_method, NULL, NULL };

  nm_con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
  node = g_dbus_node_info_new_for_xml(nm_secret_agent_xml, NULL);
  g_dbus_connection_register_object (nm_con, nm_path_secret,
      node->interfaces[0], &nm_secret_agent_vtable, NULL, NULL, NULL);
  g_dbus_node_info_unref(node);

  g_bus_watch_name(G_BUS_TYPE_SYSTEM, nm_iface, G_BUS_NAME_WATCHER_FLAGS_NONE,
      nm_name_appeared_cb, nm_name_disappeared_cb, NULL, NULL);
  return TRUE;
}

ModuleInterfaceV1 sfwbar_interface = {
  .interface = "wifi",
  .provider = "NetworkManager",
  .activate = nm_activate,
  .deactivate = nm_deactivate,
  .finalize = nm_finalize
};
