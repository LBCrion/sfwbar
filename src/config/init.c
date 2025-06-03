/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "scanner.h"
#include "config.h"
#include "gui/css.h"
#include "gui/taskbarshell.h"
#include "gui/pager.h"
#include "gui/tray.h"
#include "gui/grid.h"
#include "gui/label.h"
#include "gui/image.h"
#include "gui/scale.h"
#include "gui/button.h"
#include "gui/cchart.h"
#include "gui/taskbarpager.h"
#include "gui/taskbarpopup.h"
#include "util/file.h"
#include "util/string.h"

GHashTable *config_mods, *config_events, *config_var_types, *config_act_cond;
GHashTable *config_toplevel_keys, *config_menu_keys, *config_scanner_keys;
GHashTable *config_scanner_types, *config_scanner_flags, *config_filter_keys;
GHashTable *config_axis_keys, *config_taskbar_types, *config_widget_keys;
GHashTable *config_prop_keys, *config_placer_keys, *config_flowgrid_props;
GHashTable *config_menu_item_keys;

static GScannerConfig scanner_config = {
  .cset_skip_characters = (" \t\n\r"),
  .cset_identifier_first = (G_CSET_a_2_z G_CSET_A_2_Z "_$"),
  .cset_identifier_nth = (G_CSET_a_2_z G_CSET_A_2_Z "_01234566789."
      G_CSET_LATINS G_CSET_LATINC),
  .cpair_comment_single = ("#\n"),

  .case_sensitive = 0,
  .skip_comment_multi = 1,
  .skip_comment_single = 1,
  .scan_comment_multi = 1,

  .scan_identifier = 1,
  .scan_identifier_1char = 1,
  .scan_identifier_NULL = 0,
  .scan_symbols = 1,
  .scan_binary = 0,
  .scan_octal = 0,
  .scan_float = 1,
  .scan_hex = 1,
  .scan_hex_dollar = 0,
  .scan_string_sq = 1,
  .scan_string_dq = 1,
  .numbers_2_int = 1,
  .int_2_float = 1,
  .identifier_2_string = 0,
  .char_2_token = 1,
  .symbol_2_token = 1,
  .scope_0_fallback = 0,
  .store_int64 = 0,
};

void config_init ( void )
{
  config_mods = g_hash_table_new((GHashFunc)str_nhash, (GEqualFunc)str_nequal);
  config_add_key(config_mods, "shift", GDK_SHIFT_MASK);
  config_add_key(config_mods, "ctrl", GDK_CONTROL_MASK);
  config_add_key(config_mods, "super", GDK_SUPER_MASK);
  config_add_key(config_mods, "hyper", GDK_HYPER_MASK);
  config_add_key(config_mods, "meta", GDK_META_MASK);
  config_add_key(config_mods, "mod1", GDK_MOD1_MASK);
  config_add_key(config_mods, "mod2", GDK_MOD2_MASK);
  config_add_key(config_mods, "mod3", GDK_MOD3_MASK);
  config_add_key(config_mods, "mod4", GDK_MOD4_MASK);
  config_add_key(config_mods, "mod5", GDK_MOD5_MASK);

  config_events = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_events, "Init", 0);
  config_add_key(config_events, "LeftClick", 1);
  config_add_key(config_events, "MiddleClick", 2);
  config_add_key(config_events, "RightClick", 3);
  config_add_key(config_events, "ScrollUp", 4);
  config_add_key(config_events, "ScrollDown", 5);
  config_add_key(config_events, "ScrollLeft", 6);
  config_add_key(config_events, "ScrollRight", 7);
  config_add_key(config_events, "Drag", 8);

  config_var_types = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_var_types, "Sum", VT_SUM);
  config_add_key(config_var_types, "Product", VT_PROD);
  config_add_key(config_var_types, "Last", VT_LAST);
  config_add_key(config_var_types, "FIrst", VT_FIRST);

  config_act_cond = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_act_cond, "UserState", WS_USERSTATE);
  config_add_key(config_act_cond, "UserState2", WS_USERSTATE2);
  config_add_key(config_act_cond, "Minimized", WS_MINIMIZED);
  config_add_key(config_act_cond, "Maximized", WS_MAXIMIZED);
  config_add_key(config_act_cond, "FullScreen", WS_FULLSCREEN);
  config_add_key(config_act_cond, "Focused", WS_FOCUSED);
  config_add_key(config_act_cond, "Children", WS_CHILDREN);

  config_toplevel_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_toplevel_keys, "Scanner", G_TOKEN_SCANNER);
  config_add_key(config_toplevel_keys, "Layout", G_TOKEN_LAYOUT);
  config_add_key(config_toplevel_keys, "Bar", G_TOKEN_LAYOUT);
  config_add_key(config_toplevel_keys, "Widget_grid", G_TOKEN_LAYOUT);
  config_add_key(config_toplevel_keys, "PopUp", G_TOKEN_POPUP);
  config_add_key(config_toplevel_keys, "Placer", G_TOKEN_PLACER);
  config_add_key(config_toplevel_keys, "Switcher", G_TOKEN_SWITCHER);
  config_add_key(config_toplevel_keys, "Define", G_TOKEN_DEFINE);
  config_add_key(config_toplevel_keys, "TriggerAction", G_TOKEN_TRIGGERACTION);
  config_add_key(config_toplevel_keys, "MapAppId", G_TOKEN_MAPAPPID);
  config_add_key(config_toplevel_keys, "FilterAppId", G_TOKEN_FILTERAPPID);
  config_add_key(config_toplevel_keys, "FilterTitle", G_TOKEN_FILTERTITLE);
  config_add_key(config_toplevel_keys, "Module", G_TOKEN_MODULE);
  config_add_key(config_toplevel_keys, "Theme", G_TOKEN_THEME);
  config_add_key(config_toplevel_keys, "IconTheme", G_TOKEN_ICON_THEME);
  config_add_key(config_toplevel_keys, "DisownMinimized",
      G_TOKEN_DISOWNMINIMIZED);
  config_add_key(config_toplevel_keys, "Function", G_TOKEN_FUNCTION);
  config_add_key(config_toplevel_keys, "Var", G_TOKEN_VAR);
  config_add_key(config_toplevel_keys, "Private", G_TOKEN_PRIVATE);
  config_add_key(config_toplevel_keys, "Set", G_TOKEN_SET);
  config_add_key(config_toplevel_keys, "MenuClear", G_TOKEN_MENUCLEAR);
  config_add_key(config_toplevel_keys, "Menu", G_TOKEN_MENU);
  config_add_key(config_toplevel_keys, "Include", G_TOKEN_INCLUDE);
  config_add_key(config_toplevel_keys, "Grid", G_TOKEN_GRID);
  config_add_key(config_toplevel_keys, "Scale", G_TOKEN_SCALE);
  config_add_key(config_toplevel_keys, "Label", G_TOKEN_LABEL);
  config_add_key(config_toplevel_keys, "Button", G_TOKEN_BUTTON);
  config_add_key(config_toplevel_keys, "Image", G_TOKEN_IMAGE);
  config_add_key(config_toplevel_keys, "Chart", G_TOKEN_CHART);
  config_add_key(config_toplevel_keys, "Taskbar", G_TOKEN_TASKBAR);
  config_add_key(config_toplevel_keys, "Pager", G_TOKEN_PAGER);
  config_add_key(config_toplevel_keys, "Tray", G_TOKEN_TRAY);

  config_menu_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_menu_keys, "Item", G_TOKEN_ITEM);
  config_add_key(config_menu_keys, "Separator", G_TOKEN_SEPARATOR);
  config_add_key(config_menu_keys, "SubMenu", G_TOKEN_SUBMENU);
  config_add_key(config_menu_keys, "Sort", G_TOKEN_SORT);

  config_scanner_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_scanner_keys, "File", G_TOKEN_FILE);
  config_add_key(config_scanner_keys, "Exec", G_TOKEN_EXEC);
  config_add_key(config_scanner_keys, "MpdClient", G_TOKEN_MPDCLIENT);
  config_add_key(config_scanner_keys, "SwayClient", G_TOKEN_SWAYCLIENT);
  config_add_key(config_scanner_keys, "ExecClient", G_TOKEN_EXECCLIENT);
  config_add_key(config_scanner_keys, "SocketClient", G_TOKEN_SOCKETCLIENT);

  config_scanner_types = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_scanner_types, "RegEx", G_TOKEN_REGEX);
  config_add_key(config_scanner_types, "Json", G_TOKEN_JSON);
  config_add_key(config_scanner_types, "Grab", G_TOKEN_GRAB);

  config_scanner_flags = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_scanner_flags, "NoGlob", VF_NOGLOB);
  config_add_key(config_scanner_flags, "CheckTime", VF_CHTIME);

  config_filter_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_filter_keys, "Workspace", G_TOKEN_WORKSPACE);
  config_add_key(config_filter_keys, "Output", G_TOKEN_OUTPUT);
  config_add_key(config_filter_keys, "Floating", G_TOKEN_FLOATING);

  config_axis_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_axis_keys, "Cols", G_TOKEN_COLS);
  config_add_key(config_axis_keys, "Rows", G_TOKEN_ROWS);

  config_taskbar_types = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  g_hash_table_insert(config_taskbar_types, "False", taskbar_get_taskbar); 
  g_hash_table_insert(config_taskbar_types, "True", taskbar_popup_get_taskbar); 
  g_hash_table_insert(config_taskbar_types, "PopUp", taskbar_popup_get_taskbar); 
  g_hash_table_insert(config_taskbar_types, "Pager", taskbar_pager_get_taskbar); 

  config_widget_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  g_hash_table_insert(config_widget_keys, "Pager", pager_get_type);
  g_hash_table_insert(config_widget_keys, "Grid", grid_get_type);
  g_hash_table_insert(config_widget_keys, "Scale", scale_get_type);
  g_hash_table_insert(config_widget_keys, "Label", label_get_type);
  g_hash_table_insert(config_widget_keys, "Button", button_get_type);
  g_hash_table_insert(config_widget_keys, "Image", image_get_type);
  g_hash_table_insert(config_widget_keys, "Chart", cchart_get_type);
  g_hash_table_insert(config_widget_keys, "Taskbar", taskbar_shell_get_type);
  g_hash_table_insert(config_widget_keys, "Tray", tray_get_type);

  config_prop_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_prop_keys, "Style", G_TOKEN_STYLE);
  config_add_key(config_prop_keys, "Value", G_TOKEN_VALUE);
  config_add_key(config_prop_keys, "Css", G_TOKEN_CSS);
  config_add_key(config_prop_keys, "Interval", G_TOKEN_INTERVAL);
  config_add_key(config_prop_keys, "Trigger", G_TOKEN_TRIGGER);
  config_add_key(config_prop_keys, "Local", G_TOKEN_LOCAL);
  config_add_key(config_prop_keys, "Pins", G_TOKEN_PINS);
  config_add_key(config_prop_keys, "Preview", G_TOKEN_PREVIEW);
  config_add_key(config_prop_keys, "Action", G_TOKEN_ACTION);
  config_add_key(config_prop_keys, "Loc", G_TOKEN_LOC);
  config_add_key(config_prop_keys, "Filter_output", G_TOKEN_PEROUTPUT);
  config_add_key(config_prop_keys, "AutoClose", G_TOKEN_AUTOCLOSE);
  config_add_key(config_prop_keys, "Tooltip", G_TOKEN_TOOLTIP);
  config_add_key(config_prop_keys, "Group", G_TOKEN_GROUP);
  config_add_key(config_prop_keys, "Filter", G_TOKEN_FILTER);
  config_add_key(config_prop_keys, "Exclusive_zone", G_TOKEN_EXCLUSIVEZONE);
  config_add_key(config_prop_keys, "Layer", G_TOKEN_LAYER);
  config_add_key(config_prop_keys, "Size", G_TOKEN_SIZE);
  config_add_key(config_prop_keys, "Sensor", G_TOKEN_SENSOR);
  config_add_key(config_prop_keys, "Sensor_delay", G_TOKEN_SENSORDELAY);
  config_add_key(config_prop_keys, "Transition", G_TOKEN_TRANSITION);
  config_add_key(config_prop_keys, "Bar_id", G_TOKEN_BAR_ID);
  config_add_key(config_prop_keys, "Monitor", G_TOKEN_MONITOR);
  config_add_key(config_prop_keys, "Margin", G_TOKEN_MARGIN);
  config_add_key(config_prop_keys, "Mirror", G_TOKEN_MIRROR);
  config_add_key(config_prop_keys, "Tooltips", G_TOKEN_TOOLTIPS);
  config_add_key(config_prop_keys, "Disable", G_TOKEN_DISABLE);

  config_flowgrid_props = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_flowgrid_props, "Cols", G_TOKEN_COLS);
  config_add_key(config_flowgrid_props, "Rows", G_TOKEN_ROWS);
  config_add_key(config_flowgrid_props, "Icons", G_TOKEN_ICONS);
  config_add_key(config_flowgrid_props, "Labels", G_TOKEN_LABELS);
  config_add_key(config_flowgrid_props, "Sort", G_TOKEN_SORT);
  config_add_key(config_flowgrid_props, "Numeric", G_TOKEN_NUMERIC);
  config_add_key(config_flowgrid_props, "Primary", G_TOKEN_PRIMARY);
  config_add_key(config_flowgrid_props, "Title_width", G_TOKEN_TITLEWIDTH);

  config_placer_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_placer_keys, "XStep", G_TOKEN_XSTEP);
  config_add_key(config_placer_keys, "YStep", G_TOKEN_YSTEP);
  config_add_key(config_placer_keys, "XOrigin", G_TOKEN_XORIGIN);
  config_add_key(config_placer_keys, "YOrigin", G_TOKEN_YORIGIN);
  config_add_key(config_placer_keys, "Children", G_TOKEN_CHILDREN);
  config_add_key(config_placer_keys, "Disable", G_TOKEN_DISABLE);

  config_menu_item_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_menu_item_keys, "Label", G_TOKEN_LABEL);
  config_add_key(config_menu_item_keys, "Action", G_TOKEN_ACTION);
  config_add_key(config_menu_item_keys, "Tooltip", G_TOKEN_TOOLTIP);
  config_add_key(config_menu_item_keys, "Index", G_TOKEN_INDEX);
  config_add_key(config_menu_item_keys, "Submenu", G_TOKEN_SUBMENU);
  config_add_key(config_menu_item_keys, "DesktopId", G_TOKEN_DESKTOPID);

  parser_init();
}

void config_log_error ( GScanner *scanner, gchar *message, gboolean error )
{
  if(error)
  {
    if(!scanner->max_parse_errors)
      g_message("%s:%d: %s", scanner->input_name, scanner->line, message);
    scanner->max_parse_errors = TRUE;
  }
  else
    g_message("%s:%d: %s", scanner->input_name, scanner->line, message);
}

GtkWidget *config_parse_data ( gchar *fname, gchar *data, GtkWidget *container,
   vm_store_t *globals )
{
  GScanner *scanner;
  GtkWidget *w;
  GtkCssProvider *css;
  gchar *tmp;

  if(!data)
    return NULL;

  scanner = g_scanner_new(&scanner_config);
  while(globals && globals->transient)
    globals = globals->parent;

  scanner->msg_handler = config_log_error;
  scanner->max_parse_errors = FALSE;
  scanner->user_data = g_malloc0(sizeof(scanner_data_t));
  SCANNER_STORE(scanner) = globals? globals : vm_store_new(NULL, FALSE);

  scanner->input_name = fname;
  g_scanner_input_text(scanner, data, strlen(data));


  if( (tmp = strstr(data, "\n#CSS")) )
    *tmp = 0;
  w = config_parse_toplevel(scanner, container);
  g_free(scanner->user_data);
  g_scanner_destroy(scanner);

  if(tmp)
  {
    css = gtk_css_provider_new();
    tmp = css_legacy_preprocess(g_strdup(tmp + 5), fname);
    gtk_css_provider_load_from_data(css, tmp, strlen(tmp), NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);
    g_free(tmp);
  }

  return w;
}

GtkWidget *config_parse ( gchar *file, GtkWidget *container,
    vm_store_t *globals )
{
  GtkWidget *w = NULL;
  gchar *fname, *dir, *base, *cssfile, *csspath, *tmp;
  gchar *conf = NULL;

  if( !(fname = get_xdg_config_file(file, NULL)) ||
      !g_file_get_contents(fname, &conf, NULL, NULL))
  {
    g_warning("Error reading config file %s", file);
    g_warning("Please note that relative paths are disabled.");
    g_free(fname);
    return NULL;
  }

  g_debug("include: %s -> %s", file, fname);
  w = config_parse_data (fname, conf, container, globals);

  g_free(conf);

  dir = g_path_get_dirname(fname);
  base = g_path_get_basename(fname);
  
  if( (tmp = strrchr(base,'.')) )
    *tmp = '\0';
  cssfile = g_strconcat(base, ".css", NULL);
  csspath = g_build_filename(dir, cssfile, NULL);

  css_file_load (csspath);

  g_free(csspath);
  g_free(cssfile);
  g_free(base);
  g_free(dir);
  g_free(fname);

  return w;
}
