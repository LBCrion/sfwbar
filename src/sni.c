/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- sfwbar maintainers
 */

#include <glib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include "sfwbar.h"
#include "trayitem.h"
#include "tray.h"

struct watcher_item {
  SniWatcher *watcher;
  gchar *uid;
  guint id;
};

static const gchar sni_watcher_xml[] =
  "<node>"
  " <interface name='org.%s.StatusNotifierWatcher'>"
  "  <method name='RegisterStatusNotifierItem'>"
  "   <arg type='s' name='service' direction='in'/>"
  "  </method>"
  "  <method name='RegisterStatusNotifierHost'>"
  "   <arg type='s' name='service' direction='in'/>"
  "  </method>"
  "  <signal name='StatusNotifierItemRegistered'>"
  "   <arg type='s' name='service'/>"
  "  </signal>"
  "  <signal name='StatusNotifierItemUnregistered'>"
  "   <arg type='s' name='service'/>"
  "  </signal>"
  "  <signal name='StatusNotifierHostRegistered'/>"
  "  <property type='as' name='RegisteredStatusNotifierItems' access='read'/>"
  "  <property type='b' name='IsStatusNotifierHostRegistered' access='read'/>"
  "  <property type='i' name='ProtocolVersion' access='read'/>"
  " </interface>"
  "</node>";

GDBusConnection *sni_get_connection ( void )
{
  static GDBusConnection *con;

  if(con)
    return con;

  con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  return con;
}

static void sni_watcher_item_free ( struct watcher_item *item )
{
  if(item->id)
    g_bus_unwatch_name(item->id);
  g_free(item->uid);
  g_free(item);
}

static void sni_watcher_item_lost_cb ( GDBusConnection *con, const gchar *name,
    struct watcher_item *item )
{
  g_debug("sni watcher %s: lost item: %s", item->watcher->iface, name);
  g_dbus_connection_emit_signal(con, NULL, "/StatusNotifierWatcher",
      item->watcher->iface, "StatusNotifierItemUnregistered",
      g_variant_new("(s)", item->uid), NULL);
  item->watcher->items = g_list_delete_link(item->watcher->items,
      g_list_find(item->watcher->items, item));
  sni_watcher_item_free(item);
}

static gint sni_watcher_item_add ( SniWatcher *watcher, gchar *uid )
{
  gchar *name;
  struct watcher_item *item;
  GList *iter;

  for(iter=watcher->items; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((struct watcher_item *)iter->data)->uid, uid))
      return -1;

  item = g_malloc0(sizeof(struct watcher_item));
  item->watcher = watcher;
  item->uid = g_strdup(uid);

  if(strchr(uid,'/'))
    name = g_strndup(uid, strchr(uid,'/')-uid);
  else
    name = g_strdup(uid);

  g_debug("sni watcher %s: watching item %s", watcher->iface, name);
  
  item->id = g_bus_watch_name(G_BUS_TYPE_SESSION, name,
      G_BUS_NAME_WATCHER_FLAGS_NONE, NULL,
      (GBusNameVanishedCallback)sni_watcher_item_lost_cb, item, NULL);
  watcher->items = g_list_prepend(watcher->items, item);
  g_free(name);
  return 0;
}

static void sni_watcher_method(GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *method,
    GVariant *parameters, GDBusMethodInvocation *invocation, SniWatcher *watcher)
{
  const gchar *parameter;
  gchar *name;

  g_variant_get (parameters, "(&s)", &parameter);
  g_dbus_method_invocation_return_value(invocation, NULL);

  if(!g_strcmp0(method, "RegisterStatusNotifierItem"))
  {
    if(*parameter!='/')
      name = g_strdup(parameter);
    else
      name = g_strconcat(sender, parameter, NULL);

    if(sni_watcher_item_add(watcher,name) != -1)
      g_dbus_connection_emit_signal(con, NULL, "/StatusNotifierWatcher",
          watcher->iface, "StatusNotifierItemRegistered",
          g_variant_new("(s)", name), NULL);
    g_free(name);
  }

  else if(!g_strcmp0(method,"RegisterStatusNotifierHost"))
  {
    watcher->watcher_registered = TRUE;
    g_debug("sni watcher %s: registered host %s", watcher->iface,parameter);
    g_dbus_connection_emit_signal(con, NULL, "/StatusNotifierWatcher",
        watcher->iface, "StatusNotifierHostRegistered", NULL, NULL);
  }
}

static GVariant *sni_watcher_get_prop (GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *prop,GError **error, SniWatcher *watcher )
{
  GVariant *ret = NULL;
  GVariantBuilder *builder;
  GList *iter;

  if (!g_strcmp0 (prop,"IsStatusNotifierHostRegistered"))
    ret = g_variant_new_boolean(TRUE);

  else if(!g_strcmp0 (prop,"ProtocolVersion"))
    ret = g_variant_new_int32(0);

  else if(!g_strcmp0 (prop,"RegisteredStatusNotifierItems"))
  {
    if(!watcher->items)
      ret = g_variant_new_array(G_VARIANT_TYPE_STRING, NULL, 0);
    else
    {
      builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY); 

      for(iter=watcher->items;iter;iter=g_list_next(iter))
        g_variant_builder_add_value(builder,
            g_variant_new_string(((struct watcher_item*)iter->data)->uid));
      ret = g_variant_builder_end (builder);

      g_variant_builder_unref (builder);
    }
  }
  return ret;
}

static const GDBusInterfaceVTable watcher_vtable =
{
  (GDBusInterfaceMethodCallFunc)sni_watcher_method,
  (GDBusInterfaceGetPropertyFunc)sni_watcher_get_prop,
  NULL
};

void sni_watcher_unregister_cb ( GDBusConnection *con, const gchar *name,
    SniWatcher *watcher)
{
  if(watcher->regid)
    g_dbus_connection_unregister_object(con, watcher->regid);
  watcher->regid = 0;
  g_list_free_full(watcher->items, (GDestroyNotify)sni_watcher_item_free);
  g_debug("sni watcher %s unregistered", watcher->iface);
}

void sni_watcher_register_cb ( GDBusConnection *con, const gchar *name,
    SniWatcher *watcher)
{
 // GList *iter;

  if(watcher->regid)
    g_dbus_connection_unregister_object(con, watcher->regid);

  watcher->regid = g_dbus_connection_register_object(con,
      "/StatusNotifierWatcher", watcher->idata->interfaces[0],
      &watcher_vtable, watcher, NULL, NULL);

  g_bus_own_name(G_BUS_TYPE_SESSION, watcher->iface,
      G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL,
      (GBusNameLostCallback)sni_watcher_unregister_cb, watcher, NULL);

//  for(iter=watcher->host->items; iter; iter=g_list_next(iter))
//    sni_watcher_item_add(watcher, ((SniItem *)iter->data)->uid);

  g_debug("sni watcher %s registered",watcher->iface);
}

static void sni_host_item_new ( GDBusConnection *con, SniHost *host,
    const gchar *uid)
{
  SniItem *sni;
  GList *iter;

  for(iter=host->items; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((SniItem *)iter->data)->uid, uid))
      return;

  sni = sni_item_new(con, host, uid);
  host->items = g_list_prepend(host->items, sni);

  g_debug("sni host %s: item registered: %s %s", host->iface, sni->dest,
      sni->path);
}

void sni_host_item_registered_cb ( GDBusConnection* con, const gchar* sender,
  const gchar* path, const gchar* iface, const gchar* signal,
  GVariant* parameters, gpointer data)
{
  const gchar *parameter;

  g_variant_get (parameters, "(&s)", &parameter);
  sni_host_item_new(con,data,parameter);
}

static void sni_host_list_cb ( GDBusConnection *con, GAsyncResult *res,
    SniHost *host)
{
  GVariant *result,*inner;
  GVariantIter *iter;
  gchar *str;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(!result)
    return;
  g_variant_get(result, "(v)", &inner);
  if(inner)
  {
    iter = g_variant_iter_new(inner);
    while(g_variant_iter_next(iter, "&s", &str))
      sni_host_item_new(con, host, str);
    g_variant_iter_free(iter);
    g_variant_unref(inner);
  }
  g_variant_unref(result);
}

static void sni_host_item_unregistered_cb ( GDBusConnection* con,
    const gchar* sender, const gchar* path, const gchar* interface,
    const gchar* signal, GVariant* parameters, SniHost *host )
{
  const gchar *parameter;
  GList *item;

  g_variant_get (parameters, "(&s)", &parameter);
  g_debug("sni host %s: unregister item %s", host->iface, parameter);

  for(item=host->items; item; item=g_list_next(item))
    if(!g_strcmp0(((SniItem *)item->data)->uid, parameter))
      break;
  if(!item)
    return;

  g_debug("sni host %s: removing item %s", host->iface, parameter);
  sni_item_free(item->data);
  host->items = g_list_delete_link(host->items, item);
}

static void sni_host_register_cb ( GDBusConnection *con, const gchar *name,
    const gchar *owner_name, SniWatcher *watcher )
{
  SniHost *host = watcher->host;
  g_dbus_connection_call(con, host->watcher, "/StatusNotifierWatcher",
    host->watcher, "RegisterStatusNotifierHost",
    g_variant_new("(s)", host->iface),
    G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, host);

  g_dbus_connection_signal_subscribe(con, NULL, watcher->iface,
      "StatusNotifierItemRegistered", "/StatusNotifierWatcher", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, sni_host_item_registered_cb,host, NULL);
  g_dbus_connection_signal_subscribe(con, NULL, watcher->iface,
      "StatusNotifierItemUnregistered", "/StatusNotifierWatcher", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,
      (GDBusSignalCallback)sni_host_item_unregistered_cb, host, NULL);
  g_dbus_connection_call(con, host->watcher, "/StatusNotifierWatcher",
    "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)",
      host->watcher,"RegisteredStatusNotifierItems"), NULL,
    G_DBUS_CALL_FLAGS_NONE, -1, NULL, (GAsyncReadyCallback)sni_host_list_cb,
    host);

  g_debug("sni host %s: found watcher %s (%s)", host->iface, name, owner_name);
}

static void sni_register ( gchar *name )
{
  SniWatcher*watcher;
  SniHost*host;
  gchar *xml;

  watcher = g_malloc0(sizeof(SniWatcher));
  host = g_malloc0(sizeof(SniHost));

  xml = g_strdup_printf(sni_watcher_xml, name);
  watcher->idata = g_dbus_node_info_new_for_xml (xml, NULL);
  g_free(xml);

  if(!watcher->idata)
    g_error("SNI: introspection error");

  watcher->iface = g_strdup_printf("org.%s.StatusNotifierWatcher", name);
  host->item_iface = g_strdup_printf("org.%s.StatusNotifierItem", name);
  host->iface = g_strdup_printf("org.%s.StatusNotifierHost-%d", name, getpid());
  host->watcher = watcher->iface;
  watcher->host = host;

  g_bus_watch_name(G_BUS_TYPE_SESSION, watcher->iface,
      G_BUS_NAME_WATCHER_FLAGS_NONE,
      (GBusNameAppearedCallback)sni_host_register_cb,
      (GBusNameVanishedCallback)sni_watcher_register_cb,
      watcher, NULL);
  g_bus_own_name(G_BUS_TYPE_SESSION, host->iface,
      G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL, NULL, NULL, NULL);
}

void sni_init ( void )
{
  if(!sni_get_connection())
    return;
  sni_register("kde");
  sni_register("freedesktop");
}
