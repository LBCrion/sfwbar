/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include "sni.h"
#include "gui/menu.h"
#include "gui/menuitem.h"
#include "gui/scaleimage.h"

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
  gchar *id = NULL;
  gsize len;

  ptr = g_variant_lookup_value(dict, key, G_VARIANT_TYPE_ARRAY);
  if(!ptr)
    return NULL;

  buff = (guchar *)g_variant_get_fixed_array(ptr, &len, sizeof(guchar));
  if(buff && len>0)
  {
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, buff, len, NULL);
    gdk_pixbuf_loader_close(loader, NULL);
    if( (pixbuf = gdk_pixbuf_loader_get_pixbuf(loader)) )
    {
      id = g_strdup_printf("<pixbufcache/>snimenu-%d", pb_counter++);
      scale_image_cache_insert(id, gdk_pixbuf_copy(pixbuf));
    }
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

  if(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "sni_id"))==id)
    return submenu;

  children = gtk_container_get_children(GTK_CONTAINER(submenu));
  for(iter=children; iter; iter=g_list_next(iter))
    if( (node = sni_menu_item_find(iter->data, id)) )
      break;
  g_list_free(children);

  return iter?node:NULL;
}

static sni_item_t *sni_menu_item_get_sni_item ( GtkWidget *item )
{
  GtkWidget *parent;

  if( !(parent = gtk_widget_get_ancestor(item, GTK_TYPE_MENU)) )
    return NULL;

  while(GTK_IS_MENU_ITEM(gtk_menu_get_attach_widget(GTK_MENU(parent))))
    parent = gtk_widget_get_ancestor(
        gtk_menu_get_attach_widget(GTK_MENU(parent)), GTK_TYPE_MENU);
  return g_object_get_data(G_OBJECT(parent), "sni_item");
}

static void sni_menu_map_cb( GtkWidget *menu, sni_item_t *sni )
{
  GtkWidget *parent;
  gint id;

  if(!sni || g_object_get_data(G_OBJECT(sni->menu_obj), "suppress_ats"))
    return;

  parent = gtk_menu_get_attach_widget(GTK_MENU(menu));
  id = parent && GTK_IS_MENU_ITEM(parent)?
    GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parent), "sni_id")): 0;

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      sni_menu_iface, "AboutToShow", g_variant_new("(i)", id),
      G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      (GAsyncReadyCallback)sni_menu_about_to_show_cb, menu);
}

static gboolean sni_menu_cancel_ats_suppression ( GtkWidget *menu_obj )
{
  g_object_set_data(G_OBJECT(menu_obj), "suppress_ats",
      GINT_TO_POINTER(FALSE));
  return FALSE;
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

  g_object_set_data_full(G_OBJECT(item), "sni_props", g_variant_ref_sink(dict),
      (GDestroyNotify)g_variant_unref);
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

    menu_item_set_label_text(item, label);
    if(icon)
      menu_item_set_icon(item, icon);

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
      submenu = menu_new(NULL);
      g_signal_connect(G_OBJECT(submenu), "map",
          G_CALLBACK(sni_menu_map_cb), sni_menu_item_get_sni_item(item));
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
    }
    if(has_submenu && viter)
      sni_menu_parse(submenu, viter);
  }

  if(!g_variant_lookup(dict, "visible", "b", &state))
    state = TRUE;
  gtk_widget_set_visible(item, TRUE);
}

static void sni_menu_item_activate_cb ( GtkWidget *item, gpointer data )
{
  sni_item_t *sni;
  gint32 id;

  if( !(id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "sni_id"))) )
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
    item = menu_item_separator_new();
  else if(!g_strcmp0(toggle, "checkmark"))
  {
    item = menu_item_check_new();
    if(g_variant_lookup(dict, "toggle-state", "i", &state))
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), state);
  }
  else if(!g_strcmp0(toggle, "radio"))
  {
    item = menu_item_radio_new();
    gtk_radio_menu_item_set_group(GTK_RADIO_MENU_ITEM(item),
        GTK_IS_RADIO_MENU_ITEM(prev)?
        gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(prev)) : NULL);
  }
  else
    item = menu_item_new();

  g_object_set_data(G_OBJECT(item), "sni_id", GINT_TO_POINTER(id));

  g_signal_connect(G_OBJECT(item), "activate",
      G_CALLBACK(sni_menu_item_activate_cb), NULL);

  return item;
}

static void sni_menu_parse ( GtkWidget *menu, GVariantIter *viter )
{
  GtkWidget *item, *prev = NULL;
  GVariantIter *niter;
  GVariant *object, *dict;
  GList *remove, *iter, *children;
  guint32 id;
  gint pos = 0;

  children = gtk_container_get_children(GTK_CONTAINER(menu));
  remove = g_list_copy(children);
  while(g_variant_iter_next(viter, "v", &object))
  {
    g_variant_get(object, "(i@a{sv}av)", &id, &dict, &niter);
    g_variant_unref(object);
    for(iter=children; iter; iter=g_list_next(iter))
      if(menu_item_get_sort_index(iter->data)==(gint)id)
        break;
    if(iter)
    {
      item = iter->data;
      remove = g_list_remove(remove, item);
    }
    else
    {
      item = sni_menu_item_new(id, dict, prev);
      children = g_list_prepend(children, item);
      menu_item_insert(menu, item);
    }
    gtk_menu_reorder_child(GTK_MENU(menu), item, pos++);

    sni_menu_item_update(item, dict, niter);

    g_variant_unref(dict);
    g_variant_iter_free(niter);
    prev = item;
  }
  gtk_menu_shell_deselect(GTK_MENU_SHELL(menu));
  g_list_free_full(remove, (GDestroyNotify)gtk_widget_destroy);
  g_list_free(children);
}

static void sni_menu_get_layout_cb ( GObject *src, GAsyncResult *res,
    gpointer data )
{
  sni_item_t *sni = data;
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

  menu = sni_menu_item_find(sni->menu_obj, id);
  if( (parent = gtk_menu_get_attach_widget(GTK_MENU(menu))) )
    sni_menu_item_update(parent, dict, iter);
  else
    sni_menu_parse(menu, iter);

  g_variant_iter_free(iter);
  g_variant_unref(dict);
  g_variant_unref(result);
  g_timeout_add(1000, G_SOURCE_FUNC(sni_menu_cancel_ats_suppression),
      sni->menu_obj);
}

static void sni_menu_about_to_show_cb ( GDBusConnection *con, GAsyncResult *res,
    gpointer data )
{
  GtkWidget *parent, *item = data;
  sni_item_t *sni;
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
      id = parent? menu_item_get_sort_index(parent) : 0;
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

static GtkWidget *sni_menu_item_by_id ( GtkWidget *menu, gint32 id )
{
  GtkWidget *item = NULL, *submenu;
  GList *children, *iter;

  children = gtk_container_get_children(GTK_CONTAINER(menu));
  for(iter=children; iter && !item; iter=g_list_next(iter))
  {
    if(menu_item_get_sort_index(iter->data)==id)
      item = iter->data;
    else if( (submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(iter->data))) )
      item = sni_menu_item_by_id(submenu, id);
  }
  g_list_free(children);

  return item;
}

/* ItemsPropertiesUpdated carries only the changes, while
 * sni_menu_item_update() expects all properties: apply the changes on top of
 * the last known set */
static void sni_menu_item_props_update ( GtkWidget *menu, gint32 id,
    GVariant *set, const gchar **unset )
{
  GtkWidget *item;
  GVariantDict dict;
  GVariantIter iter;
  GVariant *value;
  const gchar *key;

  if( !(item = sni_menu_item_by_id(menu, id)) )
    return;

  g_variant_dict_init(&dict, g_object_get_data(G_OBJECT(item), "sni_props"));
  if(set)
  {
    g_variant_iter_init(&iter, set);
    while(g_variant_iter_next(&iter, "{&sv}", &key, &value))
    {
      g_variant_dict_insert_value(&dict, key, value);
      g_variant_unref(value);
    }
  }
  while(unset && *unset)
    g_variant_dict_remove(&dict, *unset++);

  sni_menu_item_update(item, g_variant_dict_end(&dict), NULL);
}

static void sni_menu_items_properties_updated_cb (GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *interface,
    const gchar *signal, GVariant *parameters, gpointer data)
{
  sni_item_t *sni = data;
  GVariantIter *updated, *removed;
  GVariant *props;
  const gchar **keys;
  gint32 id;

  g_variant_get(parameters, "(a(ia{sv})a(ias))", &updated, &removed);
  while(g_variant_iter_next(updated, "(i@a{sv})", &id, &props))
  {
    sni_menu_item_props_update(sni->menu_obj, id, props, NULL);
    g_variant_unref(props);
  }
  while(g_variant_iter_next(removed, "(i^a&s)", &id, &keys))
  {
    sni_menu_item_props_update(sni->menu_obj, id, NULL, keys);
    g_free(keys);
  }
  g_variant_iter_free(updated);
  g_variant_iter_free(removed);
}

static void sni_menu_layout_updated_cb (GDBusConnection *con,
    const gchar *sender, const gchar *path, const gchar *interface,
    const gchar *signal, GVariant *parameters, gpointer data)
{
  sni_item_t *sni = data;
  guint32 rev;
  gint32 id;

  g_variant_get(parameters, "(ui)", &rev, &id);
  g_debug("sni menu: update: %s, id: %d, rev: %u", sni->dest, id, rev);
  g_object_set_data(G_OBJECT(sni->menu_obj), "suppress_ats", GINT_TO_POINTER(TRUE));
  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      sni_menu_iface, "GetLayout", g_variant_new("(iias)", 0, -1, NULL),
      G_VARIANT_TYPE("(u(ia{sv}av))"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      sni_menu_get_layout_cb, sni);
}

void sni_menu_init ( sni_item_t *sni )
{
  if(sni->menu_obj)
    gtk_widget_destroy(sni->menu_obj);

  sni->menu_obj = menu_new(NULL);
  g_signal_connect(G_OBJECT(sni->menu_obj), "destroy",
      G_CALLBACK(g_source_remove_by_user_data), NULL);
  g_signal_connect(G_OBJECT(sni->menu_obj), "map",
      G_CALLBACK(sni_menu_map_cb), sni);

  g_object_set_data(G_OBJECT(sni->menu_obj), "sni_item", sni);

  g_dbus_connection_signal_subscribe(sni_get_connection(), sni->dest,
      sni_menu_iface, "LayoutUpdated", sni->menu_path, NULL, 0,
      sni_menu_layout_updated_cb, sni, NULL);
  g_dbus_connection_signal_subscribe(sni_get_connection(), sni->dest,
      sni_menu_iface, "ItemsPropertiesUpdated", sni->menu_path, NULL, 0,
      sni_menu_items_properties_updated_cb, sni, NULL);

  g_dbus_connection_call(sni_get_connection(), sni->dest, sni->menu_path,
      sni_menu_iface, "GetLayout", g_variant_new("(iias)", 0, -1, NULL),
      G_VARIANT_TYPE("(u(ia{sv}av))"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
      sni_menu_get_layout_cb, sni);
}
