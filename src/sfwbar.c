/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <gtk/gtk.h>
#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

gchar *confname;
gchar *sockname;
static gchar *cssname;
static gchar *monitor;
static gboolean debug = FALSE;

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

void list_monitors ( void )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon;
  gint nmon,i;
  gchar *name;

  gdisp = gdk_display_get_default();
  nmon = gdk_display_get_n_monitors(gdisp);
  for(i=0;i<nmon;i++)
  {
    gmon = gdk_display_get_monitor(gdisp,i);
    name = gdk_monitor_get_xdg_name(gmon);
    printf("%s: %s %s\n",name,gdk_monitor_get_manufacturer(gmon),
        gdk_monitor_get_model(gmon));
    g_free(name);
  }
  exit(0);
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
      0.0,1.0,0.5, G_PARAM_READABLE));

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
  GtkWindow *bar_window;
  GdkDisplay *gdisp;
  GtkWidget *box;

  css_init();
  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  lw = config_parse(confname?confname:"sfwbar.config",box);

  bar_window = bar_new(app);
  if(bar_get_toplevel_dir() == GTK_POS_LEFT || 
      bar_get_toplevel_dir() == GTK_POS_RIGHT)
    gtk_orientable_set_orientation(GTK_ORIENTABLE(box),
        GTK_ORIENTATION_VERTICAL);

  sway_ipc_init();
  wayland_init();

  if(monitor && !g_ascii_strcasecmp(monitor,"list"))
    list_monitors();

  if((lw != NULL)&&(lw->widget!=NULL))
  {
    gtk_box_pack_start(GTK_BOX(box),lw->widget,TRUE,TRUE,0);
    gtk_container_add(GTK_CONTAINER(bar_window), box);
    layout_widget_attach(lw);

    gtk_widget_show_all ((GtkWidget *)bar_window);
    layout_widgets_autoexec(GTK_WIDGET(bar_window),NULL);
  }

  if(monitor)
    bar_set_monitor(monitor);

  gdisp = gdk_display_get_default();
  g_signal_connect(gdisp, "monitor-added",bar_monitor_change_cb,NULL);
  g_signal_connect(gdisp, "monitor-removed",bar_monitor_change_cb,NULL);

  g_thread_unref(g_thread_new("scanner",layout_scanner_thread,
        g_main_context_get_thread_default()));

  action_function_exec("SfwBarInit",NULL,NULL,NULL,NULL);

  g_timeout_add (100,(GSourceFunc )shell_timer,NULL);
  g_unix_signal_add(10,(GSourceFunc)switcher_event,NULL);
  g_unix_signal_add(12,(GSourceFunc)bar_hide_event,NULL);
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
