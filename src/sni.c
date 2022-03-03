/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include "sfwbar.h"

#define MAX_STRING 9

enum {
  SNI_PROP_CATEGORY = 0,
  SNI_PROP_ID = 1,
  SNI_PROP_TITLE = 2,
  SNI_PROP_STATUS = 3,
  SNI_PROP_ICON = 4,
  SNI_PROP_OVLAY = 5,
  SNI_PROP_ATTN = 6,
  SNI_PROP_ATTNMOV = 7,
  SNI_PROP_THEME = 8,
  SNI_PROP_ICONPIX = 9,
  SNI_PROP_OVLAYPIX = 10,
  SNI_PROP_ATTNPIX = 11,
  SNI_PROP_WINDOWID = 12,
  SNI_PROP_TOOLTIP = 13,
  SNI_PROP_ISMENU = 14,
  SNI_PROP_MENU = 15
};

static gchar *sni_properties[] = { "Category", "Id", "Title", "Status",
  "IconName", "OverlayIconName", "AttentionIconName", "AttentionMovieName",
  "IconThemePath", "IconPixmap", "OverlayIconPixmap", "AttentionIconPixmap",
  "ToolTip", "WindowId", "ItemIsMenu", "Menu" };

struct sni_item {
  gchar *uid;
  gchar *udest;
  gchar *dest;
  gchar *path;
  gchar *iface;
  gchar *string[MAX_STRING];
  gchar *menu_path;
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

struct sni_menu_wrapper {
  GdkEvent *event;
  struct sni_item *sni;
};

struct sni_iface_item {
  guint id;
  gchar *name;
};

struct sni_iface {
  guint regid;
  gboolean watcher_registered;
  gchar *watcher_iface;
  gchar *item_iface;
  gchar *host_iface;
  GList *item_list;
  GDBusNodeInfo *idata;
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

static GtkWidget *tray;
static GList *sni_items;
static GList *sni_ifaces;
static gboolean invalid;

gboolean sni_variant_get_bool ( GVariant *dict, gchar *key, gboolean def )
{
  GVariant *ptr;
  gboolean result;
  ptr = g_variant_lookup_value(dict,key,G_VARIANT_TYPE_BOOLEAN);
  if(ptr)
  {
    result = g_variant_get_boolean(ptr);
    g_variant_unref(ptr);
    return result;
  }
  return def;
}

gint32 sni_variant_get_int32 ( GVariant *dict, gchar *key, gint def )
{
  GVariant *ptr;
  gint result;
  ptr = g_variant_lookup_value(dict,key,G_VARIANT_TYPE_INT32);
  if(ptr)
  {
    result = g_variant_get_int32(ptr);
    g_variant_unref(ptr);
    return result;
  }
  return def;
}

const gchar *sni_variant_get_string ( GVariant *dict, gchar *key, gchar *def )
{
  GVariant *ptr;
  const gchar *result;
  ptr = g_variant_lookup_value(dict,key,G_VARIANT_TYPE_STRING);
  if(ptr)
  {
    result = g_variant_get_string(ptr,NULL);
    g_variant_unref(ptr);
    return result;
  }
  return def;
}

GtkWidget *sni_variant_get_pixbuf ( GVariant *dict, gchar *key )
{
  GVariant *ptr,*item;
  GVariantIter iter;
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  GtkWidget *img = NULL;
  guchar *buff;
  gint i=0;

  ptr = g_variant_lookup_value(dict,key,G_VARIANT_TYPE_ARRAY);
  if(!ptr)
    return NULL;

  g_variant_iter_init(&iter,ptr);
  buff = g_malloc(g_variant_iter_n_children(&iter));
  while( (item = g_variant_iter_next_value(&iter)) )
  {
    if(g_variant_is_of_type(item,G_VARIANT_TYPE_BYTE))
      buff[i++]=g_variant_get_byte(item);
    g_variant_unref(item);
  }

  if( i == g_variant_iter_n_children(&iter) )
  {
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, buff, i, NULL);
    gdk_pixbuf_loader_close(loader,NULL);
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if(pixbuf)
      img = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(G_OBJECT(loader));
  }
  g_free(buff);
  g_variant_unref(ptr);

  return img;
}

void sni_menu_item_cb ( GtkWidget *item, struct sni_item *sni )
{
  GDBusConnection *con;
  gint32 *id = g_object_get_data(G_OBJECT(item),"sni_id");

  if(!id)
    return;

  con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);

  g_debug("sni menu call: %d (%s) %s",*id,
      gtk_menu_item_get_label(GTK_MENU_ITEM(item)),sni->dest);

  g_dbus_connection_call(con, sni->dest, sni->menu_path,
      "com.canonical.dbusmenu", "Event",
      g_variant_new("(isvu)", *id, "clicked", g_variant_new_int32(0),
        gtk_get_current_event_time()),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  g_object_unref(con);
}

void sni_menu_item_decorate ( GtkWidget *item, GVariant *dict  )
{
  GtkWidget *box;
  GtkWidget *wlabel;
  GtkWidget *child;
  const gchar *label, *icon;
  GtkWidget *img;

  gtk_widget_set_name(item,"tray");

  if(GTK_IS_SEPARATOR_MENU_ITEM(item))
    return;

  child = gtk_bin_get_child(GTK_BIN(item));
  if(child)
    gtk_container_remove(GTK_CONTAINER(item),child);

  box = gtk_grid_new();

  icon = sni_variant_get_string(dict,"icon-name",NULL);
  if(icon)
    img = gtk_image_new_from_icon_name(icon,GTK_ICON_SIZE_MENU);
  else
    img = sni_variant_get_pixbuf(dict,"icon-data");
  if(img)
    gtk_grid_attach(GTK_GRID(box),img,1,1,1,1);

  label = sni_variant_get_string(dict,"label","");
  if(label)
  {
    wlabel = gtk_label_new_with_mnemonic(label);
    gtk_grid_attach(GTK_GRID(box),wlabel,2,1,1,1);
  }

  gtk_container_add(GTK_CONTAINER(item),box);
}

GtkWidget *sni_get_menu_iter ( GVariant *list, struct sni_menu_wrapper *wrap)
{
  GVariantIter iter;
  GVariant *item,*tmp;
  GtkWidget *mitem, *menu, *smenu;
  GVariant *dict,*idv;
  const gchar *type, *toggle, *children;
  gint32 active,*id;
  GSList *group = NULL;

  if(!list)
    return NULL;
  g_variant_iter_init(&iter,list);

  menu = gtk_menu_new();
  gtk_widget_set_name(menu,"tray");

  while( (item = g_variant_iter_next_value(&iter)) )
  {
    if(g_variant_is_of_type(item,G_VARIANT_TYPE_VARIANT))
    {
      tmp = item;
      item = g_variant_get_variant(tmp);
      g_variant_unref(tmp);
    }

    dict = g_variant_get_child_value(item, 1);
    mitem = NULL;

    if(sni_variant_get_bool(dict,"visible",TRUE))
    {
      type = sni_variant_get_string(dict,"type","standard");
      toggle = sni_variant_get_string(dict,"toggle-type","");
      children = sni_variant_get_string(dict,"children-display","");
      active = sni_variant_get_int32(dict,"toggle-state",0);

      if(!g_strcmp0(children,"submenu"))
      {
        smenu = sni_get_menu_iter(g_variant_get_child_value(item, 2),wrap);
        if(smenu)
        {
          mitem = gtk_menu_item_new();
          gtk_menu_item_set_submenu(GTK_MENU_ITEM(mitem),smenu);
        }
      }
      if(!mitem && !g_strcmp0(type,"separator"))
        mitem = gtk_separator_menu_item_new();
      if(!mitem &&  !*toggle )
        mitem= gtk_menu_item_new();
      if(!mitem && !g_strcmp0(toggle,"checkmark"))
      {
        mitem= gtk_check_menu_item_new();
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mitem),active);
      }
      if(!mitem && !g_strcmp0(toggle,"radio"))
      {
        mitem= gtk_radio_menu_item_new(group);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(mitem));
      }
    }

    if(mitem)
    {
      sni_menu_item_decorate(mitem,dict);
      gtk_widget_set_sensitive(mitem,sni_variant_get_bool(dict,"enabled",TRUE));
      idv = g_variant_get_child_value(item, 0);
      id = g_malloc(sizeof(gint32));
      *id = g_variant_get_int32(idv);
      g_variant_unref(idv);
      g_object_set_data_full(G_OBJECT(mitem),"sni_id",id,g_free);
      g_signal_connect(G_OBJECT(mitem),"activate",
          G_CALLBACK(sni_menu_item_cb),wrap->sni);
      gtk_container_add(GTK_CONTAINER(menu),mitem);
    }

    g_variant_unref(dict);
    g_variant_unref(item);
  }

  g_variant_unref(list);
  return menu;
}

void sni_get_menu_cb ( GObject *src, GAsyncResult *res, gpointer data )
{
  GVariant *result, *layout, *list=NULL;
  struct sni_menu_wrapper *wrap = data;
  GtkWidget *menu;

  result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src),res,NULL);
  if(result)
  {
    gchar *tmp = g_variant_print(result,TRUE);
    g_debug("sni %s: menu: %s",wrap->sni->dest,tmp);
    g_free(tmp);
    layout = g_variant_get_child_value(result, 1);
    if(layout)
    {
      list = g_variant_get_child_value(layout, 2);
      g_variant_unref(layout);
    }
  }

  menu = sni_get_menu_iter(list,wrap);

  if(menu)
  {
    g_object_ref_sink(G_OBJECT(menu));
    g_signal_connect(G_OBJECT(menu),"unmap",G_CALLBACK(g_object_unref),NULL);
    layout_menu_popup(wrap->sni->image,menu,wrap->event,NULL,NULL);
  }

  gdk_event_free(wrap->event);
  g_free(wrap);
  if(result)
    g_variant_unref(result);
}

void sni_get_menu ( struct sni_item *sni, GdkEvent *event )
{
  GDBusConnection *con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
  struct sni_menu_wrapper *wrap = g_malloc(sizeof(struct sni_menu_wrapper));

  wrap->event = gdk_event_copy(event);
  wrap->sni = sni;

  g_debug("sni %s: requesting menu",wrap->sni->dest);

  g_dbus_connection_call(con, sni->dest, sni->menu_path, "com.canonical.dbusmenu",
      "GetLayout", g_variant_new("(iias)", 0, -1, NULL),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, sni_get_menu_cb, wrap);
  g_object_unref(con);
}

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
  if((x*y>0)&&(x*y*4 == g_variant_iter_n_children(rgba)))
  {
    buff = g_malloc(x*y*4);
    while(g_variant_iter_loop(rgba,"y",&t))
      buff[i++]=t;

    ptr=(guint32 *)buff;
    for(i=0;i<x*y;i++)
      ptr[i] = g_ntohl(ptr[i]);
      
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
    scale_image_set_image(sni->image,NULL,NULL);
    return;
  }
  if(sni->string[icon]!=NULL)
  {
    scale_image_set_image(sni->image,sni->string[icon],sni->string[SNI_PROP_THEME]);
    return;
  }
  if(sni->pixbuf[pix-SNI_PROP_ICONPIX]!=NULL)
  {
    scale_image_set_pixbuf(sni->image,sni->pixbuf[pix-SNI_PROP_ICONPIX]);
    return;
  }
  return;
}

void sni_item_prop_cb ( GDBusConnection *con, GAsyncResult *res,
    struct sni_prop_wrapper *wrap)
{
  GVariant *result, *inner;
  gchar *param;

  wrap->sni->ref--;
  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result==NULL)
    return;
  g_variant_get(result, "(v)",&inner);
  if(wrap->prop<=SNI_PROP_THEME)
  {
    g_free(wrap->sni->string[wrap->prop]);
    if(inner && g_variant_is_of_type(inner,G_VARIANT_TYPE_STRING))
      g_variant_get(inner,"s",&param);
    else
      param=NULL;
    wrap->sni->string[wrap->prop] = g_strdup(param);
    g_debug("sni %s: property %s = %s",wrap->sni->dest,
        sni_properties[wrap->prop],wrap->sni->string[wrap->prop]);
  }
  if((wrap->prop>=SNI_PROP_ICONPIX)&&(wrap->prop<=SNI_PROP_ATTNPIX))
  {
    if(wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX]!=NULL)
      g_object_unref(wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX]);
    wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX] =
      sni_item_get_pixbuf(inner);
  }
  if(wrap->prop == SNI_PROP_MENU && inner &&
      g_variant_is_of_type(inner,G_VARIANT_TYPE_OBJECT_PATH))
    {
      g_free(wrap->sni->menu_path);
      g_variant_get(inner,"o",&param);
      wrap->sni->menu_path = g_strdup(param);
      g_debug("sni %s: property %s = %s",wrap->sni->dest,
          sni_properties[wrap->prop],wrap->sni->menu_path);
    }
  if(wrap->prop == SNI_PROP_ISMENU)
    g_variant_get(inner,"b",&(wrap->sni->menu));
  if(wrap->sni->string[SNI_PROP_STATUS]!=NULL)
  {
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
      sni_item_set_icon(wrap->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
    }
  }

  if(inner)
    g_variant_unref(inner);
  if(result)
    g_variant_unref(result);
  g_free(wrap);
  invalid = TRUE;
}

void sni_item_get_prop ( GDBusConnection *con, struct sni_item *sni,
    guint prop )
{
  struct sni_prop_wrapper *wrap;

  wrap = g_malloc(sizeof(struct sni_prop_wrapper));
  wrap->prop = prop;
  wrap->sni = sni;
  wrap->sni->ref++;

  g_dbus_connection_call(con, sni->dest, sni->path,
    "org.freedesktop.DBus.Properties", "Get",
    g_variant_new("(ss)", sni->iface, sni_properties[prop]),NULL,
    G_DBUS_CALL_FLAGS_NONE,-1,sni->cancel,
    (GAsyncReadyCallback)sni_item_prop_cb,wrap);
}

void sni_item_signal_cb (GDBusConnection *con, const gchar *sender,
         const gchar *path, const gchar *interface, const gchar *signal,
         GVariant *parameters, gpointer data)
{
  g_debug("sni: received signal %s from %s",signal,sender);
  if(g_strcmp0(signal,"NewTitle")==0)
    sni_item_get_prop(con,data,SNI_PROP_TITLE);
  if(g_strcmp0(signal,"NewStatus")==0)
    sni_item_get_prop(con,data,SNI_PROP_STATUS);
  if(g_strcmp0(signal,"NewToolTip")==0)
    sni_item_get_prop(con,data,SNI_PROP_TOOLTIP);
  if(g_strcmp0(signal,"NewIconThemePath")==0)
    sni_item_get_prop(con,data,SNI_PROP_THEME);
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

gboolean sni_item_scroll_cb ( GtkWidget *w, GdkEventScroll *event,
    gpointer data )
{
  struct sni_item *sni = data;
  GDBusConnection *con;
  gchar *dir;
  gint32 delta;

  if((event->direction == GDK_SCROLL_DOWN)||
      (event->direction == GDK_SCROLL_RIGHT))
    delta=1;
  else
    delta=-1;
  if((event->direction == GDK_SCROLL_DOWN)||
      (event->direction == GDK_SCROLL_UP))
    dir = "vertical";
  else
    dir = "horizontal";

  con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
  g_dbus_connection_call(con, sni->dest, sni->path,
    sni->iface, "Scroll", g_variant_new("(si)", dir, delta),
    NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  g_object_unref(con);
  return TRUE;
}

gboolean sni_item_click_cb (GtkWidget *w, GdkEventButton *event, gpointer data)
{
  struct sni_item *sni = data;
  GDBusConnection *con;
  gchar *method=NULL;
  GtkAllocation alloc,walloc;
  GdkRectangle geo;
  gint32 x,y;

  if(event->type == GDK_BUTTON_PRESS)
  {
    con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
    g_debug("sni %s: button: %d",sni->dest,event->button);
    if(event->button == 1)
    {
      if(sni->menu_path)
        sni_get_menu(sni,(GdkEvent *)event);
      else
      {
        if(sni->menu)
          method = "ContextMenu";
        else
          method = "Activate";
      }
    }
    if(event->button == 2)
      method = "SecondaryActivate";
    if(event->button == 3)
      method = "ContextMenu";

    gdk_monitor_get_geometry(gdk_display_get_monitor_at_window(
        gtk_widget_get_display(gtk_widget_get_toplevel(w)),
        gtk_widget_get_window(w)),&geo);
    gtk_widget_get_allocation(w,&alloc);
    gtk_widget_get_allocation(gtk_widget_get_toplevel(w),&walloc);

    if(get_toplevel_dir() == GTK_POS_RIGHT)
      x = geo.width - walloc.width + event->x + alloc.x;
    else
    {
      if(get_toplevel_dir() == GTK_POS_LEFT)
        x = walloc.width;
      else
        x = event->x + alloc.x;
    }

    if(get_toplevel_dir() == GTK_POS_BOTTOM)
      y = geo.height - walloc.height;
    else
    {
      if(get_toplevel_dir() == GTK_POS_TOP)
        y = walloc.height;
      else
        y = event->y + alloc.y;
    }

    // call event at 0,0 to avoid menu popping up under the bar
    if(method)
    {
      g_debug("sni: calling %s on %s at ( %d, %d )",method,sni->dest,x,y);
      g_dbus_connection_call(con, sni->dest, sni->path,
        sni->iface, method, g_variant_new("(ii)", 0, 0),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    }
    g_object_unref(con);
  }
  return TRUE;
}

void sni_item_new (GDBusConnection *con, struct sni_iface *iface,
    const gchar *uid)
{
  struct sni_item *sni;
  gchar *path;
  guint i;
  GList *iter;

  for(iter=sni_items;iter!=NULL;iter=g_list_next(iter))
    if(g_strcmp0(((struct sni_item *)iter->data)->uid,uid)==0)
      break;
  if(iter!=NULL)
    return;

  sni = g_malloc0(sizeof(struct sni_item));
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
  gtk_widget_add_events(GTK_WIDGET(sni->box),GDK_SCROLL_MASK);
  g_signal_connect(G_OBJECT(sni->box),"button-press-event",
      G_CALLBACK(sni_item_click_cb),sni);
  g_signal_connect(G_OBJECT(sni->box),"scroll-event",
      G_CALLBACK(sni_item_scroll_cb),sni);
  g_object_ref(sni->box);
  sni->signal = g_dbus_connection_signal_subscribe(con,sni->dest,
      sni->iface,NULL,sni->path,NULL,0,sni_item_signal_cb,sni,NULL);
  sni_items = g_list_append(sni_items,sni);
  for(i=0;i<16;i++)
    sni_item_get_prop(con,sni,i);
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
  sni_item_new(con,host->data,parameter);
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
    if(!g_strcmp0(((struct sni_item *)iter->data)->iface,watcher->item_iface))
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
    g_variant_new("(ss)", host->watcher_iface,"RegisteredStatusNotifierItems"),
    NULL,
    G_DBUS_CALL_FLAGS_NONE,-1,NULL,(GAsyncReadyCallback)sni_host_list_cb,host);

  g_debug("sni host %s: found watcher %s (%s)",host->host_iface,name,owner_name);
}

void sni_host_item_unregistered_cb ( GDBusConnection* con, const gchar* sender,
  const gchar* path, const gchar* interface, const gchar* signal,
  GVariant* parameters, gpointer data)
{
  struct sni_item *sni;
  const gchar *parameter;
  GList *item;
  gint i;
  struct sni_iface *host = data;

  g_variant_get (parameters, "(&s)", &parameter);
  g_debug("sni host %s: unregister item %s",host->host_iface, parameter);

  for(item=sni_items;item!=NULL;item=g_list_next(item))
    if(g_strcmp0(((struct sni_item *)item->data)->uid,parameter)==0)
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
  if(sni->box!=NULL)
    g_object_unref(sni->box);
  g_free(sni->menu_path);
  g_free(sni->uid);
  g_free(sni->path);
  g_free(sni->dest);
  g_free(sni->iface);
  g_free(sni);
  invalid = TRUE;
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

void sni_update ( void )
{
  GList *iter;
  struct sni_item *sni;

  if( !tray || !invalid )
    return;

  flow_grid_clean(tray);

  for(iter = sni_items;iter!=NULL;iter=g_list_next(iter))
  {
    sni = iter->data;
    if(sni)
      if(sni->string[SNI_PROP_STATUS])
        flow_grid_attach(tray,sni->box);
  }

  flow_grid_pad(tray);
  gtk_widget_show_all(tray);
  invalid = FALSE;
}

void sni_init ( GtkWidget *w )
{
  tray = w;
  sni_register("kde");
  sni_register("freedesktop");
}
