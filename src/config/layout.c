/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config.h"
#include "gui/css.h"
#include "gui/bar.h"
#include "gui/grid.h"
#include "gui/popup.h"
#include "vm/vm.h"

GdkRectangle *config_get_loc ( GScanner *scanner )
{
  GdkRectangle rect = { .x=0, .y=0, .width=1, .height=1 };

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "missing '(' after loc",
      SEQ_REQ, G_TOKEN_INT, NULL, &rect.x, "missing x value in loc",
      SEQ_REQ, ',', NULL, NULL, "missing comma after x value in loc",
      SEQ_REQ, G_TOKEN_INT, NULL, &rect.y, "missing y value in loc",
      SEQ_OPT, ',', NULL, NULL, NULL,
      SEQ_CON, G_TOKEN_INT, NULL, &rect.width, "missing w value in loc",
      SEQ_OPT, ',', NULL, NULL, NULL,
      SEQ_CON, G_TOKEN_INT, NULL, &rect.height, "missing h value in loc",
      SEQ_REQ, ')', NULL, NULL, "missing ')' in loc statement",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END );

  return g_memdup2(&rect, sizeof(rect));
}

static gboolean config_action_mods ( GScanner *scanner, gint *mods )
{
  gpointer mod;

  while( (mod = config_lookup_next_ptr(scanner, config_mods)) )
  {
    g_scanner_get_next_token(scanner);
    *mods |= GPOINTER_TO_INT(mod);

    if(!config_check_and_consume(scanner, '+'))
      return FALSE;
  }
  return TRUE;
}

static gboolean config_action_slot ( GScanner *scanner, gint *slot )
{
  gint token;

  g_scanner_get_next_token(scanner);
  if(scanner->token == G_TOKEN_FLOAT && scanner->value.v_float >= 0 &&
      scanner->value.v_float < BASE_WIDGET_ACTION_LAST)
    *slot = scanner->value.v_float;
  else if( (token = config_lookup_key(scanner, config_events)) )
    *slot = token;
  else
    return FALSE;

  return !(*slot<0 || *slot>BASE_WIDGET_ACTION_LAST );
}

static GPtrArray *config_action_attachment ( GScanner *scanner )
{
  gint slot = 1, mod = 0;
  GPtrArray *attach;
  GBytes *action = NULL;

  config_parse_sequence(scanner,
      SEQ_OPT, '[', NULL, NULL, NULL,
      SEQ_CON, -2, config_action_mods, &mod, NULL,
      SEQ_CON, -2, config_action_slot, &slot, "invalid action slot",
      SEQ_CON, ']', NULL, NULL, "missing ']' after action",
      SEQ_REQ, '=', NULL, NULL, "missing '=' after action",
      SEQ_REQ, -2, config_action, &action, "missing action",
      SEQ_END);

  if(!scanner->max_parse_errors && action)
  {
    attach = base_widget_attachment_new_array(action, slot, mod);
    g_bytes_unref(action);
    return attach;
  }

  if(action)
    g_bytes_unref(action);

  return NULL;
}

GtkWidget *config_include ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;
  gchar *fname;

  config_parse_sequence(scanner,
      SEQ_OPT, '(', NULL, NULL, NULL,
      SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing filename in include",
      SEQ_OPT, ')', NULL, NULL, NULL,
      SEQ_END);

  if(scanner->max_parse_errors)
    widget = NULL;
  else
  {
    widget = config_parse(fname, container, SCANNER_STORE(scanner));
    if(container && widget && !gtk_widget_get_parent(widget))
      g_object_ref_sink(widget);
  }

  g_free(fname);
  return widget;
}

gboolean config_widget_set_property ( GScanner *scanner, gchar *prefix,
    GtkWidget *widget )
{
  GParamSpec *spec;
  GEnumValue *eval;
  GValue value = G_VALUE_INIT;
  GString *prop_name;
  const gchar *name, *blurb;

  if(!widget || scanner->token != G_TOKEN_IDENTIFIER ||
      g_scanner_peek_next_token(scanner) == G_TOKEN_IDENTIFIER)
   return FALSE; 

  prop_name = g_string_ascii_down(g_string_append(g_string_new(prefix),
        scanner->value.v_identifier));
  spec = g_object_class_find_property(G_OBJECT_GET_CLASS(widget),
      prop_name->str);
  g_string_free(prop_name, TRUE);

  if(!spec)
    return FALSE;

  blurb = g_param_spec_get_blurb(spec);
  if(g_ascii_strncasecmp(blurb, "sfwbar_config", 13))
    return FALSE;
  blurb = blurb[13]==':'? blurb+14 : blurb+13;

  name = g_param_spec_get_name(spec);
  g_value_init(&value, G_PARAM_SPEC_VALUE_TYPE(spec));

  if(G_IS_PARAM_SPEC_ENUM(spec) && (eval = g_enum_get_value_by_name(
          G_PARAM_SPEC_ENUM(spec)->enum_class,
          config_assign_enum(scanner, name))))
    g_value_set_enum(&value, eval->value);
  else if(G_IS_PARAM_SPEC_INT(spec))
    g_value_set_int(&value, config_assign_number(scanner, name));
  else if(G_IS_PARAM_SPEC_INT64(spec))
    g_value_set_int64(&value, config_assign_number(scanner, name));
  else if(G_IS_PARAM_SPEC_BOOLEAN(spec))
    g_value_set_boolean(&value, config_assign_boolean(scanner, FALSE, name));
  else if(G_IS_PARAM_SPEC_STRING(spec))
    g_value_take_string(&value, config_assign_string(scanner, name));
  else if(G_PARAM_SPEC_VALUE_TYPE(spec) == G_TYPE_PTR_ARRAY && *blurb=='b')
    g_value_take_boxed(&value, config_action_attachment(scanner));
  else if(G_PARAM_SPEC_VALUE_TYPE(spec) == G_TYPE_PTR_ARRAY)
    g_value_set_boxed(&value, config_assign_string_list(scanner));
  else if(G_PARAM_SPEC_VALUE_TYPE(spec) == G_TYPE_BYTES && *blurb=='a')
    g_value_take_boxed(&value, config_assign_action(scanner, name));
  else if(G_PARAM_SPEC_VALUE_TYPE(spec) == G_TYPE_BYTES)
    g_value_take_boxed(&value, config_assign_expr(scanner, name));
  else if(G_IS_PARAM_SPEC_OBJECT(spec) && *blurb == 's')
    g_value_take_object(&value, G_OBJECT(config_menu(scanner)));
  else if(G_PARAM_SPEC_VALUE_TYPE(spec) == GDK_TYPE_RECTANGLE)
    g_value_take_boxed(&value, config_get_loc(scanner));
  else
    return FALSE;

  g_object_set_property(G_OBJECT(widget), g_param_spec_get_name(spec), &value);
  g_value_unset(&value);
  return TRUE;
}

static gboolean config_widget_variable ( GScanner *scanner, GtkWidget *widget )
{
  vm_var_t *var;
  GBytes *code;

  if(scanner->token != G_TOKEN_IDENTIFIER)
    return FALSE;

  if( !(var = vm_store_lookup_string(base_widget_get_store(widget),
          scanner->value.v_identifier)) )
    return FALSE;
  if( !(code = config_assign_expr(scanner,
          (gchar *)g_quark_to_string(var->quark))) )
    return FALSE;

  var->value = vm_code_eval(code, widget);
  g_bytes_unref(code);

  return TRUE;
}

gboolean config_widget_property ( GScanner *scanner, GtkWidget *widget )
{
  if(config_widget_variable(scanner, widget))
    return TRUE;

  if(config_widget_set_property(scanner, NULL, widget))
    return TRUE;

  if(scanner->token == G_TOKEN_IDENTIFIER &&
      !g_ascii_strcasecmp(scanner->value.v_identifier, "group"))
  {
    g_scanner_get_next_token(scanner);
    if(config_widget_set_property(scanner, "group_" , widget))
      return TRUE;
  }

  if(GTK_IS_BOX(gtk_widget_get_parent(widget)) ||
      GTK_IS_WINDOW(gtk_widget_get_parent(widget)))
    if(config_widget_set_property(scanner, NULL,
          gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW)))
        return TRUE;

  return FALSE;
}

static GtkWidget *config_widget_get ( GScanner *scanner,
    GtkWidget *container, GType (*type_get)(void) )
{
  GtkWidget *widget, *parent;
  gchar *name;

  name = config_check_and_consume(scanner, G_TOKEN_STRING)?
    scanner->value.v_string : NULL;
  widget = name? vm_store_widget_lookup(SCANNER_STORE(scanner), name) : NULL;

  if(widget && !G_TYPE_CHECK_INSTANCE_TYPE((widget), type_get()))
  {
    g_scanner_error(scanner, "Widget id collision: type mismatch.");
    g_scanner_error(scanner, "Widget '%s' has been previously defined as '%s'",
        name, G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(widget)));
    return NULL;
  }
  else if(widget)
  {
    parent = gtk_widget_get_parent(widget);
    g_debug("Widget lookup: '%s' -> %p, parent: %p, old: %p", name, widget,
        container, gtk_widget_get_parent(parent));
    if(container && parent && gtk_widget_get_parent(parent) != container)
    {
      grid_detach(g_object_ref(widget), gtk_widget_get_parent(parent));
      gtk_container_remove(GTK_CONTAINER(parent), widget);
    }
  }
  else if(type_get != base_widget_get_type)
  {
    widget = GTK_WIDGET(g_object_new(type_get(), NULL));
    g_object_ref_sink(widget);
    if(name)
      g_object_set(G_OBJECT(widget), "id", name, NULL);
  }

  return widget;
}

GtkWidget *config_widget_build ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;
  gboolean disabled;
  GType (*type_get)(void);

  if(scanner->token != G_TOKEN_IDENTIFIER)
    return FALSE;
  if(!g_ascii_strcasecmp(scanner->value.v_identifier, "include") ||
     !g_ascii_strcasecmp(scanner->value.v_identifier, "widget") )
    widget = config_include(scanner, container);
  else if( (type_get = config_lookup_ptr(scanner, config_widget_keys)) )
    widget = config_widget_get(scanner, container, type_get);
  else
    return NULL;

  if(!widget)
  {
    if(g_scanner_peek_next_token(scanner)=='{')
      config_skip_statement(scanner);
    return NULL;
  }

  config_widget(scanner, widget);
  css_widget_cascade(widget, NULL);

  g_object_get(G_OBJECT(widget), "disable", &disabled, NULL);
  if(disabled)
    g_clear_pointer(&widget, gtk_widget_destroy);

  return widget;
}

gboolean config_widget_child ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;

  if(container && !IS_GRID(container))
  {
    g_scanner_error(scanner, "invalid container in widget declaration");
    return FALSE;
  }

  if( !(widget = config_widget_build(scanner, container)) )
    return FALSE;

  if(container && !gtk_widget_get_parent(widget))
  {
    grid_attach(container, widget);
    grid_mirror_child(container, widget);
    g_object_unref(widget);
  }

  return TRUE;
}

void config_widget ( GScanner *scanner, GtkWidget *widget )
{
  if(!base_widget_get_store(widget))
    g_object_set(G_OBJECT(widget), "store", SCANNER_STORE(scanner), NULL);
  if(!config_check_and_consume(scanner, '{'))
    return;
  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    if(config_widget_property(scanner, widget))
      continue;
    if(config_widget_child(scanner, widget))
      continue;

    g_scanner_error(scanner, "Invalid property in a widget declaration");
    config_skip_statement(scanner);
  }
  if(base_widget_check_action_slot(widget, BASE_WIDGET_ACTION_CONFIGURE))
    g_idle_add((GSourceFunc)base_widget_configure, widget);
  scanner->max_parse_errors = FALSE;
}

GtkWidget *config_layout ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *layout;

  scanner->max_parse_errors = FALSE;
  if(container)
    layout = grid_new();
  else
    layout = bar_grid_from_name(
        config_check_and_consume(scanner, G_TOKEN_STRING)?
        scanner->value.v_string : NULL);

  config_widget(scanner, layout);

  return layout;
}

void config_popup ( GScanner *scanner )
{
  gchar *id;

  config_parse_sequence(scanner,
      SEQ_OPT, '(', NULL, NULL, NULL,
      SEQ_REQ, G_TOKEN_STRING, NULL, &id, "Missing PopUp id",
      SEQ_OPT, ')', NULL, NULL, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors && id)
    config_widget(scanner, gtk_bin_get_child(GTK_BIN(popup_new(id))));

  g_free(id);
}
