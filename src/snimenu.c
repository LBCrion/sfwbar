/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <glib.h>
#include <gio/gio.h>
#include <unistd.h>
#include "sfwbar.h"
#include "sni.h"
#include "menu.h"
#include "flowitem.h"

struct sni_menu_wrapper {
  GdkEvent *event;
  GtkWidget *widget;
  SniItem *sni;
};

gint32 sni_variant_get_int32 ( GVariant *dict, gchar *key, gint def )
{
  gint32 result;

  if(g_variant_lookup(dict, key, "i", &result))
    return result;
  return def;
}

const gchar *sni_variant_get_string ( GVariant *dict, gchar *key, gchar *def )
{
  gchar *result;

  if(g_variant_lookup(dict, key, "&s", &result))
    return result;
  return def;
}

gboolean sni_variant_get_bool ( GVariant *dict, gchar *key, gboolean def )
{
  gboolean result;

  if(g_variant_lookup(dict, key, "b", &result))
    return result;
  return def;
}

GtkWidget *sni_variant_get_pixbuf ( GVariant *dict, gchar *key )
{
  GVariant *ptr;
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  GtkWidget *img = NULL;
  guchar *buff;
  gsize len;

  ptr = g_variant_lookup_value(dict,key,G_VARIANT_TYPE_ARRAY);
  if(!ptr)
    return NULL;

  buff = (guchar *)g_variant_get_fixed_array(ptr,&len,sizeof(guchar));
  if(buff && len>0)
  {
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, buff, len, NULL);
    gdk_pixbuf_loader_close(loader,NULL);
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if(pixbuf)
      img = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(G_OBJECT(loader));
  }

  g_variant_unref(ptr);

  return img;
}

void sni_menu_item_cb ( GtkWidget *item, SniItem *sni )
{
  gint32 id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),"sni_menu_id"));

  if(!id)
    return;

  g_debug("sni menu call: %d (%s) %s",id,
      gtk_menu_item_get_label(GTK_MENU_ITEM(item)),sni->dest);

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      "com.canonical.dbusmenu", "Event",
      g_variant_new("(isvu)", id, "clicked", g_variant_new_int32(0),
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

GtkWidget *sni_get_menu_iter ( GVariantIter *iter, struct sni_menu_wrapper *wrap)
{
  GVariantIter *niter;
  GVariant *item, *dict;
  GtkWidget *menu, *mitem, *smenu;
  const gchar *toggle;
  gint32 id;
  GSList *group = NULL;

  menu = gtk_menu_new();
  gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);
  gtk_widget_set_name(menu, "tray");

  while(g_variant_iter_next(iter, "v", &item))
  {
    g_variant_get(item, "(i@a{sv}av)", &id, &dict, &niter);

    if(sni_variant_get_bool(dict, "visible", TRUE))
    {
      toggle = sni_variant_get_string(dict,"toggle-type","");

      if(!g_strcmp0(sni_variant_get_string(dict, "children-display", ""),
            "submenu"))
      {
        if((smenu=sni_get_menu_iter(niter, wrap)))
        {
          mitem = gtk_menu_item_new();
          gtk_menu_item_set_submenu(GTK_MENU_ITEM(mitem), smenu);
        }
      }
      else if(!g_strcmp0(sni_variant_get_string(dict, "type", "standard"),
            "separator"))
        mitem = gtk_separator_menu_item_new();
      else if(!g_strcmp0(toggle, "checkmark"))
      {
        mitem = gtk_check_menu_item_new();
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mitem),
            sni_variant_get_int32(dict, "toggle-state", 0));
      }
      else if(!g_strcmp0(toggle, "radio"))
      {
        mitem = gtk_radio_menu_item_new(group);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(mitem));
      }
      else
        mitem = gtk_menu_item_new();

      g_object_set_data(G_OBJECT(mitem),"sni_menu_id", GINT_TO_POINTER(id));
      sni_menu_item_decorate(mitem,dict);
      gtk_widget_set_sensitive(mitem,sni_variant_get_bool(dict, "enabled",
            TRUE));
      g_signal_connect(G_OBJECT(mitem), "activate",
          G_CALLBACK(sni_menu_item_cb), wrap->sni);
      gtk_container_add(GTK_CONTAINER(menu),mitem);
      g_variant_iter_free(niter);
    }

    g_variant_unref(dict);
    g_variant_unref(item);
  }

  return menu;
}

void sni_get_menu_cb ( GObject *src, GAsyncResult *res, gpointer data )
{
  GVariant *result;
  GVariantIter *iter;
  struct sni_menu_wrapper *wrap = data;
  GtkWidget *menu;
  gchar *tmp;

  result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, NULL);
  if(!result)
    return;
  
  tmp = g_variant_print(result,TRUE);
  g_debug("sni %s: menu: %s",wrap->sni->dest,tmp);
  g_free(tmp);

  g_variant_get(result, "(u(ia{sv}av))", NULL, NULL, NULL, &iter);

  menu = sni_get_menu_iter(iter, wrap);
  g_variant_iter_free(iter);

  if(menu)
  {
    g_object_ref_sink(G_OBJECT(menu));
    g_signal_connect(G_OBJECT(menu), "unmap", G_CALLBACK(g_object_unref), NULL);
    menu_popup(wrap->widget, menu, wrap->event, NULL, NULL);
  }

  gdk_event_free(wrap->event);
  g_free(wrap);
  if(result)
    g_variant_unref(result);
}

void sni_menu_ats_cb ( GObject *src, GAsyncResult *res, gpointer data )
{
  GVariant *result;
  struct sni_menu_wrapper *wrap = data;

  result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src),res,NULL);
  if(!result)
    return;
  g_variant_unref(result);

  g_dbus_connection_call(sni_get_connection(), wrap->sni->dest, wrap->sni->menu_path,
      "com.canonical.dbusmenu", "GetLayout",
      g_variant_new("(iias)", 0, -1, NULL),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, sni_get_menu_cb, wrap);
}

void sni_get_menu ( GtkWidget *widget, GdkEvent *event )
{
  SniItem *sni;
  struct sni_menu_wrapper *wrap = g_malloc(sizeof(struct sni_menu_wrapper));

  sni = flow_item_get_parent(widget);
  wrap->event = gdk_event_copy(event);
  wrap->sni = sni;
  wrap->widget = widget;

  g_debug("sni %s: requesting menu",wrap->sni->dest);

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      "com.canonical.dbusmenu", "AboutToShow", g_variant_new("(i)", 0),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, sni_menu_ats_cb, wrap);
}
