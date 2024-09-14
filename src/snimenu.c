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
#include "scaleimage.h"
#include "flowitem.h"

const gchar *sni_menu_iface = "com.canonical.dbusmenu";

static void sni_menu_parse ( GtkWidget *widget, GVariantIter *viter );
static void sni_menu_about_to_show_cb ( GDBusConnection *, GAsyncResult *,
    gpointer);

gchar *sni_menu_get_pixbuf ( GVariant *dict, gchar *key )
{
  GVariant *ptr;
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  static gint pb_counter;
  guchar *buff;
  gchar *id;
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
    if( (pixbuf = gdk_pixbuf_loader_get_pixbuf(loader)) )
    {
      id = g_strdup_printf("<pixbufcache/>snimenu-%d", pb_counter++);
      scale_image_cache_insert(id, gdk_pixbuf_copy(pixbuf));
    }
    else
      id = NULL;
    g_object_unref(G_OBJECT(loader));
  }

  g_variant_unref(ptr);

  return id;
}

static GtkWidget *sni_menu_item_find ( GtkWidget *widget, gint32 id )
{
  GtkWidget *node, *submenu;
  GList *children, *iter;

  if(!id)
    return widget;

  if( !(submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))) )
    return NULL;

  if(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "id"))==id)
    return submenu;

  children = gtk_container_get_children(GTK_CONTAINER(submenu));
  for(iter=children; iter; iter=g_list_next(iter))
    if( (node = sni_menu_item_find(iter->data, id)) )
      break;
  g_list_free(children);

  return iter?node:NULL;
}

static SniItem *sni_menu_item_get_sni_item ( GtkWidget *item )
{
  GtkWidget *parent;

  parent = gtk_widget_get_ancestor(item, GTK_TYPE_MENU);
  while(GTK_IS_MENU_ITEM(gtk_menu_get_attach_widget(GTK_MENU(parent))))
    parent = gtk_widget_get_ancestor(
        gtk_menu_get_attach_widget(GTK_MENU(parent)), GTK_TYPE_MENU);
  return g_object_get_data(G_OBJECT(parent), "sni_item");
}

static void sni_menu_map_cb( GtkWidget *menu, SniItem *sni )
{
  if(sni)
    g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
        sni_menu_iface, "AboutToShow", g_variant_new("(i)",
          GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu), "id"))),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
        (GAsyncReadyCallback)sni_menu_about_to_show_cb, menu);
}

static void sni_menu_pixbuf_free ( gchar *id )
{
  scale_image_cache_remove(id);
  g_free(id);
}

static void sni_menu_item_update ( GtkWidget *item, GVariant *dict,
    GVariantIter *viter )
{
  GtkWidget *submenu;
  const gchar *label, *icon, *sub;
  gboolean has_submenu, state;

  gtk_widget_set_name(item, "tray");

  if(!GTK_IS_SEPARATOR_MENU_ITEM(item))
  {
    if( !(g_variant_lookup(dict, "icon-name", "&s", &icon)) )
    {
      icon = sni_menu_get_pixbuf(dict, "icon-data");
      g_object_set_data_full(G_OBJECT(item), "pixbuf", (gpointer)icon,
          (GDestroyNotify)sni_menu_pixbuf_free);
    }

    if( !(g_variant_lookup(dict, "label", "&s", &label)) )
      label = "";

    menu_item_update(item, label, icon);

    has_submenu = g_variant_lookup(dict, "children-display", "&s", &sub) &&
      !g_strcmp0(sub, "submenu");

    submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
    if(submenu && !has_submenu)
    {
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), NULL);
      gtk_widget_destroy(submenu);
    }
    if(!submenu && has_submenu)
    {
      submenu = gtk_menu_new();
      g_signal_connect(G_OBJECT(submenu), "map",
          G_CALLBACK(sni_menu_map_cb), sni_menu_item_get_sni_item(item));
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
    }
    if(has_submenu && viter)
      sni_menu_parse(submenu, viter);
  }

  gtk_widget_show_all(item);
  if(!g_variant_lookup(dict, "visible", "b", &state))
    state = TRUE;
  gtk_widget_set_visible(item, TRUE);
}

static void sni_menu_item_activate_cb ( GtkWidget *item, gpointer data )
{
  SniItem *sni;
  gint32 id;

  if( !(id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "id"))) )
    return;

  if( !(sni = sni_menu_item_get_sni_item(item)) )
    return;

  g_debug("sni menu call: %d (%s) %s",id,
      gtk_menu_item_get_label(GTK_MENU_ITEM(item)),sni->dest);

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      "com.canonical.dbusmenu", "Event",
      g_variant_new("(isvu)", id, "clicked", g_variant_new_int32(0),
        gtk_get_current_event_time()),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static GtkWidget *sni_menu_item_new ( guint32 id, GVariant *dict,
    GtkWidget *prev )
{
  GtkWidget *item;
  const gchar *toggle, *type;
  gint32 state;

  if( !(g_variant_lookup(dict, "toggle-type", "&s", &toggle)) )
    toggle = NULL;
  if(g_variant_lookup(dict, "type", "&s", &type) &&
      !g_strcmp0(type, "separator"))
    item = gtk_separator_menu_item_new();
  else if(!g_strcmp0(toggle, "checkmark"))
  {
    item = gtk_check_menu_item_new();
    if(g_variant_lookup(dict, "toggle-state", "i", &state))
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), state);
  }
  else if(!g_strcmp0(toggle, "radio"))
    item = gtk_radio_menu_item_new_from_widget(GTK_RADIO_MENU_ITEM(prev));
  else
    item = gtk_menu_item_new();

  g_object_set_data(G_OBJECT(item), "id", GINT_TO_POINTER(id));
  g_signal_connect(G_OBJECT(item), "activate",
      G_CALLBACK(sni_menu_item_activate_cb), NULL);

  return item;
}

static void sni_menu_parse ( GtkWidget *widget, GVariantIter *viter )
{
  GtkWidget *item, *prev = NULL;
  GVariantIter *niter;
  GVariant *object, *dict;
  GList *children, *remove, *iter;
  guint32 id, position;

  children = gtk_container_get_children(GTK_CONTAINER(widget));
  remove = g_list_copy(children);
  while(g_variant_iter_next(viter, "v", &object))
  {
    g_variant_get(object, "(i@a{sv}av)", &id, &dict, &niter);
    g_variant_unref(object);
    position = 0;
    for(iter=children; iter; iter=g_list_next(iter))
      if(id > GPOINTER_TO_INT(g_object_get_data(iter->data, "id")))
        position++;
    for(iter=children; iter; iter=g_list_next(iter))
      if(GPOINTER_TO_INT(g_object_get_data(iter->data, "id"))==id)
        break;

    item = iter?iter->data:sni_menu_item_new(id, dict, prev);
    remove = g_list_remove(remove, item);

    if(!iter)
    {
      gtk_menu_shell_insert(GTK_MENU_SHELL(widget), item, position);
      children = g_list_prepend(children, item);
    }
    sni_menu_item_update(item, dict, niter);

    g_variant_unref(dict);
    g_variant_iter_free(niter);
    prev = item;
  }
  gtk_menu_shell_deselect(GTK_MENU_SHELL(widget));
  for(iter=remove; iter; iter=g_list_next(iter))
    gtk_widget_destroy(iter->data);
  g_list_free(remove);
  g_list_free(children);
}

static void sni_menu_get_layout_cb ( GObject *src, GAsyncResult *res,
    gpointer data )
{
  SniItem *sni = data;
  GtkWidget *menu, *parent;
  GVariant *result, *dict;
  GVariantIter *iter;
  gchar *tmp;
  guint32 rev, id;

  if( !(result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res,
          NULL)) )
    return;

  tmp = g_variant_print(result, TRUE);
  g_debug("sni %s: menu: %s", sni->dest, tmp);
  g_free(tmp);

  g_variant_get(result, "(u(i@a{sv}av))", &rev, &id, &dict, &iter);

  if(rev > GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sni->menu_obj), "rev")))
  {
    menu = sni_menu_item_find(sni->menu_obj, id);
    if( (parent = gtk_menu_get_attach_widget(GTK_MENU(menu))) )
      sni_menu_item_update(parent, dict, iter);
    else
      sni_menu_parse(menu, iter);
    g_object_set_data(G_OBJECT(sni->menu_obj), "rev", GINT_TO_POINTER(rev));
  }

  g_variant_iter_free(iter);
  g_variant_unref(dict);
  g_variant_unref(result);
}

static void sni_menu_about_to_show_cb ( GDBusConnection *con, GAsyncResult *res,
    gpointer data )
{
  GtkWidget *parent, *item = data;
  SniItem *sni;
  GVariant *result;
  gboolean refresh;
  gint32 id;

  if( (result = g_dbus_connection_call_finish(con, res, NULL)) )
  {
    g_variant_get(result, "(b)", &refresh);
    if(refresh)
    {
      sni = sni_menu_item_get_sni_item(item);
      parent = gtk_menu_get_attach_widget(GTK_MENU(item));
      id = parent?GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent), "id")):0;
      g_debug("sni: menu refresh: '%s', node: %d", sni->dest, id);
      g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
          sni_menu_iface, "GetLayout", g_variant_new("(iias)", id, -1, NULL),
            G_VARIANT_TYPE("(u(ia{sv}av))"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
          sni_menu_get_layout_cb, sni);
    }
    g_variant_unref(result);
  }
  else
    g_warning("sni error");
}

static void sni_menu_items_properties_updated_cb (GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *interface,
    const gchar *signal, GVariant *parameters, gpointer data)
{
  g_warning("sni: menu: unhandled ItemsPropertiesUpdated signal");
}

static void sni_menu_layout_updated_cb (GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *interface,
    const gchar *signal, GVariant *parameters, gpointer data)
{
  SniItem *sni = data;
  guint32 rev;
  gint32 id;

  g_variant_get(parameters, "(ui)", &rev, &id);
  g_debug("sni menu: update: %s, id: %d, rev: %u", sni->dest, id, rev);
  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      sni_menu_iface, "GetLayout", g_variant_new("(iias)", 0, -1, NULL),
      G_VARIANT_TYPE("(u(ia{sv}av))"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      sni_menu_get_layout_cb, sni);
}

void sni_menu_init ( SniItem *sni )
{
  if(sni->menu_obj)
    gtk_widget_destroy(sni->menu_obj);

  sni->menu_obj = gtk_menu_new();
  g_signal_connect(G_OBJECT(sni->menu_obj), "map",
      G_CALLBACK(sni_menu_map_cb), sni);

  g_object_set_data(G_OBJECT(sni->menu_obj), "sni_item", sni);

  g_dbus_connection_signal_subscribe(sni_get_connection(), sni->dest,
      sni_menu_iface, "LayoutUpdated", sni->menu_path, NULL, 0,
      sni_menu_layout_updated_cb, sni, NULL);
  g_dbus_connection_signal_subscribe(sni_get_connection(), sni->dest,
      sni_menu_iface, "ItemPropertiesUpdated", sni->menu_path, NULL, 0,
      sni_menu_items_properties_updated_cb, sni, NULL);

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      sni_menu_iface, "GetLayout", g_variant_new("(iias)", 0, -1, NULL),
      G_VARIANT_TYPE("(u(ia{sv}av))"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      sni_menu_get_layout_cb, sni);
}
