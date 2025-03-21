/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config.h"
#include "scanner.h"
#include "module.h"
#include "gui/bar.h"

static void config_set ( GScanner *scanner )
{
  GBytes *code;
  gchar *ident;

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_IDENTIFIER, NULL, &ident,
        "Missing identifier after 'set'",
      SEQ_REQ, '=', NULL, NULL, "Missing '=' after 'set'",
      SEQ_END);

  if(ident && config_expr(scanner, &code))
    scanner_var_new(ident, NULL, (gchar *)code, G_TOKEN_SET, VT_FIRST);

  g_free(ident);
}

static void config_mappid_map ( GScanner *scanner )
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

static void config_trigger_action ( GScanner *scanner )
{
  gchar *trigger;
  GBytes *action;

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_STRING, NULL, &trigger,
        "missing trigger in TriggerAction",
      SEQ_REQ, ',', NULL, NULL, "missing ',' in TriggerAction",
      SEQ_REQ, -2, config_action, &action, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors)
    action_trigger_add(trigger,
        vm_closure_new(action, SCANNER_HEAP(scanner)));
}

static void config_module ( GScanner *scanner )
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

static void config_vars ( GScanner *scanner, gboolean private )
{
  vm_store_t *store;

  if(private)
  {

    if(!SCANNER_DATA(scanner)->heap)
      SCANNER_DATA(scanner)->heap = vm_store_new(SCANNER_DATA(scanner)->heap_global);
    store = SCANNER_DATA(scanner)->heap;
  }
  else
    store = SCANNER_DATA(scanner)->heap_global;

  do {
    if(!config_expect_token(scanner, G_TOKEN_IDENTIFIER,
          "expect an identifier in var declaration"))
      return;

    if(!vm_store_lookup_string(store, scanner->value.v_identifier))
      vm_store_insert(store, vm_var_new(scanner->value.v_identifier));
    else
      g_message("duplicate declaration of variable %s",
          scanner->value.v_identifier);

  } while(config_check_and_consume(scanner, ','));
  config_check_and_consume(scanner, ';');
}

GtkWidget *config_parse_toplevel ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *layout = NULL;

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
        layout = config_layout(scanner, container);
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
      case G_TOKEN_PRIVATE:
        config_vars(scanner, TRUE);
        break;
      case G_TOKEN_VAR:
        config_vars(scanner, FALSE);
        break;
      case G_TOKEN_TRIGGERACTION:
        config_trigger_action(scanner);
        break;
      case G_TOKEN_THEME:
        bar_set_theme(config_assign_string(scanner, "theme"));
        break;
      case G_TOKEN_ICON_THEME:
        bar_set_icon_theme(config_assign_string(scanner, "icon theme"));
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
        parser_function_parse(scanner);
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
  return layout;
}
