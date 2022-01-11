/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <gtk/gtk.h>
#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

static GtkWindow *bar_window;
gchar *confname;
gchar *sockname;
static gchar *cssname;
static gchar *monitor;
static gboolean debug = FALSE;
static gint toplevel_dir;

static GOptionEntry entries[] = {
  {"config",'f',0,G_OPTION_ARG_FILENAME,&confname,"Specify config file"},
  {"css",'c',0,G_OPTION_ARG_FILENAME,&cssname,"Specify css file"},
  {"socket",'s',0,G_OPTION_ARG_FILENAME,&sockname,"Specify sway socket file"},
  {"debug",'d',0,G_OPTION_ARG_NONE,&debug,"Display debug info"},
  {"monitor",'m',0,G_OPTION_ARG_STRING,&monitor,
    "Monitor to display the panel on (use \"-m list\" to list monitors`"},
  {NULL}};

void parse_command_line ( gint argc, gchar **argv)
{
  GOptionContext *optc;
  optc = g_option_context_new(" - Sway Floating Window Bar");
  g_option_context_add_main_entries(optc,entries,NULL);
  g_option_context_add_group (optc, gtk_get_option_group (TRUE));
  g_option_context_parse(optc,&argc,&argv,NULL);
}

gint get_toplevel_dir ( void )
{
  return toplevel_dir;
}

void set_monitor ( void )
{
  GdkDisplay *gdisp;
  GdkDisplayManager *gdman;
  GdkMonitor *gmon,*match;
  GSList *dlist;
  gint nmon,i;
  gchar *name;
  gboolean list;

  list = !g_strcmp0(monitor,"list");

  gdman = gdk_display_manager_get();
  gdisp = gdk_display_get_default();
  match = gdk_display_get_primary_monitor(gdisp);
  dlist = gdk_display_manager_list_displays(gdman);

  for(;dlist!=NULL;dlist=g_slist_next(dlist))
  {
    gdisp = dlist->data;
    nmon = gdk_display_get_n_monitors(gdisp);
    for(i=0;i<nmon;i++)
    {
      gmon = gdk_display_get_monitor(gdisp,i);
      name = gdk_monitor_get_xdg_name(gmon);
      if(list)
        printf("%s: %s %s\n",name,gdk_monitor_get_manufacturer(gmon),
            gdk_monitor_get_model(gmon));
      else
        if(g_strcmp0(name,monitor)==0)
          match = gmon;
      g_free(name);
    }
  }
  if(list)
    exit(0);

  gtk_layer_set_monitor(bar_window, match);
}

void monitor_change_cb ( void )
{
  gtk_widget_hide ((GtkWidget *)bar_window);
  set_monitor();
  gtk_widget_show ((GtkWidget *)bar_window);
}

gboolean window_hide_event ( struct json_object *obj )
{
  gchar *mode, state;

  if ( obj )
  {
    mode = json_string_by_name(obj,"mode");
    if(mode!=NULL)
      state = *mode;
    else
      state ='s';
    g_free(mode);
  }
  else
    if( gtk_widget_is_visible (GTK_WIDGET(bar_window)) )
      state = 'h';
    else
      state = 's';

  if(state=='h')
    gtk_widget_hide(GTK_WIDGET(bar_window));
  else
    gtk_widget_show(GTK_WIDGET(bar_window));
  return TRUE;
}

void log_print ( const gchar *log_domain, GLogLevelFlags log_level, 
    const gchar *message, gpointer data )
{
  GDateTime *now;
  if(!debug && (log_level==G_LOG_LEVEL_DEBUG || log_level==G_LOG_LEVEL_INFO) )
    return;

  now = g_date_time_new_now_local();

  fprintf(stderr,"%02d:%02d:%02.2f %s\n",
      g_date_time_get_hour(now),
      g_date_time_get_minute(now),
      g_date_time_get_seconds(now),
      message);

  g_date_time_unref(now);
}

void css_init ( void )
{
  gchar *css_str;
  GtkCssProvider *css;
  GtkWidgetClass *widget_class = g_type_class_ref(GTK_TYPE_WIDGET);

  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_double("align","text alignment","text alignment",
      0.0,1.0,0.0, G_PARAM_READABLE));

  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boolean("hexpand","horizonal expansion","horizontal expansion",
       FALSE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boolean("vexpand","vertical expansion","vertical expansion",
      FALSE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_int("icon-size","icon size","icon size",
      0,500,48, G_PARAM_READABLE));

  static GEnumValue dir_types [] = {
    {GTK_POS_TOP,"top","top"},
    {GTK_POS_BOTTOM,"bottom","bottom"},
    {GTK_POS_LEFT,"left","left"},
    {GTK_POS_RIGHT,"right","right"},
    {0,NULL,NULL}};
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_enum("direction","direction","direction",
      g_enum_register_static ("direction",dir_types),
      GTK_POS_RIGHT, G_PARAM_READABLE));

  css_str = "window { -GtkWidget-direction: bottom; }";
  css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css,css_str,strlen(css_str),NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
    GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);

  if(cssname!=NULL)
  {
    css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css,cssname,NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
  }
}

gboolean shell_timer ( gpointer data )
{
  taskbar_update();
  switcher_update();
  sni_update();

  return TRUE;
}

static void activate (GtkApplication* app, gpointer data )
{
  struct layout_widget *lw;
  GdkDisplay *gdisp;

  bar_window = (GtkWindow *)gtk_application_window_new (app);
  gtk_widget_set_name(GTK_WIDGET(bar_window),"sfwbar");
  gtk_layer_init_for_window (bar_window);
  gtk_layer_auto_exclusive_zone_enable (bar_window);
  gtk_layer_set_keyboard_interactivity(bar_window,FALSE);
  gtk_layer_set_layer(bar_window,GTK_LAYER_SHELL_LAYER_OVERLAY);

  css_init();

  gtk_widget_style_get(GTK_WIDGET(bar_window),"direction",&toplevel_dir,NULL);
  gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_LEFT,
      !(toplevel_dir==GTK_POS_RIGHT));
  gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_RIGHT,
      !(toplevel_dir==GTK_POS_LEFT));
  gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_BOTTOM,
      !(toplevel_dir==GTK_POS_TOP));
  gtk_layer_set_anchor (bar_window,GTK_LAYER_SHELL_EDGE_TOP,
      !(toplevel_dir==GTK_POS_BOTTOM));

  lw = config_parse(confname?confname:"sfwbar.config");
  if((lw != NULL)&&(lw->widget!=NULL))
  {
    gtk_container_add(GTK_CONTAINER(bar_window), lw->widget);
    layout_widget_free(lw);

    gtk_widget_show_all ((GtkWidget *)bar_window);
  }

  sway_ipc_init();
  if(!sway_ipc_active())
    wayland_init(bar_window,TRUE);
  else
    wayland_init(bar_window,FALSE);

  if(monitor)
    set_monitor();

  gdisp = gdk_screen_get_display(gtk_window_get_screen(bar_window)),
        gtk_widget_get_window(GTK_WIDGET(bar_window));
  g_signal_connect(gdisp, "monitor-added",monitor_change_cb,NULL);
  g_signal_connect(gdisp, "monitor-removed",monitor_change_cb,NULL);

  g_thread_unref(g_thread_new("scanner",layout_scanner_thread,
        g_main_context_get_thread_default()));

  g_timeout_add (100,(GSourceFunc )shell_timer,NULL);
  g_unix_signal_add(10,(GSourceFunc)switcher_event,NULL);
  g_unix_signal_add(12,(GSourceFunc)window_hide_event,NULL);
  gtk_widget_show_all ((GtkWidget *)bar_window);
}

int main (int argc, gchar **argv)
{
  GtkApplication *app;
  gint status;

  g_log_set_handler(NULL,G_LOG_LEVEL_MASK,log_print,NULL);

  parse_command_line(argc,argv);

  app = gtk_application_new ("org.gtk.sfwbar", G_APPLICATION_NON_UNIQUE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
