/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config.h"
#include "module.h"
#include "scanner.h"
#include "trigger.h"
#include "gui/bar.h"
#include "gui/css.h"
#include "gui/switcher.h"

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

  config_check_and_consume(scanner, ';');
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
    trigger_add(trigger, (trigger_func_t)trigger_action_cb,
        vm_closure_new(action, SCANNER_STORE(scanner)));
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

static GtkWidget *config_private ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;
  vm_store_t *store;

  if(!config_expect_token(scanner, '{', "Missing '{' after Private"))
    return NULL;

  store = SCANNER_STORE(scanner);
  SCANNER_STORE(scanner) = vm_store_new(store, FALSE);
  widget = config_parse_toplevel(scanner, container);
  SCANNER_STORE(scanner) = store;
  
  return widget;
}

static void config_vars ( GScanner *scanner, gboolean private )
{
  vm_var_t *var;
  GBytes *code;

  do {
    if(!config_expect_token(scanner, G_TOKEN_IDENTIFIER,
          "expect an identifier in var declaration"))
      return;

    if(!vm_store_lookup_string(SCANNER_STORE(scanner),
          scanner->value.v_identifier))
    {
      var = vm_var_new(scanner->value.v_identifier);
      vm_store_insert(SCANNER_STORE(scanner), var);
      if(config_check_and_consume(scanner, '=') && config_expr(scanner, &code))
      {
        var->value = vm_code_eval(code, NULL);
        g_bytes_unref(code);
      }
    }
    else
      g_scanner_error(scanner, "duplicate declaration of variable '%s'",
          scanner->value.v_identifier);

  } while(config_check_and_consume(scanner, ','));
  config_check_and_consume(scanner, ';');
}

static void config_switcher ( GScanner *scanner )
{
  GtkWidget *widget;
  gboolean disable = FALSE;

  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '{', "Missing '{' after 'switcher'"))
    return;

  widget = switcher_new();

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    if(!config_widget_set_property(scanner, NULL, widget))
        g_scanner_error(scanner, "Unexpected token in 'switcher'");
  }

  g_object_get(G_OBJECT(widget), "disable", &disable, NULL);
  if(disable)
    gtk_widget_destroy(widget);
}

static void config_placer ( GScanner *scanner )
{
  gint wp_x = 10;
  gint wp_y = 10;
  gint wo_x = 0;
  gint wo_y = 0;
  gboolean pid = FALSE, disable = FALSE;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '{', "Missing '{' after 'placer'"))
    return;

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    switch (config_lookup_key(scanner, config_placer_keys))
    {
      case G_TOKEN_XSTEP: 
        wp_x = config_assign_number (scanner, "xstep");
        break;
      case G_TOKEN_YSTEP: 
        wp_y = config_assign_number (scanner, "ystep");
        break;
      case G_TOKEN_XORIGIN: 
        wo_x = config_assign_number (scanner, "xorigin");
        break;
      case G_TOKEN_YORIGIN: 
        wo_y = config_assign_number (scanner, "yorigin");
        break;
      case G_TOKEN_CHILDREN:
        pid = config_assign_boolean(scanner, FALSE, "children");
        break;
      case G_TOKEN_DISABLE:
        disable = config_assign_boolean(scanner, FALSE, "disable");
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'placer'");
        break;
    }
  }

  if(!disable)
    wintree_placer_conf(wp_x, wp_y, wo_x, wo_y, pid);
}

GtkWidget *config_parse_toplevel ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *layout = NULL;

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
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
        layout = config_private(scanner, container);
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
        if(config_widget_child(scanner, NULL))
          break;
        g_scanner_error(scanner,"Unexpected toplevel token");
        break;
    }
  }
  return layout;
}
