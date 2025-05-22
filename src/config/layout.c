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

void config_widget (GScanner *scanner, GtkWidget *widget);

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

gboolean config_action_mods ( GScanner *scanner, gint *mods )
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

gboolean config_action_slot ( GScanner *scanner, gint *slot )
{
  gint token;

  g_scanner_get_next_token(scanner);
  if(scanner->token == G_TOKEN_FLOAT && scanner->value.v_float >= 0 &&
      scanner->value.v_float <= BASE_WIDGET_MAX_ACTION)
    *slot = scanner->value.v_float;
  else if( (token = config_lookup_key(scanner, config_events)) )
    *slot = token;
  else
    return FALSE;

  return !(*slot<0 || *slot>BASE_WIDGET_MAX_ACTION );
}

static void config_widget_action ( GScanner *scanner, GtkWidget *widget )
{
  gint slot = 1, mod = 0;
  GBytes *action = NULL;

  config_parse_sequence(scanner,
      SEQ_OPT, '[', NULL, NULL, NULL,
      SEQ_CON, -2, config_action_mods, &mod, NULL,
      SEQ_CON, -2, config_action_slot, &slot, "invalid action slot",
      SEQ_CON, ']', NULL, NULL, "missing ']' after action",
      SEQ_REQ, '=', NULL, NULL, "missing '=' after action",
      SEQ_REQ, -2, config_action, &action, "missing action",
      SEQ_END);

  if(!scanner->max_parse_errors)
    base_widget_set_action(widget, slot, mod, action);
  else if(action)
    g_bytes_unref(action);
}

gboolean config_include ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;
  gchar *fname;

  if(scanner->token != G_TOKEN_IDENTIFIER ||
      (g_ascii_strcasecmp(scanner->value.v_identifier, "include") &&
      (!container || g_ascii_strcasecmp(scanner->value.v_identifier, "widget"))) )
    return FALSE;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "Missing '(' after include",
      SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing filename in include",
      SEQ_REQ, ')', NULL, NULL, "Missing ')',after include",
      SEQ_END);

  if(scanner->max_parse_errors)
  {
    g_free(fname);
    return TRUE;
  }

  widget = config_parse(fname, container, SCANNER_STORE(scanner));
  if(container)
  {
    config_widget(scanner, widget);
    grid_attach(container, widget);
    grid_mirror_child(container, widget);
    css_widget_cascade(widget, NULL);
  }
  g_free(fname);

  return TRUE;
}

gboolean config_widget_set_property ( GScanner *scanner, gchar *prefix,
    GtkWidget *widget )
{
  GParamSpec *spec;
  GEnumValue *eval;
  GValue value = G_VALUE_INIT;
  GString *prop_name;
  const gchar *name;

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

  if(g_strcmp0(g_param_spec_get_blurb(spec), "sfwbar_config"))
    return FALSE;

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
    g_value_set_string(&value, config_assign_string(scanner, name));
  else if(G_PARAM_SPEC_VALUE_TYPE(spec) == G_TYPE_PTR_ARRAY)
    g_value_set_boxed(&value, config_assign_string_list(scanner));
  else if(G_PARAM_SPEC_VALUE_TYPE(spec) == G_TYPE_BYTES)
    g_value_take_boxed(&value, config_assign_expr(scanner, name));
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
  GtkWindow *win;
  gint key;

  if(config_widget_variable(scanner, widget))
    return TRUE;

  key = config_lookup_key(scanner, config_prop_keys);

  if(IS_BASE_WIDGET(widget) && key == G_TOKEN_ACTION)
  {
    config_widget_action(scanner, widget);
    return TRUE;
  }

  win = GTK_WINDOW(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW));
  if(win && gtk_bin_get_child(GTK_BIN(win)) == widget &&
      gtk_window_get_window_type(win) == GTK_WINDOW_POPUP &&
      key == G_TOKEN_AUTOCLOSE)
  {
      popup_set_autoclose(GTK_WIDGET(win),
          config_assign_boolean(scanner, FALSE, "autoclose"));
      return TRUE;
  }

  if(config_widget_set_property(scanner, NULL, widget))
    return TRUE;

  if(key == G_TOKEN_GROUP)
  {
    g_scanner_get_next_token(scanner);
    if(config_widget_set_property(scanner, "group_" , widget))
      return TRUE;
  }

  if(GTK_IS_BOX(gtk_widget_get_parent(widget)))
    if(config_widget_set_property(scanner, NULL,
              gtk_widget_get_ancestor(widget, BAR_TYPE)))
      return TRUE;

  return FALSE;
}

GtkWidget *config_widget_find_existing ( GScanner *scanner,
    GtkWidget *container, GType (*get_type)(void) )
{
  GtkWidget *widget, *parent;

  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
    return NULL;

  if( !(widget = base_widget_from_id(SCANNER_STORE(scanner),
          scanner->next_value.v_string)) )
    return NULL;

  if(!G_TYPE_CHECK_INSTANCE_TYPE((widget), get_type()))
    return NULL;

  parent = gtk_widget_get_parent(widget);
  parent = parent? gtk_widget_get_parent(parent) : NULL;

  if(container && parent != container)
    return NULL;

  g_scanner_get_next_token(scanner);
  return widget;
}

gboolean config_widget_child ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;
  GType (*type_get)(void);
  gboolean disabled;

  if(container && !IS_GRID(container))
    return FALSE;

  if(config_include(scanner, container))
    return TRUE;

  if( !(type_get = config_lookup_ptr(scanner, config_widget_keys)) )
    return FALSE;

  scanner->max_parse_errors = FALSE;
  if( (widget = config_widget_find_existing(scanner, container, type_get)) )
    container = gtk_widget_get_ancestor(widget, GRID_TYPE);
  else
  {
    widget = GTK_WIDGET(g_object_new(type_get(), NULL));
    if(config_check_and_consume(scanner, G_TOKEN_STRING))
      g_object_set(G_OBJECT(widget), "id", scanner->value.v_string, NULL);
    if(container)
    {
      grid_attach(container, widget);
      grid_mirror_child(container, widget);
    }
    css_widget_cascade(widget, NULL);
  }

  config_widget(scanner, widget);

  g_object_get(G_OBJECT(widget), "disable", &disabled, NULL);
  if(disabled)
    gtk_widget_destroy(widget);
  else if(!container)
  {
    g_scanner_error(scanner, "orphan widget without a valid address: %s",
        base_widget_get_id(widget));
    gtk_widget_destroy(widget);
    return TRUE; /* we parsed successfully, but the address was wrong */
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
    if(scanner->token == ';')
      continue;
    if(config_widget_property(scanner, widget))
      continue;
    if(config_widget_child(scanner, widget))
      continue;

    g_scanner_error(scanner, "Invalid property in a widget declaration");
  }
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
