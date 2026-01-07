/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include <gio/gdesktopappinfo.h>
#include "exec.h"
#include "locale1.h"
#include "scanner.h"
#include "gui/css.h"
#include "gui/menu.h"
#include "gui/menuitem.h"
#include "gui/scaleimage.h"
#include "util/string.h"
#include "vm/vm.h"

static GHashTable *menu_items;

enum {
  MENU_ITEM_DESKTOPID = 1,
  MENU_ITEM_VALUE,
  MENU_ITEM_STYLE,
  MENU_ITEM_TOOLTIP,
  MENU_ITEM_ACTION,
  MENU_ITEM_MENU,
  MENU_ITEM_INDEX,
  MENU_ITEM_ICON,
  MENU_ITEM_CSS,
};

G_DEFINE_INTERFACE(MenuIface, menu_iface, G_TYPE_OBJECT)

static void menu_item_set_style (GtkWidget *self, GBytes *code )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);
  expr_update(&priv->style_expr, code);
}

static void menu_item_set_css (GtkWidget *self, const gchar *css )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  if(!g_strcmp0(priv->css, css))
    return;

  str_assign(&priv->css, g_strdup(css));
  if(priv->provider)
    gtk_style_context_remove_provider(gtk_widget_get_style_context(
          GTK_WIDGET(self)), GTK_STYLE_PROVIDER(priv->provider));
  priv->provider = css_widget_apply(GTK_WIDGET(self), priv->css);
}

static void menu_item_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  if(id == MENU_ITEM_DESKTOPID)
    menu_item_update_from_desktop(GTK_WIDGET(self), g_value_get_string(value));
  else if(id == MENU_ITEM_VALUE)
    menu_item_set_label_expr(GTK_WIDGET(self), g_value_get_boxed(value));
  else if(id == MENU_ITEM_STYLE)
    menu_item_set_style(GTK_WIDGET(self), g_value_get_boxed(value));
  else if(id == MENU_ITEM_TOOLTIP)
    menu_item_set_tooltip(GTK_WIDGET(self), g_value_get_boxed(value));
  else if(id == MENU_ITEM_ACTION)
    menu_item_set_action(GTK_WIDGET(self), g_value_get_boxed(value));
  else if(id == MENU_ITEM_MENU)
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(self), g_object_ref(g_value_get_object(value)));
  else if(id == MENU_ITEM_INDEX)
    menu_item_set_sort_index(GTK_WIDGET(self), g_value_get_int(value));
  else if(id == MENU_ITEM_ICON)
    menu_item_set_icon(GTK_WIDGET(self), g_value_get_string(value));
  else if(id == MENU_ITEM_CSS)
    menu_item_set_css(GTK_WIDGET(self), g_value_get_string(value));
}

static void menu_iface_class_init ( GObjectClass *class )
{
  class->set_property = menu_item_set_property;
  g_object_class_install_property(class, MENU_ITEM_DESKTOPID,
      g_param_spec_string("desktopid", "desktopid", "sfwbar_config", NULL,
        G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_VALUE,
      g_param_spec_boxed("value", "value", "sfwbar_config", G_TYPE_BYTES,
        G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_STYLE,
      g_param_spec_boxed("style", "style", "sfwbar_config", G_TYPE_BYTES,
        G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_TOOLTIP,
      g_param_spec_boxed("tooltip", "tooltip", "sfwbar_config", G_TYPE_BYTES,
        G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_ACTION,
      g_param_spec_boxed("action", "action", "sfwbar_config:a", G_TYPE_BYTES,
        G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_MENU,
      g_param_spec_object("menu", "menu", "sfwbar_config:s",
        GTK_TYPE_WIDGET, G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_INDEX,
      g_param_spec_int("index", "index", "sfwbar_config",
        0, INT_MAX, 0, G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_ICON,
      g_param_spec_string("icon", "icon", "sfwbar_config", NULL,
        G_PARAM_WRITABLE));
  g_object_class_install_property(class, MENU_ITEM_CSS,
      g_param_spec_string("css", "css", "sfwbar_config", NULL,
        G_PARAM_WRITABLE));
}

static void menu_item_priv_free( MenuItemPrivate *priv )
{
  if(!priv)
    return;

  if(priv->id)
    g_hash_table_remove(menu_items, priv->id);
  g_clear_pointer(&priv->label_expr, expr_cache_unref);
  g_clear_pointer(&priv->style_expr, expr_cache_unref);
  g_clear_pointer(&priv->tooltip_expr, expr_cache_unref);
  g_clear_pointer(&priv->desktop_file, g_free);
  g_clear_pointer(&priv->action, g_bytes_unref);
  g_clear_pointer(&priv->css, g_free);
  g_clear_pointer(&priv->provider, g_object_unref);
  g_free(priv);
}

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
      exec_launch(app);
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

  vm_run_action(priv->action, widget, NULL, wintree_from_id(wid), &state, NULL,
      NULL);
}

static void menu_item_init_common( GtkWidget *self )
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

DEFINE_MENU_ITEM(MenuItem, menu_item, MENU, ITEM, GTK_TYPE_MENU_ITEM)
DEFINE_MENU_ITEM(MenuItemCheck, menu_item_check, MENU, ITEM_CHECK, GTK_TYPE_CHECK_MENU_ITEM)
DEFINE_MENU_ITEM(MenuItemRadio, menu_item_radio, MENU, ITEM_RADIO, GTK_TYPE_RADIO_MENU_ITEM)
DEFINE_MENU_ITEM(MenuItemSeparator, menu_item_separator, MENU, ITEM_SEPARATOR, GTK_TYPE_SEPARATOR_MENU_ITEM)

static void menu_iface_default_init ( MenuIfaceInterface *a )
{
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
  gchar *desktop_file;

  if(GTK_IS_SEPARATOR_MENU_ITEM(self))
    return;
  if(g_str_has_suffix(desktop_id, ".desktop"))
    desktop_file = g_strdup(desktop_id);
  else
    desktop_file = g_strconcat(desktop_id, ".desktop", NULL);

  if( (app = g_desktop_app_info_new(desktop_file)) )
    menu_item_update_from_app(self, app);
  g_free(desktop_file);
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
  gchar *ptr, *text;

  if(GTK_IS_SEPARATOR_MENU_ITEM(self))
    return;
  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv && priv->label);

  ptr = strchr(label, '%');
  if(ptr)
    g_object_set(G_OBJECT(self), "icon", ptr+1, NULL);
  text = ptr? g_strndup(label, ptr - label) : g_strdup(label);
  gtk_label_set_text_with_mnemonic(GTK_LABEL(priv->label), text);
  g_free(text);

  priv->flags |= MI_LABEL;
}

void menu_item_set_label_expr ( GtkWidget *self, GBytes *code )
{
  MenuItemPrivate *priv;

  if(GTK_IS_SEPARATOR_MENU_ITEM(self))
    return;
  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv && priv->label);

  expr_update(&priv->label_expr, code);
}

void menu_item_set_icon ( GtkWidget *self, const gchar *icon )
{
  MenuItemPrivate *priv;

  if(GTK_IS_SEPARATOR_MENU_ITEM(self))
    return;
  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv && priv->icon);

  if(icon && *icon)
    scale_image_set_image(priv->icon, icon, NULL);
  css_set_class(priv->icon, "hidden", !icon || !*icon);
  priv->flags |= MI_ICON;
}

void menu_item_set_tooltip ( GtkWidget *self, GBytes *code )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  g_clear_pointer(&priv->tooltip_expr, expr_cache_unref);
  priv->tooltip_expr = expr_cache_new_with_code(code);
  priv->flags |= MI_TOOLTIP;
}

void menu_item_set_action ( GtkWidget *self, GBytes *action )
{
  MenuItemPrivate *priv;

  if(GTK_IS_SEPARATOR_MENU_ITEM(self))
    return;
  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  g_bytes_unref(priv->action);
  priv->action = g_bytes_ref(action);
}

void menu_item_set_sort_index ( GtkWidget *self, gint index )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  if(!priv)
    g_message("%d", GTK_IS_SEPARATOR_MENU_ITEM(self));
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

  if(GTK_IS_SEPARATOR_MENU_ITEM(self))
    return;
  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  if( (submenu = menu_new(subname)) )
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(self), submenu);
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

  self = menu_item_new();
  menu_item_set_id(self, id);

  return self;
}

gboolean menu_item_remove ( gchar *id )
{
  GtkWidget *item;

  if(!menu_items || !(item = g_hash_table_lookup(menu_items, id)) )
    return FALSE;

  if(gtk_menu_item_get_submenu(GTK_MENU_ITEM(item)))
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), NULL);

  gtk_widget_destroy(item);
  return FALSE;
}

void menu_item_update ( GtkWidget *self )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  scanner_invalidate();
  if(vm_expr_eval(priv->label_expr))
    menu_item_set_label(self, priv->label_expr->cache);
  if(vm_expr_eval(priv->style_expr))
    gtk_widget_set_name(self, priv->style_expr->cache);
  if(vm_expr_eval(priv->tooltip_expr))
    gtk_widget_set_tooltip_text(self, priv->tooltip_expr->cache);
}

void menu_item_set_store ( GtkWidget *self, vm_store_t *store )
{
  MenuItemPrivate *priv;

  priv = g_object_get_data(G_OBJECT(self), "menu_item_private");
  g_return_if_fail(priv);

  if(priv->label_expr && !priv->label_expr->store)
    priv->label_expr->store = store;

  if(priv->style_expr && !priv->style_expr->store)
    priv->style_expr->store = store;

  if(priv->tooltip_expr && !priv->tooltip_expr->store)
    priv->tooltip_expr->store = store;
}

void menu_item_insert ( GtkWidget *menu, GtkWidget *item )
{
  GList *list, *iter;
  gint count = 0;

  if(gtk_widget_get_parent(item))
  {
    if(gtk_widget_get_parent(item) != menu)
      g_warning("menu item added to multiple menues");
    return;
  }

  if(!GTK_IS_MENU_ITEM(item) || !menu_item_get_sort_index(item))
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

