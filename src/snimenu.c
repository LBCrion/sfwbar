/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include <glib.h>
#include <gio/gio.h>
#include <unistd.h>
#include "sfwbar.h"
#include "sni.h"
#include "menu.h"

struct sni_menu_wrapper {
  GdkEvent *event;
  SniItem *sni;
};

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

GtkWidget *sni_variant_get_pixbuf ( GVariant *dict, gchar *key )
{
  GVariant *ptr,*item;
  GVariantIter iter;
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  GtkWidget *img = NULL;
  guchar *buff;
  gint i=0,len;

  ptr = g_variant_lookup_value(dict,key,G_VARIANT_TYPE_ARRAY);
  if(!ptr)
    return NULL;

  g_variant_iter_init(&iter,ptr);
  len = g_variant_iter_n_children(&iter);
  buff = g_malloc(len);
  while( (item = g_variant_iter_next_value(&iter)) && i<len )
  {
    if(g_variant_is_of_type(item,G_VARIANT_TYPE_BYTE))
      buff[i++]=g_variant_get_byte(item);
    g_variant_unref(item);
  }

  if( i == len )
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

void sni_menu_item_cb ( GtkWidget *item, SniItem *sni )
{
  gint32 *id = g_object_get_data(G_OBJECT(item),"sni_id");

  if(!id)
    return;

  g_debug("sni menu call: %d (%s) %s",*id,
      gtk_menu_item_get_label(GTK_MENU_ITEM(item)),sni->dest);

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      "com.canonical.dbusmenu", "Event",
      g_variant_new("(isvu)", *id, "clicked", g_variant_new_int32(0),
        gtk_get_current_event_time()),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
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
  gchar *tmp;

  result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src),res,NULL);
  if(result)
  {
    tmp = g_variant_print(result,TRUE);
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
    css_widget_cascade(menu,NULL);
    g_object_ref_sink(G_OBJECT(menu));
    g_signal_connect(G_OBJECT(menu),"unmap",G_CALLBACK(g_object_unref),NULL);
    menu_popup(wrap->sni->image,menu,wrap->event,NULL,NULL);
  }

  gdk_event_free(wrap->event);
  g_free(wrap);
  if(result)
    g_variant_unref(result);
}

void sni_get_menu ( SniItem *sni, GdkEvent *event )
{
  struct sni_menu_wrapper *wrap = g_malloc(sizeof(struct sni_menu_wrapper));

  wrap->event = gdk_event_copy(event);
  wrap->sni = sni;

  g_debug("sni %s: requesting menu",wrap->sni->dest);

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      "com.canonical.dbusmenu", "GetLayout",
      g_variant_new("(iias)", 0, -1, NULL),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, sni_get_menu_cb, wrap);
}
