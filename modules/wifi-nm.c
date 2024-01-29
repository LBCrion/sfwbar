#include <gtk/gtk.h>
#include <gio/gio.h>
#include "../src/module.h"
#include "../src/basewidget.h"

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
  nm_apoint_t *ap;
};

struct _nm_apoint {
  GList *nodes;
  gchar *ssid;
  gchar *hash;
  guint flags, wpa, rsn, mode;
  guint8 strength;
  gboolean visible;
  nm_conn_t *conn;
  nm_active_t *active;
};

struct _nm_conn {
  gchar *path;
  gchar *ssid;
  nm_active_t *active;
};

struct _nm_active {
  gchar *path;
  gchar *conn;
};

struct _nm_device {
  gchar *path;
  gchar *name;
  guint sub;
  guint del;
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
guint16 sfwbar_module_version = 2;

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
static GHashTable *ap_nodes, *dialog_list, *new_conns, *conn_list;
static GHashTable *active_list, *apoint_list;
static GList *interfaces;
static nm_device_t *default_iface;
static guint sub_add, sub_del, sub_chg;
static guint32 nm_strength_threshold;

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
  return "wpa";
}

static nm_apoint_t *nm_apoint_dup ( nm_apoint_t *src )
{
  nm_apoint_t *dest;

  if(!src)
    return NULL;

  dest = g_malloc0(sizeof(nm_apoint_t));
  dest->ssid = g_strdup(src->ssid);
  dest->hash = g_strdup(src->hash);
  dest->flags = src->flags;
  dest->wpa = src->wpa;
  dest->rsn = src->rsn;
  dest->mode = src->mode;
  dest->strength = src->strength;
  dest->conn = src->conn;
  dest->active = src->active;

  return dest;
}

static void nm_apoint_free ( nm_apoint_t *apoint )
{
  g_free(apoint->ssid);
  g_free(apoint->hash);
  g_free(apoint);
}

static gint nm_apoint_comp ( nm_apoint_t *ap1, nm_apoint_t *ap2 )
{
  return (g_strcmp0(ap1->ssid, ap2->ssid) ||
      ap1->flags != ap2->flags ||
      ap1->wpa != ap2->wpa ||
      ap1->rsn != ap2->rsn ||
      ap1->mode != ap2->mode);
}

static nm_conn_t *nm_apoint_get_conn( nm_apoint_t *ap )
{
  nm_conn_t *conn, *result = NULL;
  GHashTableIter iter;

  g_hash_table_iter_init(&iter, conn_list);
  while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&conn))
    if(!g_strcmp0(ap->ssid, conn->ssid) && (!result || conn->active))
      result = conn;

  return result;
}

static void *nm_apoint_get_str ( nm_apoint_t *ap, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "ssid"))
    return g_strdup(ap->ssid?ap->ssid:"");
  if(!g_ascii_strcasecmp(prop, "path"))
    return g_strdup(ap->hash?ap->hash:"");
  if(!g_ascii_strcasecmp(prop, "type"))
    return g_strdup(nm_apoint_get_type(ap));
  if(!g_ascii_strcasecmp(prop, "known"))
    return g_strdup(ap->conn?ap->ssid:"");
  if(!g_ascii_strcasecmp(prop, "strength"))
    return g_strdup_printf("%d", ap->strength);
  if(!g_ascii_strcasecmp(prop, "connected"))
    return g_strdup_printf("%d", !!ap->active);

  return NULL;
}

static module_queue_t update_q = {
  .free = (void (*)(void *))nm_apoint_free,
  .duplicate = (void *(*)(void *))nm_apoint_dup,
  .get_str = (void *(*)(void *, gchar *))nm_apoint_get_str,
  .get_num = (void *(*)(void *, gchar *))NULL,
  .compare = (gboolean (*)(const void *, const void *))nm_apoint_comp,
};

static void *nm_remove_get_str ( gchar *name, gchar *prop )
{
  if(!g_ascii_strcasecmp(prop, "RemovedPath"))
    return g_strdup(name);
  else
    return NULL;
}

static module_queue_t remove_q = {
  .free = g_free,
  .duplicate = (void *(*)(void *))g_strdup,
  .get_str = (void *(*)(void *, gchar *))nm_remove_get_str,
  .compare = (gboolean (*)(const void *, const void *))g_strcmp0,
};

static void nm_apoint_update ( nm_apoint_t *ap )
{
  if(ap->visible)
    module_queue_append(&update_q, ap);
  else
    module_queue_append(&remove_q, ap->hash);

  g_message("ap: %s, %s, known: %d, conn: %d, strength: %d", ap->ssid,
      nm_apoint_get_type(ap), !!ap->conn, !!ap->active, ap->strength);
}

static gboolean nm_apoint_xref ( nm_apoint_t *ap )
{
  nm_conn_t *conn;
  nm_active_t *active;
  gboolean change = FALSE;

  conn = nm_apoint_get_conn(ap);
  if(ap->conn != conn)
  {
    ap->conn = conn;
    change = TRUE;
  }

  active = conn?conn->active:NULL;
  if(ap->active != active)
  {
    ap->active = active;
    change = TRUE;
  }

  return change;
}

static void nm_apoint_xref_all ( void )
{
  nm_apoint_t *ap;
  GHashTableIter iter;

  g_hash_table_iter_init(&iter, apoint_list);
  while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&ap))
    if(nm_apoint_xref(ap))
      nm_apoint_update(ap);
}

static void nm_conn_free ( gchar *path )
{
  nm_conn_t *conn;

  if( !(conn = g_hash_table_lookup(conn_list, path)) )
    return;

  g_hash_table_remove(conn_list, path);
  g_free(conn->ssid);
  g_free(conn->path);
  g_free(conn);

  nm_apoint_xref_all();

}

static void nm_conn_connect ( gchar *path )
{
  g_message("connecting: %s", path);
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
  g_hash_table_remove(dialog_list, dialog->path);
}

static void nm_button_clicked ( GtkWidget *button, nm_dialog_t *dialog )
{
  if(button==dialog->ok)
    g_dbus_method_invocation_return_value(dialog->invocation,
        g_variant_new("(s)", gtk_entry_get_text(GTK_ENTRY(dialog->pass))));
  else
    g_dbus_method_invocation_return_dbus_error(dialog->invocation,
        "org.freedesktop.NetworkManager.SecretAgent.UserCanceled", "");
  gtk_widget_destroy(dialog->win);
  g_hash_table_remove(dialog_list, dialog->path);
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
  g_hash_table_remove(dialog_list, dialog->path);
  return FALSE;
}

static gboolean nm_passphrase_prompt ( GVariant *vconn, gpointer inv )
{
  nm_dialog_t *dialog;
  nm_conn_t *conn;
  GtkWidget *grid, *label;
  gchar *title, *path;
  gboolean user = FALSE;

  g_variant_get(vconn,"(@a{sa{sv}}&osasu)", NULL, &path, NULL, NULL, NULL);
  if( !(conn = g_hash_table_lookup(conn_list, path)) )
    return FALSE;

  title = g_strdup_printf("Passphrase for network %s", conn->ssid);
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
  gtk_widget_set_name(label, "iwd_dialog_title");
  gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 2, 1);

  if(user)
  {
    label = gtk_label_new("Username:");
    gtk_widget_set_name(label, "iwd_user_label");
    gtk_grid_attach(GTK_GRID(grid),label, 1, 2, 1, 1);
    dialog->user = gtk_entry_new();
    gtk_widget_set_name(dialog->user, "iwd_user_entry");
    gtk_grid_attach(GTK_GRID(grid), dialog->user, 2, 2, 1, 1);
    g_signal_connect(G_OBJECT(dialog->user), "changed",
        G_CALLBACK(nm_passphrase_changed_cb), dialog);
  }

  label = gtk_label_new("Passphrase:");
  gtk_widget_set_name(label, "iwd_passphrase_label");
  gtk_grid_attach(GTK_GRID(grid), label, 1, 3, 1, 1);
  dialog->pass = gtk_entry_new();
  gtk_widget_set_name(dialog->pass, "iwd_passphrase_entry");
  gtk_entry_set_visibility(GTK_ENTRY(dialog->pass), FALSE);
  g_signal_connect(G_OBJECT(dialog->pass), "activate",
      G_CALLBACK(nm_passphrase_cb), dialog);
    g_signal_connect(G_OBJECT(dialog->pass), "changed",
        G_CALLBACK(nm_passphrase_changed_cb), dialog);
  gtk_grid_attach(GTK_GRID(grid), dialog->pass, 2, 3, 1, 1);

  dialog->ok = gtk_button_new_with_label("Ok");
  gtk_widget_set_name(dialog->ok, "iwd_button_ok");
  gtk_grid_attach(GTK_GRID(grid), dialog->ok, 1, 4, 1, 1);
  gtk_widget_set_sensitive(dialog->ok, FALSE);
  g_signal_connect(G_OBJECT(dialog->ok), "clicked",
      G_CALLBACK(nm_button_clicked), dialog);

  dialog->cancel = gtk_button_new_with_label("Cancel");
  gtk_widget_set_name(dialog->cancel, "iwd_button_cancel");
  gtk_grid_attach(GTK_GRID(grid), dialog->cancel, 2, 4, 1, 1);
  g_signal_connect(G_OBJECT(dialog->cancel), "clicked",
      G_CALLBACK(nm_button_clicked), dialog);

  g_message("dialog: %s", path);
  g_object_ref_sink(G_OBJECT(dialog->win));
  dialog->path = g_strdup(path);
  g_hash_table_insert(dialog_list, dialog->path, dialog);
  gtk_widget_show_all(dialog->win);

  return TRUE;
}

static void nm_conn_new_cb ( GDBusConnection *con, GAsyncResult *res,
    gpointer d )
{
  GVariant *result;
  gchar *path;

  g_message("connected");
  if( (result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    g_variant_get(result, "(&o&o)", NULL, &path);
    g_hash_table_insert(new_conns, g_strdup(path), path);

    g_variant_unref(result);
  }
}

static void nm_conn_new ( nm_apoint_t *ap )
{
  GVariant *conn_entry[3], *wifi_entry[2], *wsec_entry[2], *entry[5];
  GVariant *dict, *ipv4_entry, *ipv6_entry;

  if((ap->rsn & 0x00000200) || (ap->wpa & 0x00000200))
    return; // we don't support 802.11x

  if(ap->conn)
  {
    nm_conn_connect(ap->conn->path);
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

  g_dbus_connection_call(nm_con, nm_iface, nm_path, nm_iface,
      "AddAndActivateConnection",
      g_variant_new("(@a{sa{sv}}oo)", dict, default_iface->path, "/"),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)nm_conn_new_cb, NULL);
}

static void nm_conn_handle_cb ( GDBusConnection *con, GAsyncResult *res,
    gchar *path )
{
  nm_conn_t *conn;
  nm_active_t *active;
  GVariant *result, *dict, *odict;
  GHashTableIter iter;
  gchar *ssid;

  if( (result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    g_variant_get(result,"(@a{sa{sv}})", &odict);
    if(g_variant_lookup(odict, "802-11-wireless", "@a{sv}", &dict))
    {
      if( (ssid = nm_ssid_get(dict, "ssid")) )
      {
        if( !(conn = g_hash_table_lookup(conn_list, path)) )
        {
          conn = g_malloc0(sizeof(nm_conn_t));
          conn->path = g_strdup(path);
          conn->ssid = ssid;
          g_hash_table_insert(conn_list, conn->path, conn);
          g_hash_table_iter_init(&iter, active_list);
          while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&active))
            if(!g_strcmp0(active->conn, conn->path))
              conn->active = active;
          nm_apoint_xref_all();
          g_message("conn: %s (%s)", ssid, path);
        }
        else
          g_free(ssid);
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

  if( (apoint=node->ap) )
    if( !(apoint->nodes = g_list_remove(apoint->nodes, node)) )
    {
      g_hash_table_remove(apoint_list, apoint->hash);
      g_message("ap removed: %s", apoint->ssid);
      module_queue_append(&remove_q, apoint->hash);
      nm_apoint_free(apoint);
    }

  g_free(node->path);
  g_free(node);
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

  if( !(node = g_hash_table_lookup(ap_nodes, path)) )
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
    if( (apoint = g_hash_table_lookup(apoint_list, hash)) )
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
      g_hash_table_insert(apoint_list, apoint->hash, apoint);
    }
    apoint->nodes = g_list_prepend(apoint->nodes, node);
    node->ap = apoint;
    g_hash_table_insert(ap_nodes, node->path, node);
    change = TRUE;
  }
  apoint = node->ap;

  change |= nm_apoint_xref(apoint);

  if(g_variant_lookup(dict, "Strength", "y", &strength))
  {
    node->strength = CLAMP(strength, 0, 100);
    ap_strength = 0;
    for(iter=apoint->nodes; iter; iter=g_list_next(iter))
      ap_strength = MAX(ap_strength, NM_AP_NODE(iter->data)->strength);
    if((gint)(apoint->strength/10) != (gint)(ap_strength/10))
    {
      node->ap->strength = ap_strength;
      change = TRUE;
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

static void nm_active_disconnect ( nm_active_t *active )
{
  if(!active)
    return;

  g_message("deactivate: %s", active->path);
  g_dbus_connection_call(nm_con, nm_iface, nm_path, nm_iface,
      "DeactivateConnection", g_variant_new("(o)", active->path),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void nm_conn_forget ( nm_conn_t *conn )
{
  if(!conn)
    return;
  g_message("forget: %s", conn->path);
  g_dbus_connection_call(nm_con, nm_iface, conn->path, nm_iface_conn, "Delete",
      NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void nm_active_handle ( const gchar *path, gchar *ifa, GVariant *dict )
{
  nm_active_t *active;
  nm_conn_t *conn;
  gchar *conn_path, *type;

  if(!g_variant_lookup(dict, "Type", "&s", &type) ||
      g_strcmp0(type, "802-11-wireless"))
    return;
  if(!g_variant_lookup(dict, "Connection", "&o", &conn_path))
    return;

  if(g_hash_table_lookup(active_list, path))
    return;

  active = g_malloc0(sizeof(nm_active_t));
  active->path = g_strdup(path);
  active->conn = g_strdup(conn_path);
  g_hash_table_insert(active_list, active->path, active);

  if((conn = g_hash_table_lookup(conn_list, conn_path)) && !conn->active)
  {
    conn->active = active;
    nm_apoint_xref_all();
  }
  g_message("nm: connected: %s", path);
}

static void nm_active_free ( gchar *path )
{
  nm_active_t *active;
  nm_conn_t *conn;

  if( !(active = g_hash_table_lookup(active_list, path)) )
    return;
  g_debug("nm: disconnected: %s", path);

  if( (conn = g_hash_table_lookup(conn_list, active->conn)) )
  {
    conn->active = NULL;
    nm_apoint_xref_all();
  }
  g_hash_table_remove(active_list, active->path);
  g_free(active->path);
  g_free(active->conn);
  g_free(active);
}

static void nm_device_free ( gchar *path )
{
  GList *iter;
  nm_device_t *iface;

  for(iter=interfaces; iter; iter=g_list_next(iter))
    if(!g_strcmp0(path, NM_DEVICE(iter->data)->path))
      break;

  if(!iter)
    return;
  iface = iter->data;
  g_debug("nm: removing interface %s", iface->name);

  interfaces = g_list_remove(interfaces, iface);
  if(default_iface==iface)
    default_iface = NULL;
  if(!default_iface && interfaces)
    default_iface = interfaces->data;

  g_free(iface->name);
  g_free(iface->path);
  g_free(iface);
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

  for(iter=interfaces; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((nm_device_t *)iter->data)->path, path))
      break;

  if(iter)
    iface = iter->data;
  else
  {
    iface = g_malloc0(sizeof(nm_device_t));
    iface->path = g_strdup(path);
    interfaces = g_list_append(interfaces, iface);
    if(!default_iface)
      default_iface = iface;
  }
  if(g_strcmp0(iface->name, name))
  {
    g_free(iface->name);
    iface->name = g_strdup(name);
  }
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
      g_hash_table_remove(ap_nodes, object);
    else if(!g_strcmp0(iface, nm_iface_conn))
      nm_conn_free(object);
    else if(!g_strcmp0(iface, nm_iface_active))
      nm_active_free(object);
  }
  g_variant_iter_free(iter);
}

static void nm_object_changed ( GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *ifa, const gchar *signal,
    GVariant *params, gpointer data )
{
  GVariant *dict;
  gchar *iface;
  nm_active_t *active;
  guint32 state;

  g_variant_get(params,"(&s@a{sv}@as)", &iface, &dict, NULL);
  if(!g_strcmp0(iface, nm_iface_apoint))
    nm_ap_node_handle((gchar *)path, NULL, dict);
  else if(!g_strcmp0(iface, nm_iface_wireless))
  {
    if(g_variant_lookup(dict, "LastScan", "x", NULL))
      g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
          (gpointer)g_intern_static_string("nm_scan_complete"));
  }
  else if(!g_strcmp0(iface, nm_iface_active))
  {
    if(g_variant_lookup(dict, "State", "u", &state))
    {
      if(state == 2)
        g_hash_table_remove(new_conns, path);
      else if(state == 4 && g_hash_table_lookup(new_conns, path))
      {
        if( (active = g_hash_table_lookup(active_list, path)) )
          nm_conn_forget(g_hash_table_lookup(conn_list, active->conn));
      }
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

void nm_name_appeared_cb (GDBusConnection *con, const gchar *name,
    const gchar *owner, gpointer d)
{
  sub_add = g_dbus_connection_signal_subscribe(con, owner, nm_iface_objmgr,
      "InterfacesAdded", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
      nm_object_new, NULL, NULL);

  sub_del = g_dbus_connection_signal_subscribe(con, owner, nm_iface_objmgr,
      "InterfacesRemoved", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
      nm_object_removed, NULL, NULL);

  sub_chg = g_dbus_connection_signal_subscribe(con, owner, nm_iface_objprop,
      "PropertiesChanged", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
      nm_object_changed, NULL, NULL);

  g_dbus_connection_call(con, nm_iface, "/org/freedesktop", nm_iface_objmgr,
      "GetManagedObjects", NULL, G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
      G_DBUS_CALL_FLAGS_NONE, -1, NULL, (GAsyncReadyCallback)nm_object_list_cb,
      NULL);
}

void nm_name_disappeared_cb (GDBusConnection *con, const gchar *name,
    gpointer d)
{
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
  if(!g_strcmp0(method,"CancelGetSecrets"))
  {
    g_variant_get(parameters, "(&o&s)", &opath, NULL);
    if( (dialog = g_hash_table_lookup(dialog_list, opath)) )
    {
      g_message("CancelGetSecrets: %p %s", dialog, opath);
      nm_button_clicked(dialog->cancel, dialog);
      return;
    }
  }

  g_dbus_method_invocation_return_dbus_error(inv,
        "org.freedesktop.NetworkManager.SecretAgent.UserCanceled", "");
}

gboolean sfwbar_module_init ( void )
{
  GDBusNodeInfo *node;
  static GDBusInterfaceVTable nm_secret_agent_vtable = {
    (GDBusInterfaceMethodCallFunc)nm_secret_agent_method, NULL, NULL };

  update_q.trigger = g_intern_static_string("nm_updated");
  remove_q.trigger = g_intern_static_string("nm_removed");
  ap_nodes = g_hash_table_new_full(g_str_hash, g_str_equal,
      NULL, (GDestroyNotify)nm_ap_node_free);
  dialog_list = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  new_conns = g_hash_table_new(g_str_hash, g_str_equal);
  conn_list = g_hash_table_new(g_str_hash, g_str_equal);
  active_list = g_hash_table_new(g_str_hash, g_str_equal);
  apoint_list = g_hash_table_new(g_str_hash, g_str_equal);

  nm_con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
  node = g_dbus_node_info_new_for_xml(nm_secret_agent_xml, NULL);
  g_dbus_connection_register_object (nm_con, nm_path_secret,
      node->interfaces[0], &nm_secret_agent_vtable, NULL, NULL, NULL);
  g_dbus_node_info_unref(node);

  g_bus_watch_name(G_BUS_TYPE_SYSTEM, nm_iface, G_BUS_NAME_WATCHER_FLAGS_NONE,
      nm_name_appeared_cb, nm_name_disappeared_cb, NULL, NULL);
  return TRUE;
}

static void *nm_expr_get ( void **params, void *widget, void *event )
{
  gchar *result;

  if(!params || !params[0])
    return g_strdup("");

  if( (result = module_queue_get_string(&update_q, params[0])) )
    return result;

  if( (result = module_queue_get_string(&remove_q, params[0])) )
    return result;

  return g_strdup("");
}

static void nm_action_ack ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  module_queue_remove(&update_q);
}

static void nm_action_ack_removed ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  module_queue_remove(&remove_q);
}

static void nm_action_scan ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(!default_iface)
    return;
  g_main_context_invoke(NULL, (GSourceFunc)base_widget_emit_trigger,
      (gpointer)g_intern_static_string("nm_scan"));
  g_dbus_connection_call(nm_con, nm_iface, default_iface->path,
      nm_iface_wireless, "RequestScan", g_variant_new("(a{sv})", NULL),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void nm_action_connect ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  nm_apoint_t *ap;

  if(cmd && (ap = g_hash_table_lookup(apoint_list, cmd)) )
    nm_conn_new(ap);
}

static void nm_action_disconnect ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  nm_apoint_t *ap;

  if(cmd && (ap = g_hash_table_lookup(apoint_list, cmd)) )
    nm_active_disconnect(ap->active);
}

static void nm_action_forget ( gchar *cmd, gchar *name, void *d1,
    void *d2, void *d3, void *d4 )
{
  nm_apoint_t *ap;

  if(cmd && (ap = g_hash_table_lookup(apoint_list, cmd)) )
    nm_conn_forget(ap->conn);
}

ModuleExpressionHandlerV1 get_handler = {
  .flags = 0,
  .name = "NmGet",
  .parameters = "Ss",
  .function = nm_expr_get
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &get_handler,
  NULL
};

ModuleActionHandlerV1 ack_handler = {
  .name = "NmAck",
  .function = nm_action_ack
};

ModuleActionHandlerV1 ack_removed_handler = {
  .name = "NmAckRemoved",
  .function = nm_action_ack_removed
};

ModuleActionHandlerV1 scan_handler = {
  .name = "NmScan",
  .function = nm_action_scan
};

ModuleActionHandlerV1 connect_handler = {
  .name = "NmConnect",
  .function = nm_action_connect
};

ModuleActionHandlerV1 disconnect_handler = {
  .name = "NmDisconnect",
  .function = nm_action_disconnect
};

ModuleActionHandlerV1 forget_handler = {
  .name = "NmForget",
  .function = nm_action_forget
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &ack_handler,
  &ack_removed_handler,
  &scan_handler,
  &connect_handler,
  &disconnect_handler,
  &forget_handler,
  NULL
};
