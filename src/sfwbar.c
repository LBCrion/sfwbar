/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <gtk/gtk.h>
#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

gchar *confname=NULL, *cssname=NULL, *sockname=NULL;
static GOptionEntry entries[] = {
  {"config",'f',0,G_OPTION_ARG_FILENAME,&confname,"Specify config file"},
  {"css",'c',0,G_OPTION_ARG_FILENAME,&cssname,"Specify css file"},
  {"socket",'s',0,G_OPTION_ARG_FILENAME,&sockname,"Specify sway socket file"},
  {NULL}};

void parse_command_line ( gint argc, gchar **argv)
{
  GOptionContext *optc;
  optc = g_option_context_new(" - Sway Floating Window Bar");
  g_option_context_add_main_entries(optc,entries,NULL);
  g_option_context_add_group (optc, gtk_get_option_group (TRUE));
  g_option_context_parse(optc,&argc,&argv,NULL);
}

void init_context ( void )
{
  context = g_malloc( sizeof(struct context) );
  context->scan_list=NULL;
  context->file_list=NULL;
  context->widgets=NULL;
  context->wt_list = NULL;
  context->sni_ifaces=NULL;
  context->sni_items=NULL;
  context->features=0;
  context->ipc = -1;
  context->sw_count=0;
  context->sw_hstate='s';
  context->buff_len = 1024;
  context->read_buff = g_malloc(context->buff_len);
  context->features |= F_TB_ICON;
  context->features |= F_TB_LABEL;
  context->tb_rows = -1;
  context->tb_rows = -1;
  context->pager_cols = -1;
  context->pager_rows = -1;
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

gpointer scanner_thread ( gpointer data )
{
  while ( TRUE )
  {
    scanner_expire();
    layout_widgets_update();
    usleep(100000);
  }
}

gint shell_timer ( gpointer data )
{
  layout_widgets_draw();

  if((context->features & F_TASKBAR)&&(context->status & ST_TASKBAR))
    taskbar_refresh();

  if(context->features & F_TRAY)
    sni_refresh();

  if(context->features & F_SWITCHER)
    switcher_update();

  return TRUE;
}

static void activate (GtkApplication* app, gpointer data )
{
  struct layout_widget *lw;

  context->window = (GtkWindow *)gtk_application_window_new (app);
  gtk_layer_init_for_window (context->window);
  gtk_layer_auto_exclusive_zone_enable (context->window);
  gtk_layer_set_keyboard_interactivity(context->window,FALSE);
  gtk_layer_set_layer(context->window,GTK_LAYER_SHELL_LAYER_OVERLAY);

  css_init();
  
  lw = config_parse(confname?confname:"sfwbar.config");

  if((lw != NULL)&&(lw->widget!=NULL))
  {
    gtk_container_add(GTK_CONTAINER(context->window), lw->widget);
    layout_widget_free(lw);

    gtk_widget_show_all ((GtkWidget *)context->window);
    if(context->features & F_TASKBAR)
     gtk_widget_set_size_request (GTK_WIDGET (context->window), -1, gtk_widget_get_allocated_height(context->box));
    gtk_window_resize (context->window, 1, 1);
  }

  if((context->features & F_TASKBAR)||(context->features & F_PLACEMENT)||(context->features & F_PAGER))
  {
   sway_ipc_init();
   if(context->ipc<0)
      wlr_ft_init();
  }

  if(context->widgets)
    g_thread_unref(g_thread_new("scanner",scanner_thread,NULL));


  g_timeout_add (100,(GSourceFunc )shell_timer,context);
  g_unix_signal_add(10,(GSourceFunc)switcher_event,NULL);
  g_unix_signal_add(12,(GSourceFunc)hide_event,NULL);
  gtk_widget_show_all ((GtkWidget *)context->window);
}

struct context *context;

int main (int argc, gchar **argv)
{
  GtkApplication *app;
  gint status;

  init_context();

  parse_command_line(argc,argv);

  app = gtk_application_new ("org.gtk.sfwbar", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), &context);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
