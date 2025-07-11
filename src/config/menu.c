/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config/config.h"
#include "gui/menu.h"
#include "gui/menuitem.h"

static gboolean config_menu_item_props ( GScanner *scanner, GtkWidget *item )
{
  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    if(!config_widget_set_property(scanner, NULL, item))
      g_scanner_error(scanner, "unsupported property in menu item declaration");
    config_check_and_consume(scanner, ';');
  }
  return !scanner->max_parse_errors;
}

static GtkWidget *config_menu_item ( GScanner *scanner )
{
  gchar *id;
  GBytes *action, *label;
  GtkWidget *item;

  if(config_check_and_consume(scanner, '('))
  {
    config_parse_sequence(scanner,
        SEQ_REQ, -2, config_expr, &label, "invalid title expression in item",
        SEQ_REQ, ',', NULL, NULL, "missing ',' in 'item'",
        SEQ_REQ, -2, config_action, &action, "invalid action in item",
        SEQ_OPT, ',', NULL, NULL, NULL,
        SEQ_CON, G_TOKEN_STRING, NULL, &id, "missing id in 'item'",
        SEQ_REQ, ')', NULL, NULL, "missing ')' after 'item'",
        SEQ_END);

    if(!scanner->max_parse_errors && label && action)
    {
      item = menu_item_get(id, TRUE);
      menu_item_set_label_expr(item, label);
      menu_item_set_action(item, action);
      g_bytes_unref(label);
      g_bytes_unref(action);
    }
    else
    {
      item = NULL;
      if(label)
        g_bytes_unref(label);
      if(action)
        g_bytes_unref(action);
    }
    g_free(id);
    return item;
  }

  id = config_check_and_consume(scanner, G_TOKEN_STRING)?
    g_strdup(scanner->value.v_string) : NULL;
  item = menu_item_get(id, TRUE);
  g_free(id);

  if(config_check_and_consume(scanner, '{'))
    if(!config_menu_item_props(scanner, item))
      g_clear_pointer(&item, gtk_widget_destroy);

  return item;
}

static GtkWidget *config_submenu ( GScanner *scanner )
{
  GtkWidget *item, *submenu;
  gchar *itemname, *subname, *subid;
  gboolean items;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL,"missing '(' after 'submenu'",
      SEQ_REQ, G_TOKEN_STRING, NULL, &itemname, "missing submenu title",
      SEQ_OPT, ',', NULL, NULL, NULL,
      SEQ_CON, G_TOKEN_STRING, NULL, &subname, "missing submenu name",
      SEQ_OPT, ',', NULL, NULL, NULL,
      SEQ_CON, G_TOKEN_STRING, NULL, &subid, "missing submenu id",
      SEQ_REQ, ')', NULL, NULL,"missing ')' after 'submenu'",
      SEQ_OPT, '{', NULL, &items, "missing '{' after 'submenu'",
      SEQ_END);

  if(!scanner->max_parse_errors && itemname)
  {
    item = menu_item_get(subid, TRUE);
    menu_item_set_label(item, itemname);
    submenu = menu_new(subname);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
    if(items)
      config_menu_items(scanner, submenu);
  }
  else
    item = NULL;

  g_free(itemname);
  g_free(subname);

  return item;
}

void config_menu_items ( GScanner *scanner, GtkWidget *menu )
{
  GtkWidget *item;

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    item = NULL;
    switch(config_lookup_key(scanner, config_menu_keys))
    {
      case G_TOKEN_ITEM:
        item = config_menu_item(scanner);
        break;
      case G_TOKEN_SEPARATOR:
        item = menu_item_separator_new();
        break;
      case G_TOKEN_SUBMENU:
        item = config_submenu(scanner);
        break;
      case G_TOKEN_SORT:
        g_object_set_data(G_OBJECT(menu), "sort",
            GINT_TO_POINTER(config_assign_boolean(scanner, FALSE, "sort")));
        break;
      default:
        item = NULL;
        g_scanner_error(scanner,
            "Unexpected token in menu. Expecting a menu item");
        break;
    }
    config_check_and_consume(scanner, ';');
    if(item)
    {
      if(g_object_get_data(G_OBJECT(menu), "sort"))
        menu_item_insert(menu, item);
      else
        gtk_container_add(GTK_CONTAINER(menu), item);
    }
  }
}

GtkWidget *config_menu ( GScanner *scanner )
{
  GtkWidget *menu = NULL;
  gchar *name = NULL;

  config_parse_sequence(scanner,
      SEQ_OPT, '(', NULL, NULL, "missing '(' after 'menu'",
      SEQ_OPT, G_TOKEN_STRING, NULL, &name, "missing menu name",
      SEQ_OPT, ')', NULL, NULL, "missing ')' after 'menu'",
      SEQ_OPT, '{', NULL, NULL, "missing '{' after 'menu'",
      SEQ_END);

  if(scanner->max_parse_errors)
    return NULL;
  menu = menu_new(name);
  g_free(name);
  config_menu_items(scanner, menu);
  config_check_and_consume(scanner, ';');
  return menu;
}

void config_menu_clear ( GScanner *scanner )
{
  gchar *name = NULL;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "missing '(' after 'menu'",
      SEQ_REQ, G_TOKEN_STRING, NULL, &name, "missing menu name",
      SEQ_REQ, ')', NULL, NULL, "missing ')' after 'menu'",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors && name)
    menu_remove(name);

  g_free(name);
}
