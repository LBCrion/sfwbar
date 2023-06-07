/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "../config.h"
#include "../sfwbar.h"

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
  scanner->config->scope_0_fallback = 0;

  scanner->config->cset_identifier_nth = g_strconcat(".",
      scanner->config->cset_identifier_nth,NULL);
  scanner->config->cset_identifier_first = g_strconcat("$",
      scanner->config->cset_identifier_first,NULL);

  scanner->msg_handler = config_log_error;
  scanner->max_parse_errors = FALSE;

  g_scanner_scope_add_symbol(scanner,0, "Scanner", (gpointer)G_TOKEN_SCANNER );
  g_scanner_scope_add_symbol(scanner,0, "Layout", (gpointer)G_TOKEN_LAYOUT );
  g_scanner_scope_add_symbol(scanner,0, "PopUp", (gpointer)G_TOKEN_POPUP );
  g_scanner_scope_add_symbol(scanner,0, "Placer", (gpointer)G_TOKEN_PLACER );
  g_scanner_scope_add_symbol(scanner,0, "Switcher",
      (gpointer)G_TOKEN_SWITCHER );
  g_scanner_scope_add_symbol(scanner,0, "Define", (gpointer)G_TOKEN_DEFINE );
  g_scanner_scope_add_symbol(scanner,0, "TriggerAction",
      (gpointer)G_TOKEN_TRIGGERACTION );
  g_scanner_scope_add_symbol(scanner,0, "MapAppId",
      (gpointer)G_TOKEN_MAPAPPID );
  g_scanner_scope_add_symbol(scanner,0, "FilterAppId",
      (gpointer)G_TOKEN_FILTERAPPID );
  g_scanner_scope_add_symbol(scanner,0, "FilterTitle",
      (gpointer)G_TOKEN_FILTERTITLE );
  g_scanner_scope_add_symbol(scanner,0, "Module",
      (gpointer)G_TOKEN_MODULE );
  g_scanner_scope_add_symbol(scanner,0, "Theme", (gpointer)G_TOKEN_THEME );
  g_scanner_scope_add_symbol(scanner,0, "DisownMinimized",
      (gpointer)G_TOKEN_DISOWNMINIMIZED );
  g_scanner_scope_add_symbol(scanner,0, "End", (gpointer)G_TOKEN_END );
  g_scanner_scope_add_symbol(scanner,0, "File", (gpointer)G_TOKEN_FILE );
  g_scanner_scope_add_symbol(scanner,0, "Exec", (gpointer)G_TOKEN_EXEC );
  g_scanner_scope_add_symbol(scanner,0, "MpdClient",
      (gpointer)G_TOKEN_MPDCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "SwayClient",
      (gpointer)G_TOKEN_SWAYCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "ExecClient",
      (gpointer)G_TOKEN_EXECCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "SocketClient",
      (gpointer)G_TOKEN_SOCKETCLIENT );
  g_scanner_scope_add_symbol(scanner,0, "Number", (gpointer)G_TOKEN_NUMBERW );
  g_scanner_scope_add_symbol(scanner,0, "String", (gpointer)G_TOKEN_STRINGW );
  g_scanner_scope_add_symbol(scanner,0, "NoGlob", (gpointer)G_TOKEN_NOGLOB );
  g_scanner_scope_add_symbol(scanner,0, "CheckTime",
      (gpointer)G_TOKEN_CHTIME );
  g_scanner_scope_add_symbol(scanner,0, "Sum", (gpointer)G_TOKEN_SUM );
  g_scanner_scope_add_symbol(scanner,0, "Product", (gpointer)G_TOKEN_PRODUCT );
  g_scanner_scope_add_symbol(scanner,0, "Last", (gpointer)G_TOKEN_LASTW );
  g_scanner_scope_add_symbol(scanner,0, "First", (gpointer)G_TOKEN_FIRST );
  g_scanner_scope_add_symbol(scanner,0, "Grid", (gpointer)G_TOKEN_GRID );
  g_scanner_scope_add_symbol(scanner,0, "Scale", (gpointer)G_TOKEN_SCALE );
  g_scanner_scope_add_symbol(scanner,0, "Label", (gpointer)G_TOKEN_LABEL );
  g_scanner_scope_add_symbol(scanner,0, "Button", (gpointer)G_TOKEN_BUTTON );
  g_scanner_scope_add_symbol(scanner,0, "Image", (gpointer)G_TOKEN_IMAGE );
  g_scanner_scope_add_symbol(scanner,0, "Chart", (gpointer)G_TOKEN_CHART );
  g_scanner_scope_add_symbol(scanner,0, "Include", (gpointer)G_TOKEN_INCLUDE );
  g_scanner_scope_add_symbol(scanner,0, "TaskBar", (gpointer)G_TOKEN_TASKBAR );
  g_scanner_scope_add_symbol(scanner,0, "Pager", (gpointer)G_TOKEN_PAGER );
  g_scanner_scope_add_symbol(scanner,0, "Tray", (gpointer)G_TOKEN_TRAY );
  g_scanner_scope_add_symbol(scanner,0, "Style", (gpointer)G_TOKEN_STYLE );
  g_scanner_scope_add_symbol(scanner,0, "Css", (gpointer)G_TOKEN_CSS );
  g_scanner_scope_add_symbol(scanner,0, "Interval",
      (gpointer)G_TOKEN_INTERVAL );
  g_scanner_scope_add_symbol(scanner,0, "Value", (gpointer)G_TOKEN_VALUE );
  g_scanner_scope_add_symbol(scanner,0, "Pins", (gpointer)G_TOKEN_PINS );
  g_scanner_scope_add_symbol(scanner,0, "Preview", (gpointer)G_TOKEN_PREVIEW );
  g_scanner_scope_add_symbol(scanner,0, "Cols", (gpointer)G_TOKEN_COLS );
  g_scanner_scope_add_symbol(scanner,0, "Rows", (gpointer)G_TOKEN_ROWS );
  g_scanner_scope_add_symbol(scanner,0, "Action", (gpointer)G_TOKEN_ACTION );
  g_scanner_scope_add_symbol(scanner,0, "Display", (gpointer)G_TOKEN_DISPLAY );
  g_scanner_scope_add_symbol(scanner,0, "Icons", (gpointer)G_TOKEN_ICONS );
  g_scanner_scope_add_symbol(scanner,0, "Labels", (gpointer)G_TOKEN_LABELS );
  g_scanner_scope_add_symbol(scanner,0, "Loc", (gpointer)G_TOKEN_LOC );
  g_scanner_scope_add_symbol(scanner,0, "Numeric", (gpointer)G_TOKEN_NUMERIC );
  g_scanner_scope_add_symbol(scanner,0, "Filter_output", 
      (gpointer)G_TOKEN_PEROUTPUT );
  g_scanner_scope_add_symbol(scanner,0, "Title_width", 
      (gpointer)G_TOKEN_TITLEWIDTH );
  g_scanner_scope_add_symbol(scanner,0, "Tooltip", (gpointer)G_TOKEN_TOOLTIP );
  g_scanner_scope_add_symbol(scanner,0, "Trigger", (gpointer)G_TOKEN_TRIGGER );
  g_scanner_scope_add_symbol(scanner,0, "Group", (gpointer)G_TOKEN_GROUP );
  g_scanner_scope_add_symbol(scanner,0, "XStep", (gpointer)G_TOKEN_XSTEP );
  g_scanner_scope_add_symbol(scanner,0, "YStep", (gpointer)G_TOKEN_YSTEP );
  g_scanner_scope_add_symbol(scanner,0, "XOrigin", (gpointer)G_TOKEN_XORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "YOrigin", (gpointer)G_TOKEN_YORIGIN );
  g_scanner_scope_add_symbol(scanner,0, "Children", 
      (gpointer)G_TOKEN_CHILDREN );
  g_scanner_scope_add_symbol(scanner,0, "Sort", (gpointer)G_TOKEN_SORT );
  g_scanner_scope_add_symbol(scanner,0, "Filter", (gpointer)G_TOKEN_FILTER );
  g_scanner_scope_add_symbol(scanner,0, "Primary", (gpointer)G_TOKEN_PRIMARY );
  g_scanner_scope_add_symbol(scanner,0, "True", (gpointer)G_TOKEN_TRUE );
  g_scanner_scope_add_symbol(scanner,0, "False", (gpointer)G_TOKEN_FALSE );
  g_scanner_scope_add_symbol(scanner,0, "Menu", (gpointer)G_TOKEN_MENU );
  g_scanner_scope_add_symbol(scanner,0, "MenuClear", 
      (gpointer)G_TOKEN_MENUCLEAR );
  g_scanner_scope_add_symbol(scanner,0, "UserState",
      (gpointer)G_TOKEN_USERSTATE );
  g_scanner_scope_add_symbol(scanner,0, "UserState2",
      (gpointer)G_TOKEN_USERSTATE2 );
  g_scanner_scope_add_symbol(scanner,0, "Function",
      (gpointer)G_TOKEN_FUNCTION );
  g_scanner_scope_add_symbol(scanner,0, "Item", (gpointer)G_TOKEN_ITEM );
  g_scanner_scope_add_symbol(scanner,0, "Separator",
      (gpointer)G_TOKEN_SEPARATOR );
  g_scanner_scope_add_symbol(scanner,0, "SubMenu", (gpointer)G_TOKEN_SUBMENU );
  g_scanner_scope_add_symbol(scanner,0, "Minimized",
      (gpointer)G_TOKEN_MINIMIZED );
  g_scanner_scope_add_symbol(scanner,0, "Maximized",
      (gpointer)G_TOKEN_MAXIMIZED );
  g_scanner_scope_add_symbol(scanner,0, "FullScreen",
      (gpointer)G_TOKEN_FULLSCREEN );
  g_scanner_scope_add_symbol(scanner,0, "Focused", (gpointer)G_TOKEN_FOCUSED );
  g_scanner_scope_add_symbol(scanner,0, "RegEx", (gpointer)G_TOKEN_REGEX );
  g_scanner_scope_add_symbol(scanner,0, "Json", (gpointer)G_TOKEN_JSON );
  g_scanner_scope_add_symbol(scanner,0, "Set", (gpointer)G_TOKEN_SET );
  g_scanner_scope_add_symbol(scanner,0, "Grab", (gpointer)G_TOKEN_GRAB );
  g_scanner_scope_add_symbol(scanner,0, "Title", (gpointer)G_TOKEN_TITLE );
  g_scanner_scope_add_symbol(scanner,0, "AppId", (gpointer)G_TOKEN_APPID );
  g_scanner_scope_add_symbol(scanner,0, "Workspace",
      (gpointer)G_TOKEN_WORKSPACE );
  g_scanner_scope_add_symbol(scanner,0, "Output", (gpointer)G_TOKEN_OUTPUT );
  g_scanner_scope_add_symbol(scanner,0, "Floating",
      (gpointer)G_TOKEN_FLOATING );

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
  g_scanner_input_text( scanner, data, -1 );

  w = config_parse_toplevel ( scanner, toplevel );
  g_free(scanner->config->cset_identifier_first);
  g_free(scanner->config->cset_identifier_nth);
  g_scanner_destroy(scanner);

  return w;
}

void config_string ( gchar *string )
{
  gchar *conf;

  if(!string)
    return;

  conf = g_strdup(string);
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
