/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include <gio/gdesktopappinfo.h>
#include "locale1.h"
#include "gui/css.h"
#include "gui/menu.h"
#include "gui/menuitem.h"
#include "gui/scaleimage.h"
#include "vm/expr.h"

static GHashTable *menu_items;

static void menu_item_activate ( GtkMenuItem *self, gpointer d )
{
  MenuItemPrivate *priv;
  GDesktopAppInfo *app;
  GtkWidget *parent, *widget;
  gpointer wid;
  guint16 state;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");

  if(!priv->action && priv->desktop_file)
    if( (app = g_desktop_app_info_new_from_filename(priv->desktop_file)) )
    {
      g_app_info_launch((GAppInfo *)app, NULL, NULL, NULL);
      g_object_unref(app);
      return;
    }

  if(!priv->action)
    return;
  if( (parent = gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_MENU)) )
  {
    wid = g_object_get_data (G_OBJECT(parent), "wid");
    state = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(parent), "state"));
    widget = g_object_get_data (G_OBJECT(parent), "caller");
  }
  else
  {
    wid = NULL;
    state = 0;
    widget = NULL;
  }

  if(!wid)
    wid = wintree_get_focus();

  action_exec(widget, priv->action, NULL, wintree_from_id(wid), &state);
}

void menu_item_update_from_app ( GtkWidget *self, GDesktopAppInfo *app )
{
  GKeyFile *keyfile;
  gchar *icon, *comment, *label = NULL;
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  g_free(priv->desktop_file);
  priv->desktop_file = g_strdup(g_desktop_app_info_get_filename(app));

  if( !(priv->flags & MI_LABEL))
  {
    keyfile = g_key_file_new();
    if(priv->desktop_file && g_key_file_load_from_file(keyfile,
          priv->desktop_file, G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
      if( !(label = g_key_file_get_locale_string(keyfile,
              G_KEY_FILE_DESKTOP_GROUP, "X-GNOME-FullName",
              locale1_get_locale(), NULL)) )
        label = g_key_file_get_locale_string(keyfile,
            G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME,
            locale1_get_locale(), NULL);
    }
    if(!label)
      label = g_strdup(g_app_info_get_display_name((GAppInfo *)app));
    g_key_file_unref(keyfile);
    if(label)
      gtk_label_set_text_with_mnemonic(GTK_LABEL(priv->label), label);
    g_free(label);
  }

  if( !(priv->flags & MI_ICON) )
  {
    if( (icon = g_desktop_app_info_get_string(app, "Icon")) )
    {
      scale_image_set_image(priv->icon, icon, NULL);
      css_set_class(priv->icon, "hidden", FALSE);
    }
    g_free(icon);
  }
  if( !(priv->flags & MI_TOOLTIP) )
  {
    if( (comment = g_desktop_app_info_get_locale_string(app, "Comment")) )
      gtk_widget_set_tooltip_text(self, comment);
    g_free(comment);
  }
}

void menu_item_update_from_desktop ( GtkWidget *self, const gchar *desktop_id )
{
  GDesktopAppInfo *app;

  if( (app = g_desktop_app_info_new(desktop_id)) )
    menu_item_update_from_app(self, app);
}

void menu_item_set_id ( GtkWidget *self, gchar *id )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  if(priv->id)
    g_hash_table_remove(menu_items, priv->id);
  priv->id = g_strdup(id);
  if(priv->id)
    g_hash_table_insert(menu_items, priv->id, self);
}

gchar *menu_item_get_id ( GtkWidget *self )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_val_if_fail(priv, NULL);

  return priv->id;
}

void menu_item_set_label ( GtkWidget *self, const gchar *label )
{
  MenuItemPrivate *priv;
  gchar *icon;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv && priv->label);

  if( (icon = strchr(label, '%')) )
  {
    *icon = 0;
    gtk_label_set_text_with_mnemonic(GTK_LABEL(priv->label), label);
    *icon = '%';
    menu_item_set_icon(self, icon+1);
  }
  else
    gtk_label_set_text_with_mnemonic(GTK_LABEL(priv->label), label);

  priv->flags |= MI_LABEL;
}

void menu_item_set_label_expr ( GtkWidget *self, expr_cache_t *expr )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv && priv->label);

  expr_cache_free(priv->label_expr);
  priv->label_expr = expr;
}

void menu_item_set_icon ( GtkWidget *self, const gchar *icon )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv && priv->icon);

  if(icon && *icon)
    scale_image_set_image(priv->icon, icon, NULL);
  css_set_class(priv->icon, "hidden", !icon || !*icon);
  priv->flags |= MI_ICON;
}

void menu_item_set_tooltip ( GtkWidget *self, gchar *tooltip )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  gtk_widget_set_tooltip_text(self, tooltip);
  priv->flags |= MI_TOOLTIP;
}

void menu_item_set_action ( GtkWidget *self, GBytes *action )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  g_bytes_unref(priv->action);
  priv->action = action;
}

void menu_item_set_sort_index ( GtkWidget *self, gint index )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  priv->sort_index = index;
}

gint menu_item_get_sort_index ( GtkWidget *self )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  if(!priv)
    g_message("%p %d", self, GTK_IS_MENU_ITEM(self));
  g_return_val_if_fail(priv, 0);

  return priv->sort_index;
}

gint menu_item_compare ( GtkWidget *i1, GtkWidget *i2 )
{
  MenuItemPrivate *p1, *p2;
  const gchar *t1, *t2;

  p1 = g_object_get_data(G_OBJECT(i1), "menu_item_private");
  p2 = g_object_get_data(G_OBJECT(i2), "menu_item_private");

  g_return_val_if_fail(p1 && p2, 0);

  if(p1->sort_index != p2->sort_index)
    return p1->sort_index - p2->sort_index;

  t1 = gtk_label_get_text(GTK_LABEL(p1->label));
  t2 = gtk_label_get_text(GTK_LABEL(p2->label));

  if(!t1 || !t2)
    return 0;

  return g_strcmp0(t1, t2);
}

void menu_item_set_submenu ( GtkWidget *self, gchar *subname )
{
  MenuItemPrivate *priv;
  GtkWidget *submenu;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  if( (submenu = menu_new(subname)) )
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(self), submenu);
}

static void menu_item_priv_free( MenuItemPrivate *priv )
{
  if(!priv)
    return;

  if(priv->id)
    g_hash_table_remove(menu_items, priv->id);
  g_clear_pointer(&priv->desktop_file, g_free);
  g_clear_pointer(&priv->action, g_bytes_unref);
  g_free(priv);
}

void menu_item_init( GtkWidget *self )
{
  MenuItemPrivate *priv;

  priv = g_malloc0(sizeof(MenuItemPrivate));
  g_object_set_data_full(G_OBJECT(self), "menu_item_private", priv,
      (GDestroyNotify)menu_item_priv_free);
  g_signal_connect(G_OBJECT(self), "activate", G_CALLBACK(menu_item_activate),
      NULL);
  gtk_widget_set_name(GTK_WIDGET(self), "menu_item");
  if(!GTK_IS_SEPARATOR_MENU_ITEM(self))
  {
    priv->box = gtk_grid_new();
    priv->icon = scale_image_new();
    gtk_grid_attach(GTK_GRID(priv->box), priv->icon, 1, 1, 1, 1);
    priv->label = gtk_label_new_with_mnemonic("");
    gtk_grid_attach(GTK_GRID(priv->box), priv->label, 2, 1, 1, 1);
    gtk_container_add(GTK_CONTAINER(self), priv->box);
  }
  if(priv->icon)
    css_set_class(priv->icon, "hidden", TRUE);
  gtk_widget_show_all(GTK_WIDGET(self));
}

GtkWidget *menu_item_get ( gchar *id, gboolean create )
{
  GtkWidget *self;

  if(!menu_items)
    menu_items = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  if( id && (self = g_hash_table_lookup(menu_items, id)) )
    return self;

  if(!create)
    return NULL;

  self = gtk_menu_item_new();
  menu_item_init(self);
  menu_item_set_id(self, id);

  return self;
}

void menu_item_remove ( gchar *id )
{
  GtkWidget *item;

  if(!menu_items || !(item = g_hash_table_lookup(menu_items, id)) )
    return;

  if(gtk_menu_item_get_submenu(GTK_MENU_ITEM(item)))
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), NULL);

  gtk_widget_destroy(item);
}

void menu_item_label_update ( GtkWidget *self )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  if(expr_cache_eval(priv->label_expr))
    menu_item_set_label(self, priv->label_expr->cache);
}

void menu_item_insert ( GtkWidget *menu, GtkWidget *item )
{
  GList *list, *iter;
  gint count = 0;

  if(!GTK_IS_MENU_ITEM(item))
  {
    gtk_container_add(GTK_CONTAINER(menu), item);
    return;
  }

  list = gtk_container_get_children(GTK_CONTAINER(menu));
  for(iter=list; iter; iter=g_list_next(iter))
  {
    if(GTK_IS_MENU_ITEM(iter->data) && menu_item_compare(item, iter->data)<=0)
      break;
    count++;
  }
  g_list_free(list);
  gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, count);
}

