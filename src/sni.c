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


struct sni_iface_item {
  guint id;
  gchar *name;
};

static GList *sni_items;
static GList *sni_ifaces;

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

void sni_host_item_new ( GDBusConnection *con, struct sni_iface *iface,
    const gchar *uid)
{
  sni_item_t *sni;
  GList *iter;

  for(iter=sni_items;iter!=NULL;iter=g_list_next(iter))
    if(g_strcmp0(((sni_item_t *)iter->data)->uid,uid)==0)
      break;
  if(iter!=NULL)
    return;

  sni = sni_item_new(con,iface,uid);
  if(sni)
    sni_items = g_list_append(sni_items,sni);

  g_debug("sni: host %s: item registered: %s %s",iface->host_iface,sni->dest,
      sni->path);
}

void sni_host_item_registered_cb ( GDBusConnection* con, const gchar* sender,
  const gchar* path, const gchar* iface, const gchar* signal,
  GVariant* parameters, gpointer data)
{
  const gchar *parameter;
  GList *host;

  for(host=sni_ifaces;host!=NULL;host=g_list_next(host))
    if(g_strcmp0(((struct sni_iface *)host->data)->watcher_iface,iface)==0)
      break;
  if(host==NULL)
    return;

  g_variant_get (parameters, "(&s)", &parameter);
  sni_host_item_new(con,host->data,parameter);
}

void sni_watcher_item_lost_cb ( GDBusConnection *con, const gchar *name,
    gpointer data )
{
  struct sni_iface *watcher = data;
  struct sni_iface_item *wsni;
  GList *item;

  for(item=watcher->item_list;item!=NULL;item=g_list_next(item))
    if(strncmp(name,((struct sni_iface_item *)item->data)->name,strlen(name))==0)
      break;
  if(item==NULL)
    return;

  wsni = item->data;
  g_debug("sni watcher %s: lost item: %s",watcher->watcher_iface,name);
  g_dbus_connection_emit_signal(con,NULL,"/StatusNotifierWatcher",
      watcher->watcher_iface,"StatusNotifierItemUnregistered",
      g_variant_new("(s)",wsni->name),NULL);
  g_bus_unwatch_name(wsni->id);
  g_free(wsni->name);
  g_free(wsni);
  watcher->item_list = g_list_delete_link(watcher->item_list,item);
}

gint sni_watcher_item_add ( struct sni_iface *watcher, gchar *uid )
{
  struct sni_iface_item *wsni;
  GList *iter;
  gchar *name;
  
  for(iter=watcher->item_list;iter!=NULL;iter=g_list_next(iter))
    if(g_strcmp0(((struct sni_iface_item *)iter->data)->name,uid)==0)
      return -1;

  wsni = g_malloc(sizeof(struct sni_iface_item));
  wsni->name = g_strdup(uid);

  if(strchr(wsni->name,'/')!=NULL)
    name = g_strndup(wsni->name,strchr(wsni->name,'/')-wsni->name);
  else
    name = g_strdup(wsni->name);

  g_debug("watcher %s: watching item %s", watcher->watcher_iface, name);
  wsni->id = g_bus_watch_name(G_BUS_TYPE_SESSION,name,
      G_BUS_NAME_WATCHER_FLAGS_NONE,NULL,
      sni_watcher_item_lost_cb,watcher,NULL);
  watcher->item_list = g_list_append(watcher->item_list,wsni);
  g_free(name);
  return 0;
}

static void sni_watcher_method(GDBusConnection *con, const gchar *sender,
    const gchar *path, const gchar *iface, const gchar *method,
    GVariant *parameters, GDBusMethodInvocation *invocation, void *data)
{
  const gchar *parameter;
  struct sni_iface *watcher = data;
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
          watcher->watcher_iface,"StatusNotifierItemRegistered",
          g_variant_new("(s)",name),NULL);
    g_free(name);
  }

  if(g_strcmp0(method,"RegisterStatusNotifierHost")==0)
  {
    watcher->watcher_registered = TRUE;
    g_debug("sni watcher %s: registered host %s",watcher->watcher_iface,parameter);
    g_dbus_connection_emit_signal(con,NULL,"/StatusNotifierWatcher",
        watcher->watcher_iface,"StatusNotifierHostRegistered",
        g_variant_new("(s)",parameter),NULL);
  }
}

static GVariant *sni_watcher_get_prop (GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *iface,
    const gchar *prop,GError **error, void *data)
{
  GVariant *ret = NULL;
  struct sni_iface *watcher = data;
  GVariantBuilder *builder;
  GList *iter;

  if (g_strcmp0 (prop, "IsStatusNotifierHostRegistered") == 0)
    ret = g_variant_new_boolean(watcher->watcher_registered);

  if (g_strcmp0 (prop, "ProtocolVersion") == 0)
    ret = g_variant_new_int32(0);

  if (g_strcmp0 (prop, "RegisteredStatusNotifierItems") == 0)
  {
    if(watcher->item_list==NULL)
      ret = g_variant_new_array(G_VARIANT_TYPE_STRING,NULL,0);
    else
    {
      builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY); 

      for(iter=watcher->item_list;iter!=NULL;iter=g_list_next(iter))
        g_variant_builder_add_value(builder,
            g_variant_new_string(((struct sni_iface_item *)iter->data)->name));
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
    struct sni_iface *watcher)
{
  GList *iter;

  if(watcher->regid != 0)
    g_dbus_connection_unregister_object(con, watcher->regid);

  watcher->regid = g_dbus_connection_register_object (con,
      "/StatusNotifierWatcher", watcher->idata->interfaces[0],
      &watcher_vtable, watcher, NULL, NULL);

  for(iter=sni_items;iter!=NULL;iter=g_list_next(iter))
    if(!g_strcmp0(((sni_item_t *)iter->data)->iface,watcher->item_iface))
      sni_watcher_item_add(watcher,((sni_item_t *)iter->data)->uid);
}

void sni_watcher_unregister_cb ( GDBusConnection *con, const gchar *name,
    struct sni_iface *watcher)
{
  GList *iter;
  struct sni_iface_item *wsni;

  if(watcher->regid != 0)
    g_dbus_connection_unregister_object(con, watcher->regid);
  watcher->regid = 0;
  for(iter=watcher->item_list;iter!=NULL;iter=g_list_next(iter))
  {
    wsni=iter->data;
    g_free(wsni->name);
    g_free(iter->data);
  }
  g_list_free(watcher->item_list);
  watcher->item_list = NULL;
}

void sni_host_list_cb ( GDBusConnection *con, GAsyncResult *res,
    struct sni_iface *iface)
{
  GVariant *result,*inner,*str;
  GVariantIter *iter;

  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result==NULL)
    return;
  g_variant_get(result, "(v)",&inner);
  iter = g_variant_iter_new(inner);
  while((str=g_variant_iter_next_value(iter)))
  {
    sni_host_item_new(con, iface, g_variant_get_string(str,NULL));
    g_variant_unref(str);
  }
  g_variant_iter_free(iter);
  if(inner)
    g_variant_unref(inner);
  if(result)
    g_variant_unref(result);
}

void sni_host_register_cb ( GDBusConnection *con, const gchar *name,
    const gchar *owner_name, gpointer data )
{
  struct sni_iface *host = data;
  g_dbus_connection_call(con, host->watcher_iface, "/StatusNotifierWatcher",
    host->watcher_iface, "RegisterStatusNotifierHost",
    g_variant_new("(s)", host->host_iface),
    G_VARIANT_TYPE ("(a{sv})"),
    G_DBUS_CALL_FLAGS_NONE,-1,NULL,NULL,host);

  g_dbus_connection_call(con, host->watcher_iface, "/StatusNotifierWatcher",
    "org.freedesktop.DBus.Properties", "Get",
    g_variant_new("(ss)", host->watcher_iface,"RegisteredStatusNotifierItems"),
    NULL,
    G_DBUS_CALL_FLAGS_NONE,-1,NULL,(GAsyncReadyCallback)sni_host_list_cb,host);

  g_debug("sni host %s: found watcher %s (%s)",host->host_iface,name,owner_name);
}

void sni_host_item_unregistered_cb ( GDBusConnection* con, const gchar* sender,
  const gchar* path, const gchar* interface, const gchar* signal,
  GVariant* parameters, gpointer data)
{
  sni_item_t *sni;
  const gchar *parameter;
  GList *item;
  gint i;
  struct sni_iface *host = data;

  g_variant_get (parameters, "(&s)", &parameter);
  g_debug("sni host %s: unregister item %s",host->host_iface, parameter);

  for(item=sni_items;item!=NULL;item=g_list_next(item))
    if(g_strcmp0(((sni_item_t *)item->data)->uid,parameter)==0)
      break;
  if(item==NULL)
    return;

  sni = item->data;
  g_dbus_connection_signal_unsubscribe(con,sni->signal);
  g_cancellable_cancel(sni->cancel);
  sni_items = g_list_delete_link(sni_items,item);
  g_debug("sni host %s: removing item %s",host->host_iface,sni->uid);
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
  g_free(sni->iface);
  g_free(sni);
  tray_invalidate_all();
}

void sni_register ( gchar *name )
{
  struct sni_iface *iface;
  gchar *xml;
  GDBusConnection *con;

  con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
  if(!con)
    return;

  iface = g_malloc0(sizeof(struct sni_iface));

  xml = g_strdup_printf(sni_watcher_xml,name);
  iface->idata = g_dbus_node_info_new_for_xml (xml, NULL);
  g_free(xml);

  if(iface->idata==NULL)
    g_error("SNI: introspection error");

  iface->watcher_iface = g_strdup_printf("org.%s.StatusNotifierWatcher",name);
  iface->item_iface = g_strdup_printf("org.%s.StatusNotifierItem",name);
  iface->host_iface = g_strdup_printf("org.%s.StatusNotifierHost-%d",name,
      getpid());
  sni_ifaces = g_list_append(sni_ifaces,iface);

  g_bus_own_name(G_BUS_TYPE_SESSION,iface->watcher_iface,
      G_BUS_NAME_OWNER_FLAGS_NONE,NULL,
      (GBusNameAcquiredCallback)sni_watcher_register_cb,
      (GBusNameLostCallback)sni_watcher_unregister_cb,iface,NULL);
  g_bus_own_name(G_BUS_TYPE_SESSION,iface->host_iface,
      G_BUS_NAME_OWNER_FLAGS_NONE,NULL,NULL,NULL,NULL,NULL);
  g_bus_watch_name(G_BUS_TYPE_SESSION,iface->watcher_iface,
      G_BUS_NAME_WATCHER_FLAGS_NONE,sni_host_register_cb,NULL,iface,NULL);

  g_dbus_connection_signal_subscribe(con,NULL,iface->watcher_iface,
      "StatusNotifierItemRegistered","/StatusNotifierWatcher",NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,sni_host_item_registered_cb,NULL,NULL);
  g_dbus_connection_signal_subscribe(con,NULL,iface->watcher_iface,
      "StatusNotifierItemUnregistered","/StatusNotifierWatcher",NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,sni_host_item_unregistered_cb,iface,NULL);
  g_object_unref(con);
}

void sni_init ( void )
{
  sni_register("kde");
  sni_register("freedesktop");
}
