#include <glib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "sfwbar.h"

enum {
  SNI_PROP_CATEGORY = 0,
  SNI_PROP_ID = 1,
  SNI_PROP_TITLE = 2,
  SNI_PROP_STATUS = 3,
  SNI_PROP_ICON = 4,
  SNI_PROP_OVLAY = 5,
  SNI_PROP_ATTN = 6,
  SNI_PROP_ATTNMOV = 7,
  SNI_PROP_ICONPIX = 8,
  SNI_PROP_OVLAYPIX = 9,
  SNI_PROP_ATTNPIX = 10,
  SNI_PROP_WINDOWID = 11,
  SNI_PROP_TOOLTIP = 12,
  SNI_PROP_ISMENU = 13,
  SNI_PROP_MENU = 14
};

gchar *sni_properties[] = { "Category", "Id", "Title", "Status", "IconName",
  "OverlayIconName", "AttentionIconName", "AttentionMovieName", "IconPixmap",
  "OverlayIconPixmap", "AttentionIconPixmap", "ToolTip", "WindowId",
  "ItemIsMenu", "Menu" };

struct sni_item {
  gchar *uid;
  gchar *udest;
  gchar *dest;
  gchar *path;
  gchar *iface;
  gchar *string[8];
  GdkPixbuf *pixbuf[3];
  gboolean menu;
  gboolean dirty;
  gint ref;
  guint signal;
  GCancellable *cancel;
  GtkWidget *image;
  GtkWidget *box;
};

struct sni_prop_wrapper {
  guint prop;
  struct sni_item *sni;
};

struct sni_iface_item {
  guint id;
  gchar *name;
};

struct sni_iface {
  guint regid;
  struct context *context;
  gboolean watcher_registered;
  gchar *watcher_iface;
  gchar *item_iface;
  gchar *host_iface;
  GList *item_list;
  GDBusNodeInfo *idata;
};

static const gchar sni_watcher_xml[] =
  "<node>"
  "  <interface name='org.kde.StatusNotifierWatcher'>"
  "    <method name='RegisterStatusNotifierItem'>"
  "      <arg type='s' name='service' direction='in'/>"
  "    </method>"
  "    <method name='RegisterStatusNotifierHost'>"
  "      <arg type='s' name='service' direction='in'/>"
  "    </method>"
  "    <signal name='StatusNotifierItemRegistered'>"
  "      <arg type='s' name='service'/>"
  "    </signal>"
  "    <signal name='StatusNotifierItemUnregistered'>"
  "      <arg type='s' name='service'/>"
  "    </signal>"
  "    <signal name='StatusNotifierHostRegistered'>"
  "    </signal>"
  "    <property type='as' name='RegisteredStatusNotifierItems' access='read'/>"
  "    <property type='b' name='IsStatusNotifierHostRegistered' access='read'/>"
  "    <property type='i' name='ProtocolVersion' access='read'/>"
  "  </interface>"
  "</node>";

GdkPixbuf *sni_item_get_pixbuf ( GVariant *v )
{
  gint32 x,y;
  GVariantIter *iter,*rgba;
  guchar *buff;
  guint32 *ptr;
  guchar t;
  cairo_surface_t *cs;
  GdkPixbuf *res;
  gint i=0;

  g_variant_get(v,"a(iiay)",&iter);
  g_variant_get(g_variant_iter_next_value(iter),"(iiay)",&x,&y,&rgba);
  printf("pix %d %d %ld\n",x,y,g_variant_iter_n_children(rgba));
  if((x*y>0)&&(x*y*4 == g_variant_iter_n_children(rgba)))
  {
    buff = g_malloc(x*y*4);
    while(g_variant_iter_loop(rgba,"y",&t))
      buff[i++]=t;

    ptr=(guint32 *)buff;
    for(i=0;i<x*y;i++)
      ptr[i] = ntohl(ptr[i]);
      
    cs = cairo_image_surface_create_for_data(buff,CAIRO_FORMAT_ARGB32,x,y,
        cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,x));
    res = gdk_pixbuf_get_from_surface(cs,0,0,x,y);

    cairo_surface_destroy(cs);
    g_free(buff);
  }
  else
    res = NULL;
  if(iter)
    g_variant_iter_free(iter);
  if(rgba)
    g_variant_iter_free(rgba);
  return res;
}

void sni_item_set_icon ( struct sni_item *sni, gint icon, gint pix )
{
  if(icon==-1)
  {
    printf("restting image\n");
    scale_image_set_image(sni->image,NULL);
    return;
  }
  if(sni->string[icon]!=NULL)
  {
    scale_image_set_image(sni->image,sni->string[icon]);
    printf("setting icon %s\n",sni->string[icon]);
    return;
  }
  if(sni->pixbuf[pix-SNI_PROP_ICONPIX]!=NULL)
  {
    printf("setting buff %p\n",sni->pixbuf[pix-SNI_PROP_ICONPIX]);
    scale_image_set_pixbuf(sni->image,sni->pixbuf[pix-SNI_PROP_ICONPIX]);
    return;
  }
  return;
}

void sni_item_prop_cb ( GDBusConnection *con, GAsyncResult *res, struct sni_prop_wrapper *wrap)
{
  GVariant *result, *inner;
  gchar *param;

  wrap->sni->ref--;
  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result==NULL)
    return;
  g_variant_get(result, "(v)",&inner);
  if(wrap->prop<=SNI_PROP_ATTNMOV)
  {
    g_free(wrap->sni->string[wrap->prop]);
    if(inner && g_variant_is_of_type(inner,G_VARIANT_TYPE_STRING))
      g_variant_get(inner,"s",&param);
    else
      param=NULL;
    wrap->sni->string[wrap->prop] = g_strdup(param);
    printf("%s %s\n",sni_properties[wrap->prop],wrap->sni->string[wrap->prop]);
  }
  if((wrap->prop>=SNI_PROP_ICONPIX)&&(wrap->prop<=SNI_PROP_ATTNPIX))
  {
    if(wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX]!=NULL)
      g_object_unref(wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX]);
    wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX] = sni_item_get_pixbuf(inner);
  }
  if(wrap->sni->string[SNI_PROP_STATUS]!=NULL)
  {
    printf("status %s\n",wrap->sni->string[SNI_PROP_STATUS]);
    if(wrap->sni->string[SNI_PROP_STATUS][0]=='A')
    {
      gtk_widget_set_name(wrap->sni->image,"tray_active");
      sni_item_set_icon(wrap->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
    }
    if(wrap->sni->string[SNI_PROP_STATUS][0]=='N')
    {
      gtk_widget_set_name(wrap->sni->image,"tray_attention");
      sni_item_set_icon(wrap->sni,SNI_PROP_ATTN,SNI_PROP_ATTNPIX);
    }
    if(wrap->sni->string[SNI_PROP_STATUS][0]=='P')
    {
      gtk_widget_set_name(wrap->sni->image,"tray_passive");
      sni_item_set_icon(wrap->sni,-1,-1);
    }
  }

  if(inner)
    g_variant_unref(inner);
  if(result)
    g_variant_unref(result);
  g_free(wrap);
}

void sni_item_get_prop ( GDBusConnection *con, struct sni_item *sni, guint prop )
{
  struct sni_prop_wrapper *wrap;
  wrap = g_malloc(sizeof(struct sni_prop_wrapper));
  wrap->prop = prop;
  wrap->sni = sni;
  wrap->sni->ref++;

  g_dbus_connection_call(con, sni->dest, sni->path,
    "org.freedesktop.DBus.Properties", "Get",
    g_variant_new("(ss)", sni->iface, sni_properties[prop]),NULL,
    G_DBUS_CALL_FLAGS_NONE,-1,sni->cancel,(GAsyncReadyCallback)sni_item_prop_cb,wrap);
}

void sni_item_signal_cb (GDBusConnection *con, const gchar *sender,
         const gchar *path, const gchar *interface, const gchar *signal,
         GVariant *parameters, gpointer data)
{
  printf("signal %s from %s\n",signal,sender);
  if(g_strcmp0(signal,"NewTitle")==0)
    sni_item_get_prop(con,data,SNI_PROP_TITLE);
  if(g_strcmp0(signal,"NewStatus")==0)
    sni_item_get_prop(con,data,SNI_PROP_STATUS);
  if(g_strcmp0(signal,"NewToolTip")==0)
    sni_item_get_prop(con,data,SNI_PROP_TOOLTIP);
  if(g_strcmp0(signal,"NewIcon")==0)
  {
    sni_item_get_prop(con,data,SNI_PROP_ICON);
    sni_item_get_prop(con,data,SNI_PROP_ICONPIX);
  }
  if(g_strcmp0(signal,"NewOverlayIcon")==0)
  {
    sni_item_get_prop(con,data,SNI_PROP_OVLAY);
    sni_item_get_prop(con,data,SNI_PROP_OVLAYPIX);
  }
  if(g_strcmp0(signal,"NewAttentionIcon")==0)
  {
    sni_item_get_prop(con,data,SNI_PROP_ATTN);
    sni_item_get_prop(con,data,SNI_PROP_ATTNPIX);
  }
}

gboolean sni_item_click_cb (GtkWidget *w, GdkEventButton *event, gpointer data)
{
  printf("clicked\n");
  if(event->type == GDK_BUTTON_PRESS)
    printf("clicked %d\n",event->button);
  return TRUE;
}

void sni_item_new (GDBusConnection *con, struct sni_iface *iface,
    const gchar *uid)
{
  struct sni_item *sni;
  gchar *path;
  guint i;
  GList *iter;
  for(iter=iface->context->sni_items;iter!=NULL;iter=g_list_next(iter))
    if(g_strcmp0(((struct sni_item *)iter->data)->uid,uid)==0)
      break;
  if(iter!=NULL)
    return;

  sni = g_malloc(sizeof(struct sni_item));
  sni->uid = g_strdup(uid);
  sni->cancel = g_cancellable_new();
  path = strchr(uid,'/');
  if(path!=NULL)
  {
    sni->dest = g_strndup(uid,path-uid);
    sni->path = g_strdup(path);
  }
  else
  {
    sni->path = g_strdup("/StatusNotifierItem");
    sni->dest = g_strdup(uid);
  }
  sni->iface = g_strdup(iface->item_iface);
  sni->image = scale_image_new();
  sni->box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(sni->box),sni->image);
  g_signal_connect(G_OBJECT(sni->box),"button-press-event",
      G_CALLBACK(sni_item_click_cb),NULL);
  g_object_ref(sni->box);
  sni->signal = g_dbus_connection_signal_subscribe(con,sni->dest,
      sni->iface,NULL,sni->path,NULL,0,sni_item_signal_cb,sni,NULL);
  printf("item registered: %s %s\n",sni->dest,sni->path);
  iface->context->sni_items = g_list_append(iface->context->sni_items,sni);
  for(i=0;i<3;i++)
    sni->pixbuf[i]=NULL;
  for(i=0;i<7;i++)
    sni->string[i]=NULL;
  for(i=0;i<15;i++)
    sni_item_get_prop(con,sni,i);
}

void sni_host_item_registered_cb ( GDBusConnection* con, const gchar* sender,
  const gchar* path, const gchar* iface, const gchar* signal,
  GVariant* parameters, gpointer data)
{
  const gchar *parameter;
  struct context *context = data;
  GList *host;
  for(host=context->sni_ifaces;host!=NULL;host=g_list_next(host))
    if(g_strcmp0(((struct sni_iface *)host->data)->watcher_iface,iface)==0)
      break;
  if(host==NULL)
    return;

  g_variant_get (parameters, "(&s)", &parameter);
  sni_item_new(con,host->data,parameter);
}
void watcher_item_new_cb ( GDBusConnection *con, const gchar *name, const gchar *owner_name, gpointer data )
{
  printf("appeared %s owner %s\n",name,owner_name);
}

void sni_watcher_item_lost_cb ( GDBusConnection *con, const gchar *name, gpointer data )
{
  struct sni_iface *host = data;
  struct sni_iface_item *wsni;
  GList *item;
  for(item=host->item_list;item!=NULL;item=g_list_next(item))
    if(strncmp(name,((struct sni_iface_item *)item->data)->name,strlen(name))==0)
      break;
  if(item==NULL)
    return;
  wsni = item->data;
  printf("%s disappeared\n",name);
  g_dbus_connection_emit_signal(con,NULL,"/StatusNotifierWatcher",
      host->watcher_iface,"StatusNotifierItemUnregistered",
      g_variant_new("(s)",wsni->name),NULL);
  g_bus_unwatch_name(wsni->id);
  g_free(wsni->name);
  g_free(wsni);
  host->item_list = g_list_delete_link(host->item_list,item);
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

   printf("watching %s\n", name);
   wsni->id = g_bus_watch_name(G_BUS_TYPE_SESSION,name,
       G_BUS_NAME_WATCHER_FLAGS_NONE,watcher_item_new_cb,
       sni_watcher_item_lost_cb,watcher,NULL);
   watcher->item_list = g_list_append(watcher->item_list,wsni);
   g_free(name);
   printf("%s %d %p\n",wsni->name,wsni->id,watcher->item_list);
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
    if(*parameter==':')
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
    printf("registered host %s\n",parameter);
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
        g_variant_builder_add_value(builder,g_variant_new_string(((struct sni_iface_item *)iter->data)->name));
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
  for(iter=watcher->context->sni_items;iter!=NULL;iter=g_list_next(iter))
    if(g_strcmp0(((struct sni_item *)iter->data)->iface,watcher->item_iface)==0)
      sni_watcher_item_add(watcher,((struct sni_item *)iter->data)->uid);
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

void sni_host_list_cb ( GDBusConnection *con, GAsyncResult *res, struct sni_iface *iface)
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
    sni_item_new(con, iface, g_variant_get_string(str,NULL));
    g_variant_unref(str);
  }
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
    g_variant_new("(ss)", host->watcher_iface, "RegisteredStatusNotifierItems"),
    NULL,
    G_DBUS_CALL_FLAGS_NONE,-1,NULL,(GAsyncReadyCallback)sni_host_list_cb,host);

  printf("host: appeared %s owner %s\n",name,owner_name);
}

void sni_host_item_unregistered_cb ( GDBusConnection* con, const gchar* sender,
  const gchar* _path, const gchar* interface, const gchar* signal,
  GVariant* parameters, gpointer data)
{
  struct context *context = data;
  struct sni_item *sni;
  const gchar *parameter;
  GList *item;
  gint i;
  g_variant_get (parameters, "(&s)", &parameter);
  printf("unregister %s\n",parameter);
  for(item=context->sni_items;item!=NULL;item=g_list_next(item))
    if(g_strcmp0(((struct sni_item *)item->data)->uid,parameter)==0)
      break;
  if(item==NULL)
    return;
  sni = item->data;
  g_dbus_connection_signal_unsubscribe(con,sni->signal);
  g_cancellable_cancel(sni->cancel);
  context->sni_items = g_list_delete_link(context->sni_items,item);
  printf("removed %s\n",sni->uid);
  for(i=0;i<3;i++)
    if(sni->pixbuf[i]!=NULL)
      g_object_unref(sni->pixbuf[i]);
  for(i=0;i<7;i++)
    g_free(sni->string[i]);
  if(sni->image!=NULL)
    g_object_unref(sni->image);
  g_free(sni->uid);
  g_free(sni->path);
  g_free(sni->dest);
  g_free(sni->iface);
  g_free(sni);
}

void sni_register ( gchar *name, struct context *context )
{
  struct sni_iface *iface;
  GDBusConnection *con;
  iface = g_malloc(sizeof(struct sni_iface));
  iface->regid = 0;
  iface->watcher_registered = FALSE;
  iface->item_list = NULL;
  iface->idata = g_dbus_node_info_new_for_xml (sni_watcher_xml, NULL);

  if(iface->idata==NULL)
    printf("introspection error");
  iface->watcher_iface = g_strdup_printf("org.%s.StatusNotifierWatcher",name);
  iface->item_iface = g_strdup_printf("org.%s.StatusNotifierItem",name);
  iface->host_iface = g_strdup_printf("org.%s.StatusNotifierHost-%d",name,
      getpid());
  context->sni_ifaces = g_list_append(context->sni_ifaces,iface);
  iface->context = context;
  g_bus_own_name(G_BUS_TYPE_SESSION,iface->watcher_iface,
      G_BUS_NAME_OWNER_FLAGS_NONE,NULL,
      (GBusNameAcquiredCallback)sni_watcher_register_cb,
      (GBusNameLostCallback)sni_watcher_unregister_cb,iface,NULL);
  g_bus_own_name(G_BUS_TYPE_SESSION,iface->host_iface,
      G_BUS_NAME_OWNER_FLAGS_NONE,NULL,NULL,NULL,NULL,NULL);
  g_bus_watch_name(G_BUS_TYPE_SESSION,iface->watcher_iface,
      G_BUS_NAME_WATCHER_FLAGS_NONE,sni_host_register_cb,NULL,iface,NULL);

  con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
  g_dbus_connection_signal_subscribe(con,NULL,iface->watcher_iface,
      "StatusNotifierItemRegistered","/StatusNotifierWatcher",NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,sni_host_item_registered_cb,context,NULL);
  g_dbus_connection_signal_subscribe(con,NULL,iface->watcher_iface,
      "StatusNotifierItemUnregistered","/StatusNotifierWatcher",NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,sni_host_item_unregistered_cb,context,NULL);
}

void sni_item_remove ( GtkWidget *widget, struct context *context )
{
  gtk_container_remove ( GTK_CONTAINER(context->tray), widget );
}

void sni_refresh ( struct context *context )
{
  GList *iter;
  struct sni_item *sni;
  gtk_container_foreach(GTK_CONTAINER(context->tray),
      (GtkCallback)sni_item_remove,context);
  for(iter = context->sni_items;iter!=NULL;iter=g_list_next(iter))
  {
    sni = iter->data;
    if(sni!=NULL)
      if(sni->string[SNI_PROP_STATUS]!=NULL)
        if(sni->string[SNI_PROP_STATUS][0]!='P')
          gtk_container_add(GTK_CONTAINER(context->tray),sni->box);
  }
  gtk_widget_show_all(context->tray);
}

GtkWidget *sni_init (struct context *context)
{
  context->features |= F_TRAY;
  context->tray = gtk_grid_new();
  sni_register("kde",context);
  return context->tray;
}
