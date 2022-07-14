/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include "sfwbar.h"
#include "trayitem.h"
#include "tray.h"

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

void sni_watcher_item_lost_cb ( GDBusConnection *con, const gchar *name,
    gpointer data )
{
  SniWatcher *watcher = data;
  GHashTableIter iter;
  gchar *uid;

  g_hash_table_iter_init(&iter,watcher->items);
  while(g_hash_table_iter_next(&iter,(void **)&uid,NULL))
    if(!strncmp(uid,name,strlen(name)))
    {
      g_debug("sni watcher %s: lost item: %s",watcher->iface,name);
      g_dbus_connection_emit_signal(con,NULL,"/StatusNotifierWatcher",
          watcher->iface,"StatusNotifierItemUnregistered",
          g_variant_new("(s)",uid),NULL);
      g_bus_unwatch_name(
          GPOINTER_TO_UINT(g_hash_table_lookup(watcher->items,uid)));
      g_hash_table_remove(watcher->items,uid);
      break;
    }
}

gint sni_watcher_item_add ( SniWatcher *watcher, gchar *uid )
{
  gchar *name;

  if(g_hash_table_contains(watcher->items,uid))
    return -1;

  if(strchr(uid,'/')!=NULL)
    name = g_strndup(uid,strchr(uid,'/')-uid);
  else
    name = g_strdup(uid);

  g_debug("sni watcher %s: watching item %s", watcher->iface, name);
  g_hash_table_insert(watcher->items,g_strdup(uid),GUINT_TO_POINTER(
        g_bus_watch_name(G_BUS_TYPE_SESSION,name,
          G_BUS_NAME_WATCHER_FLAGS_NONE,NULL,
          sni_watcher_item_lost_cb,watcher,NULL)));
  g_free(name);
  return 0;
}

static void sni_watcher_method(GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *method,
    GVariant *parameters, GDBusMethodInvocation *invocation, void *data)
{
  const gchar *parameter;
  SniWatcher *watcher = data;
  gchar *name;

  g_variant_get (parameters, "(&s)", &parameter);
  g_dbus_method_invocation_return_value(invocation,NULL);

  if(g_strcmp0(method,"RegisterStatusNotifierItem")==0)
  {
    if(*parameter!='/')
      name = g_strdup(parameter);
    else
      name = g_strconcat(sender,parameter,NULL);
    if(sni_watcher_item_add(watcher,name)!=-1)
      g_dbus_connection_emit_signal(con,NULL,"/StatusNotifierWatcher",
          watcher->iface,"StatusNotifierItemRegistered",
          g_variant_new("(s)",name),NULL);
    g_free(name);
  }

  if(g_strcmp0(method,"RegisterStatusNotifierHost")==0)
  {
    watcher->watcher_registered = TRUE;
    g_debug("sni watcher %s: registered host %s",watcher->iface,parameter);
    g_dbus_connection_emit_signal(con,NULL,"/StatusNotifierWatcher",
        watcher->iface,"StatusNotifierHostRegistered",
        g_variant_new("(s)",parameter),NULL);
  }
}

static GVariant *sni_watcher_get_prop (GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *prop,GError **error, void *data)
{
  GVariant *ret = NULL;
  SniWatcher *watcher = data;
  GVariantBuilder *builder;
  GHashTableIter iter;
  gchar *name;

  if (g_strcmp0 (prop, "IsStatusNotifierHostRegistered") == 0)
    ret = g_variant_new_boolean(watcher->watcher_registered);

  if (g_strcmp0 (prop, "ProtocolVersion") == 0)
    ret = g_variant_new_int32(0);

  if (g_strcmp0 (prop, "RegisteredStatusNotifierItems") == 0)
  {
    if(!g_hash_table_size(watcher->items))
      ret = g_variant_new_array(G_VARIANT_TYPE_STRING,NULL,0);
    else
    {
      builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY); 

      g_hash_table_iter_init(&iter,watcher->items);
      while(g_hash_table_iter_next(&iter,(gpointer *)&name,NULL))
        g_variant_builder_add_value(builder,g_variant_new_string(name));
      ret = g_variant_builder_end (builder);

      g_variant_builder_unref (builder);
    }
  }

  return ret;
}

static const GDBusInterfaceVTable watcher_vtable =
{
  sni_watcher_method,
  sni_watcher_get_prop,
  NULL
};

void sni_watcher_register_cb ( GDBusConnection *con, const gchar *name,
    SniWatcher *watcher)
{
  GList *iter;

  if(watcher->regid != 0)
    g_dbus_connection_unregister_object(con, watcher->regid);

  watcher->regid = g_dbus_connection_register_object (con,
      "/StatusNotifierWatcher", watcher->idata->interfaces[0],
      &watcher_vtable, watcher, NULL, NULL);

  for(iter=watcher->host->items;iter!=NULL;iter=g_list_next(iter))
    sni_watcher_item_add(watcher,((sni_item_t *)iter->data)->uid);
}

void sni_watcher_unregister_cb ( GDBusConnection *con, const gchar *name,
    SniWatcher *watcher)
{
  if(watcher->regid != 0)
    g_dbus_connection_unregister_object(con, watcher->regid);
  watcher->regid = 0;
  g_hash_table_remove_all(watcher->items);
}

static void sni_host_item_new ( GDBusConnection *con, SniHost *host,
    const gchar *uid)
{
  sni_item_t *sni;
  GList *iter;

  for(iter=host->items;iter!=NULL;iter=g_list_next(iter))
    if(!g_strcmp0(((sni_item_t *)iter->data)->uid,uid))
      break;
  if(iter)
    return;

  sni = sni_item_new(con,host,uid);
  if(sni)
    host->items = g_list_prepend(host->items,sni);

  g_debug("sni host %s: item registered: %s %s",host->iface,sni->dest,
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
  g_variant_get(result, "(v)",&inner);
  iter = g_variant_iter_new(inner);
  while(g_variant_iter_next(iter,"&s",&str))
    sni_host_item_new(con, host, str);
  g_variant_iter_free(iter);
  if(inner)
    g_variant_unref(inner);
  g_variant_unref(result);
}

static void sni_host_register_cb ( GDBusConnection *con, const gchar *name,
    const gchar *owner_name, gpointer data )
{
  SniHost *host = data;
  g_dbus_connection_call(con, host->watcher, "/StatusNotifierWatcher",
    host->watcher, "RegisterStatusNotifierHost",
    g_variant_new("(s)", host->iface),
    G_VARIANT_TYPE ("(a{sv})"),
    G_DBUS_CALL_FLAGS_NONE,-1,NULL,NULL,host);

  g_dbus_connection_call(con, host->watcher, "/StatusNotifierWatcher",
    "org.freedesktop.DBus.Properties", "Get",
    g_variant_new("(ss)", host->watcher,"RegisteredStatusNotifierItems"),
    NULL,
    G_DBUS_CALL_FLAGS_NONE,-1,NULL,(GAsyncReadyCallback)sni_host_list_cb,host);

  g_debug("sni host %s: found watcher %s (%s)",host->iface,name,owner_name);
}

static void sni_host_item_unregistered_cb ( GDBusConnection* con,
    const gchar* sender, const gchar* path, const gchar* interface,
    const gchar* signal, GVariant* parameters, gpointer data)
{
  sni_item_t *sni;
  const gchar *parameter;
  GList *item;
  gint i;
  SniHost *host = data;

  g_variant_get (parameters, "(&s)", &parameter);
  g_debug("sni host %s: unregister item %s",host->iface, parameter);

  for(item=host->items;item!=NULL;item=g_list_next(item))
    if(g_strcmp0(((sni_item_t *)item->data)->uid,parameter)==0)
      break;
  if(item==NULL)
    return;

  sni = item->data;
  g_dbus_connection_signal_unsubscribe(con,sni->signal);
  g_cancellable_cancel(sni->cancel);
  host->items = g_list_delete_link(host->items,item);
  g_debug("sni host %s: removing item %s",host->iface,sni->uid);
  for(i=0;i<3;i++)
    if(sni->pixbuf[i]!=NULL)
      g_object_unref(sni->pixbuf[i]);
  for(i=0;i<MAX_STRING;i++)
    g_free(sni->string[i]);
  tray_item_destroy(sni);

  g_free(sni->menu_path);
  g_free(sni->uid);
  g_free(sni->path);
  g_free(sni->dest);
  g_free(sni);
  tray_invalidate_all();
}

void sni_register ( gchar *name )
{
  SniWatcher*watcher;
  SniHost*host;
  gchar *xml;
  GDBusConnection *con;

  con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
  if(!con)
    return;

  watcher = g_malloc0(sizeof(SniWatcher));
  host = g_malloc0(sizeof(SniHost));

  xml = g_strdup_printf(sni_watcher_xml,name);
  watcher->idata = g_dbus_node_info_new_for_xml (xml, NULL);
  g_free(xml);

  if(!watcher->idata)
    g_error("SNI: introspection error");

  watcher->iface = g_strdup_printf("org.%s.StatusNotifierWatcher",name);
  host->item_iface = g_strdup_printf("org.%s.StatusNotifierItem",name);
  host->iface = g_strdup_printf("org.%s.StatusNotifierHost-%d",name,getpid());
  host->watcher = watcher->iface;
  watcher->items = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
  watcher->host = host;

  g_bus_own_name(G_BUS_TYPE_SESSION,watcher->iface,
      G_BUS_NAME_OWNER_FLAGS_NONE,NULL,
      (GBusNameAcquiredCallback)sni_watcher_register_cb,
      (GBusNameLostCallback)sni_watcher_unregister_cb,watcher,NULL);

  g_bus_own_name(G_BUS_TYPE_SESSION,host->iface,
      G_BUS_NAME_OWNER_FLAGS_NONE,NULL,NULL,NULL,NULL,NULL);
  g_bus_watch_name(G_BUS_TYPE_SESSION,watcher->iface,
      G_BUS_NAME_WATCHER_FLAGS_NONE,sni_host_register_cb,NULL,host,NULL);
  g_dbus_connection_signal_subscribe(con,NULL,watcher->iface,
      "StatusNotifierItemRegistered","/StatusNotifierWatcher",NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,sni_host_item_registered_cb,host,NULL);
  g_dbus_connection_signal_subscribe(con,NULL,watcher->iface,
      "StatusNotifierItemUnregistered","/StatusNotifierWatcher",NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,sni_host_item_unregistered_cb,host,NULL);
  g_object_unref(con);
}

void sni_init ( void )
{
  sni_register("kde");
  sni_register("freedesktop");
}
