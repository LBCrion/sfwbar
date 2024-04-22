/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "../config.h"
#include "../sfwbar.h"
#include "../taskbar.h"

GHashTable *config_mods, *config_events, *config_var_types, *config_act_cond;
GHashTable *config_toplevel_keys, *config_menu_keys, *config_scanner_keys;
GHashTable *config_scanner_types, *config_scanner_flags, *config_filter_keys;
GHashTable *config_axis_keys, *config_taskbar_types, *config_widget_keys;
GHashTable *config_prop_keys, *config_placer_keys, *config_flowgrid_props;

#define config_add_key(table, str, key) \
  g_hash_table_insert(table, str, GINT_TO_POINTER(key))

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
  config_add_key(config_taskbar_types, "True", TASKBAR_POPUP);
  config_add_key(config_taskbar_types, "False", TASKBAR_NORMAL);
  config_add_key(config_taskbar_types, "PopUp", TASKBAR_POPUP);
  config_add_key(config_taskbar_types, "Pager", TASKBAR_DESK);

  config_widget_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_widget_keys, "Pager", G_TOKEN_PAGER);
  config_add_key(config_widget_keys, "Grid", G_TOKEN_GRID);
  config_add_key(config_widget_keys, "Scale", G_TOKEN_SCALE);
  config_add_key(config_widget_keys, "Label", G_TOKEN_LABEL);
  config_add_key(config_widget_keys, "Button", G_TOKEN_BUTTON);
  config_add_key(config_widget_keys, "Image", G_TOKEN_IMAGE);
  config_add_key(config_widget_keys, "Chart", G_TOKEN_CHART);
  config_add_key(config_widget_keys, "Include", G_TOKEN_INCLUDE);
  config_add_key(config_widget_keys, "Taskbar", G_TOKEN_TASKBAR);
  config_add_key(config_widget_keys, "Tray", G_TOKEN_TRAY);

  config_prop_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_prop_keys, "Style", G_TOKEN_STYLE);
  config_add_key(config_prop_keys, "Value", G_TOKEN_VALUE);
  config_add_key(config_prop_keys, "Css", G_TOKEN_CSS);
  config_add_key(config_prop_keys, "Interval", G_TOKEN_INTERVAL);
  config_add_key(config_prop_keys, "Trigger", G_TOKEN_TRIGGER);
  config_add_key(config_prop_keys, "Pins", G_TOKEN_PINS);
  config_add_key(config_prop_keys, "Preview", G_TOKEN_PREVIEW);
  config_add_key(config_prop_keys, "Action", G_TOKEN_ACTION);
  config_add_key(config_prop_keys, "Loc", G_TOKEN_LOC);
  config_add_key(config_prop_keys, "Filter_output", G_TOKEN_PEROUTPUT);
  config_add_key(config_prop_keys, "Title_width", G_TOKEN_TITLEWIDTH);
  config_add_key(config_prop_keys, "AutoClose", G_TOKEN_AUTOCLOSE);
  config_add_key(config_prop_keys, "Tooltip", G_TOKEN_TOOLTIP);
  config_add_key(config_prop_keys, "Group", G_TOKEN_GROUP);
  config_add_key(config_prop_keys, "Filter", G_TOKEN_FILTER);

config_flowgrid_props = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_flowgrid_props, "Cols", G_TOKEN_COLS);
  config_add_key(config_flowgrid_props, "Rows", G_TOKEN_ROWS);
  config_add_key(config_flowgrid_props, "Icons", G_TOKEN_ICONS);
  config_add_key(config_flowgrid_props, "Labels", G_TOKEN_LABELS);
  config_add_key(config_flowgrid_props, "Sort", G_TOKEN_SORT);
  config_add_key(config_flowgrid_props, "Numeric", G_TOKEN_NUMERIC);
  config_add_key(config_flowgrid_props, "Primary", G_TOKEN_PRIMARY);

  config_placer_keys = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  config_add_key(config_placer_keys, "XStep", G_TOKEN_XSTEP);
  config_add_key(config_placer_keys, "YStep", G_TOKEN_YSTEP);
  config_add_key(config_placer_keys, "XOrigin", G_TOKEN_XORIGIN);
  config_add_key(config_placer_keys, "YOrigin", G_TOKEN_YORIGIN);
  config_add_key(config_placer_keys, "Children", G_TOKEN_CHILDREN);
}

gint config_lookup_key ( GScanner *scanner, GHashTable *table )
{
  if(scanner->token != G_TOKEN_IDENTIFIER)
    return 0;

  return GPOINTER_TO_INT(g_hash_table_lookup(table,
        scanner->value.v_identifier));
}

gint config_lookup_next_key ( GScanner *scanner, GHashTable *table )
{
  if(g_scanner_peek_next_token(scanner) != G_TOKEN_IDENTIFIER)
    return 0;

  return GPOINTER_TO_INT( g_hash_table_lookup(table,
        scanner->next_value.v_identifier) );
}

void config_log_error ( GScanner *scanner, gchar *message, gboolean error )
{
  if(error)
  {
    if(!scanner->max_parse_errors)
      g_message("%s:%d: %s",scanner->input_name,scanner->line,message);
    scanner->max_parse_errors = TRUE;
  }
  else
    g_message("%s:%d: %s",scanner->input_name,scanner->line,message);
}

GtkWidget *config_parse_data ( gchar *fname, gchar *data, gboolean toplevel )
{
  GScanner *scanner;
  GtkWidget *w;
  GtkCssProvider *css;
  gchar *tmp;

  if(!data)
    return NULL;

  scanner = g_scanner_new(NULL);
  scanner->config->scan_octal = 0;
  scanner->config->symbol_2_token = 1;
  scanner->config->case_sensitive = 0;
  scanner->config->numbers_2_int = 1;
  scanner->config->int_2_float = 1;
  scanner->config->scope_0_fallback = 1;

  scanner->config->cset_identifier_nth = g_strconcat(".",
      scanner->config->cset_identifier_nth,NULL);
  scanner->config->cset_identifier_first = g_strconcat("$",
      scanner->config->cset_identifier_first,NULL);

  scanner->msg_handler = config_log_error;
  scanner->max_parse_errors = FALSE;

  tmp = strstr(data,"\n#CSS");
  if(tmp)
  {
    *tmp=0;
    css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,tmp+5,strlen(tmp+5),NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);
  }

  scanner->input_name = fname;
  g_scanner_input_text( scanner, data, strlen(data) );

  w = config_parse_toplevel ( scanner, toplevel );
  g_free(scanner->config->cset_identifier_first);
  g_free(scanner->config->cset_identifier_nth);
  g_scanner_destroy(scanner);

  return w;
}

void config_string ( gchar *string )
{
  gchar *conf;

  if(!string || !*string)
    return;

  conf = g_strdup(string);
  g_debug("parsing config string: %s", conf);
  config_parse_data("config string",conf,TRUE);
  g_free(conf);
}

void config_pipe_read ( gchar *command )
{
  FILE *fp;
  gchar *conf;
  GIOChannel *chan;

  if(!command)
    return;

  fp = popen(command, "r");
  if(!fp)
    return;

  chan = g_io_channel_unix_new( fileno(fp) );
  if(chan)
  {
    if(g_io_channel_read_to_end( chan , &conf,NULL,NULL)==G_IO_STATUS_NORMAL)
      config_parse_data(command,conf,TRUE);
    g_free(conf);
    g_io_channel_unref(chan);
  }

  pclose(fp);
}

GtkWidget *config_parse ( gchar *file, gboolean toplevel )
{
  gchar *fname, *dir, *base ,*cssfile, *csspath, *tmp;
  gchar *conf=NULL;
  gsize size;
  GtkWidget *w=NULL;

  fname = get_xdg_config_file(file,NULL);
  g_debug("include: %s -> %s",file,fname);

  if(fname)
    if(!g_file_get_contents(fname,&conf,&size,NULL))
      conf=NULL;

  if(!conf)
  {
    g_error("Error: can't read config file %s\n",file);
    exit(1);
  }

  w = config_parse_data (fname, conf, toplevel);

  g_free(conf);

  dir = g_path_get_dirname(fname);
  base = g_path_get_basename(fname);
  
  tmp = strrchr(base,'.');
  if(tmp)
    *tmp = '\0';
  cssfile = g_strconcat(base,".css",NULL);
  csspath = g_build_filename(dir,cssfile,NULL);

  css_file_load (csspath);

  g_free(csspath);
  g_free(cssfile);
  g_free(base);
  g_free(dir);
  g_free(fname);

  return w;
}
