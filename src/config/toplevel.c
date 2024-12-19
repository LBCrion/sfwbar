/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config.h"
#include "scanner.h"
#include "module.h"
#include "gui/bar.h"
#include "gui/menu.h"
#include "vm/vm.h"

GBytes *parser_action_compat ( gchar *action, gchar *expr1, gchar *expr2,
    guint16 cond, guint16 ncond );
gboolean config_action ( GScanner *scanner, action_t **action_dst )
{
  GBytes *code;
  action_t *action;

  if( !(code = parser_action_compile(scanner)) )
  {
    *action_dst = NULL;
    return FALSE;
  }

  action = action_new();
  action->code = code;
  *action_dst = action;

  return TRUE;
}

GtkWidget *config_menu_item ( GScanner *scanner )
{
  gchar *label, *id;
  action_t *action;
  GtkWidget *item;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "missing '(' after 'item'",
      SEQ_REQ, G_TOKEN_STRING, NULL, &label, "missing label in 'item'",
      SEQ_REQ, ',', NULL, NULL, "missing ',' in 'item'",
      SEQ_REQ,  -2, config_action, &action, NULL,
      SEQ_OPT, ',', NULL, NULL, NULL,
      SEQ_CON, G_TOKEN_STRING, NULL, &id, "missing id in 'item'",
      SEQ_REQ, ')', NULL, NULL, "missing ')' after 'item'",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors && label && action)
    item = menu_item_new(label, action, id);
  else
    item = NULL;

  g_free(label);
  g_free(id);

  return item;
}

GtkWidget *config_submenu ( GScanner *scanner )
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
    item = menu_item_new(itemname, NULL, subid);
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
    switch(config_lookup_key(scanner, config_menu_keys))
    {
      case G_TOKEN_ITEM:
        item = config_menu_item(scanner);
        break;
      case G_TOKEN_SEPARATOR:
        item = gtk_separator_menu_item_new();
        config_check_and_consume(scanner, ';');
        break;
      case G_TOKEN_SUBMENU:
        item = config_submenu(scanner);
        break;
      default:
        item = NULL;
        g_scanner_error(scanner,
            "Unexpected token in menu. Expecting a menu item");
        break;
    }
    if(item)
      gtk_container_add(GTK_CONTAINER(menu), item);
  }
}

void config_menu ( GScanner *scanner )
{
  gchar *name = NULL;
  GtkWidget *menu;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "missing '(' after 'menu'",
      SEQ_REQ, G_TOKEN_STRING, NULL, &name, "missing menu name",
      SEQ_REQ, ')', NULL, NULL, "missing ')' after 'menu'",
      SEQ_REQ, '{', NULL, NULL, "missing '{' after 'menu'",
      SEQ_END);

  if(!scanner->max_parse_errors && name)
  {
    menu = menu_new(name);
    config_menu_items(scanner, menu);
  }

  g_free(name);
  config_check_and_consume(scanner, ';');
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

void config_function ( GScanner *scanner )
{
  gchar *name;
  action_t *action;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "missing '(' after 'function'",
      SEQ_REQ, G_TOKEN_STRING, NULL ,&name, "missing function name",
      SEQ_REQ, ')', NULL, NULL, "missing ')' after 'function'",
      SEQ_OPT, '{', NULL, NULL, "missing '{' after 'function'",
      SEQ_END);

  if(!scanner->max_parse_errors)
    while(!config_is_section_end(scanner))
    {
      if(config_action(scanner, &action))
        action_function_add(name, action);
      else
        g_scanner_error(scanner, "invalid action");
    }

  g_free(name);
}

void config_set ( GScanner *scanner )
{
  gchar *ident;
  gchar *value;

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_IDENTIFIER, NULL, &ident,
        "Missing identifier after 'set'",
      SEQ_REQ, '=', NULL, NULL, "Missing '=' after 'set'",
      SEQ_REQ, G_TOKEN_VALUE, NULL, &value, "Missing value after 'set'",
      SEQ_END);

  if(!scanner->max_parse_errors && ident && value)
    scanner_var_new(ident, NULL, value, G_TOKEN_SET, VT_FIRST);

  g_free(ident);
  g_free(value);
}

void config_mappid_map ( GScanner *scanner )
{
  gchar *pattern = NULL, *appid = NULL;

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_STRING, NULL, &pattern, "missing pattern in MapAppId",
      SEQ_REQ, ',', NULL, NULL, "missing comma after pattern in MapAppId",
      SEQ_REQ, G_TOKEN_STRING, NULL, &appid, "missing app_id in MapAppId",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors)
    wintree_appid_map_add(pattern, appid);

  g_free(pattern);
  g_free(appid);
}

void config_trigger_action ( GScanner *scanner )
{
  gchar *trigger;
  action_t *action;

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_STRING, NULL, &trigger,
        "missing trigger in TriggerAction",
      SEQ_REQ, ',', NULL, NULL, "missing ',' in TriggerAction",
      SEQ_REQ, -2, config_action, &action, NULL,
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

if(!scanner->max_parse_errors)
  action_trigger_add(action, trigger);
}

void config_module ( GScanner *scanner )
{
  gchar *name;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "missing '(' after 'module'",
      SEQ_REQ, G_TOKEN_STRING, NULL, &name, "missing module name",
      SEQ_REQ, ')', NULL, NULL, "missing ')' after 'module'",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors && name)
    module_load ( name );

  g_free(name);
}

GtkWidget *config_parse_toplevel ( GScanner *scanner, GtkWidget *container )
{
  scanner->user_data = NULL;

  while(g_scanner_peek_next_token(scanner) != G_TOKEN_EOF)
  {
    g_scanner_get_next_token(scanner);
    if(config_widget_child(scanner, NULL))
      continue;
    if(config_scanner_source(scanner))
      continue;
    switch(config_lookup_key(scanner, config_toplevel_keys))
    {
      case G_TOKEN_SCANNER:
        config_scanner(scanner);
        break;
      case G_TOKEN_LAYOUT:
        config_layout(scanner, container);
        break;
      case G_TOKEN_POPUP:
        config_popup(scanner);
        break;
      case G_TOKEN_PLACER:
        config_placer(scanner);
        break;
      case G_TOKEN_SWITCHER:
        config_switcher(scanner);
        break;
      case G_TOKEN_MENU:
        config_menu(scanner);
        break;
      case G_TOKEN_MENUCLEAR:
        config_menu_clear(scanner);
        break;
      case G_TOKEN_INCLUDE:
        config_include(scanner, NULL);
        break;
      case G_TOKEN_DEFINE:
        parser_macro_add(scanner);
        break;
      case G_TOKEN_SET:
        config_set(scanner);
        break;
      case G_TOKEN_TRIGGERACTION:
        config_trigger_action(scanner);
        break;
      case G_TOKEN_THEME:
        bar_set_theme(config_assign_string(scanner,"theme"));
        break;
      case G_TOKEN_ICON_THEME:
        bar_set_icon_theme(config_assign_string(scanner,"icon theme"));
        break;
      case G_TOKEN_MAPAPPID:
        config_mappid_map(scanner);
        break;
      case G_TOKEN_FILTERAPPID:
        if(!config_expect_token(scanner, G_TOKEN_STRING,
          "Missing <string> after FilterAppId"))
          break;
        wintree_filter_appid(scanner->value.v_string);
        break;
      case G_TOKEN_FILTERTITLE:
        if(!config_expect_token(scanner, G_TOKEN_STRING,
          "Missing <string> after FilterTitle"))
          break;
        wintree_filter_title(scanner->value.v_string);
        break;
      case G_TOKEN_FUNCTION:
        config_function(scanner);
        break;
      case G_TOKEN_MODULE:
        config_module(scanner);
        break;
      case G_TOKEN_DISOWNMINIMIZED:
        wintree_set_disown(config_assign_boolean(scanner, FALSE,
              "DisownMinimized"));
        break;
      default:
        g_scanner_error(scanner,"Unexpected toplevel token");
        break;
    }
  }
  return scanner->user_data;
}
