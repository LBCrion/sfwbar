/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

gchar *confname=NULL, *cssname=NULL, *sockname=NULL;
static GOptionEntry entries[] = {
  {"config",'f',0,G_OPTION_ARG_FILENAME,&confname,"Specify config file"},
  {"css",'c',0,G_OPTION_ARG_FILENAME,&cssname,"Specify css file"},
  {"socket",'s',0,G_OPTION_ARG_FILENAME,&sockname,"Specify sway socket file"},
  {NULL}};

void parse_command_line ( struct context *context, int argc, char **argv)
{
  GOptionContext *optc;
  optc = g_option_context_new(" - Sway Floating Window Bar");
  g_option_context_add_main_entries(optc,entries,NULL);
  g_option_context_add_group (optc, gtk_get_option_group (TRUE));
  g_option_context_parse(optc,&argc,&argv,NULL);
}

void dispatch_event ( struct ipc_event *ev, struct context *context )
{
  if ((ev->event == 0)&&(context->features & F_PLACEMENT))
    place_window(ev->wid, ev->pid, context);

  if(context->features & (F_TASKBAR | F_SWITCHER | F_PLACEMENT))
  {
    if (ev->event == 99)
      ev->event = 0;
    if (ev->event == 0 || ev->event == 3)
      taskbar_update_window (ev,context);
    if (ev->event == 1)
      taskbar_delete_window(ev->wid,context);
    if (ev->event == 2)
      context->tb_focus = ev->wid;
  }

  if(context->features & F_TASKBAR)
    taskbar_refresh(context);

  g_free(ev->title);
  g_free(ev->appid);
}

void init_context ( struct context *context )
{
  context->scan_list=NULL;
  context->file_list=NULL;
  context->widgets=NULL;
  context->buttons = NULL;
  context->features=0;
  context->default_dec=4;
  context->buff_len = 1024;
  context->read_buff = g_malloc(context->buff_len);
}

void placement_init ( struct context *context, const ucl_object_t *obj )
{
  const ucl_object_t *ptr;
  if((ptr=ucl_object_lookup(obj,"placement"))==NULL)
    return;
  context->features |= F_PLACEMENT;
  context->wp_x= ucl_int_by_name(ptr,"xcascade",10);
  context->wp_y= ucl_int_by_name(ptr,"ycascade",10);
  if(context->wp_x<1)
    context->wp_x=1;
  if(context->wp_y<1)
    context->wp_y=1;
}

void switcher_init (struct context *context, const ucl_object_t *obj )
{
  const ucl_object_t *ptr;
  if((ptr=ucl_object_lookup(obj,"switcher"))==NULL)
    return;
  context->features |= F_SWITCHER;
  context->sw_max = ucl_int_by_name(ptr,"delay",1)*10;
  context->sw_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  context->sw_box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(context->sw_win),context->sw_box);
}

void css_init ( void )
{
  GtkCssProvider *css;
  gchar *cssf;
  GtkWidgetClass *widget_class = g_type_class_ref(GTK_TYPE_WIDGET);

  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_double("align","text alignment","text alignment",
      0.0,1.0,0.0, G_PARAM_READABLE));

  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boolean("hexpand","hotizonal expansion","horizontal expansion",
       false, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boolean("vexpand","vertical expansion","vertical expansion",
      false, G_PARAM_READABLE));
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

  if(cssname!=NULL)
    cssf=g_strdup(cssname);
  else
    cssf = get_xdg_config_file("sfwbar.css");
  if(cssf!=NULL)
  {
    css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css,cssf,NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
      GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(cssf);
  }
}

GtkWidget *load_config ( struct context *context )
{
  const gchar *json;
  gchar *fname;
  struct ucl_parser *uparse;
  const ucl_object_t *obj;
  GtkWidget *root;
  
  if(confname!=NULL)
    fname = g_strdup(confname);
  else
    fname = get_xdg_config_file("sfwbar.config");
  uparse = ucl_parser_new(0);
  ucl_parser_add_file(uparse,fname);
  obj = ucl_parser_get_object(uparse);
  g_free(fname);
  json = ucl_parser_get_error(uparse);
  if(json!=NULL)
    printf("%s\n",json);
  
  css_init();
  placement_init(context,obj);
  switcher_init(context,obj);
  scanner_init(context,obj);
  root = layout_init(context,obj);

  ucl_object_unref((ucl_object_t *)obj);
  ucl_parser_free(uparse);
  return root;
}

gint shell_timer ( struct context *context )
{
  const ucl_object_t *obj;
  struct ucl_parser *parse;
  gchar *response;
  struct ipc_event ev;
  gint32 etype;

  scanner_expire(context->scan_list);
  widget_update_all(context);
  if(context->ipc!=-1)
  {
    response = ipc_poll(context->ipc,&etype);
    while (response != NULL)
    { 
      parse = ucl_parser_new(0);
      ucl_parser_add_string(parse,response,strlen(response));
      obj = ucl_parser_get_object(parse);
      if(obj!=NULL)
      {
        ev = ipc_parse_event(obj);
        if(etype==0x80000003)
          dispatch_event(&ev,context);
        if(etype==0x80000000)
          pager_update(context);
        if(etype==0x80000004)
          switcher_event(context,obj);
      }
      ucl_object_unref((ucl_object_t *)obj);
      ucl_parser_free(parse);
      g_free(response);
      response = ipc_poll(context->ipc,&etype);
    }
  }

  if(context->features & F_SWITCHER)
    switcher_update(context);

  return TRUE;
}

static void activate (GtkApplication* app, struct context *context)
{
//  GtkWindow *window;
  GtkWidget *root;

  context->window = (GtkWindow *)gtk_application_window_new (app);
  gtk_layer_init_for_window (context->window);
  gtk_layer_auto_exclusive_zone_enable (context->window);
  gtk_layer_set_keyboard_interactivity(context->window,FALSE);
  
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_LEFT,TRUE);
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_RIGHT,TRUE);
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_BOTTOM,TRUE);
  gtk_layer_set_anchor (context->window,GTK_LAYER_SHELL_EDGE_TOP,FALSE);
  
  root = load_config(context);

  if(root != NULL)
  {
    gtk_container_add(GTK_CONTAINER(context->window), root);

    gtk_widget_show_all ((GtkWidget *)context->window);
    gtk_widget_set_size_request (GTK_WIDGET (context->window), -1, gtk_widget_get_allocated_height(context->box));
    gtk_window_resize (context->window, 1, 1);
  }

  if((context->features & F_TASKBAR)||(context->features & F_SWITCHER))
    taskbar_populate(context);

  if((context->features & F_TASKBAR)||(context->features & F_PLACEMENT)||(context->features & F_PAGER))
  {
    context->ipc = ipc_open(10);
    if(context->ipc>=0)
      ipc_subscribe(context->ipc);
  }

  g_timeout_add (100,(GSourceFunc )shell_timer,context);
  gtk_widget_show_all ((GtkWidget *)context->window);
}


int main (int argc, char **argv)
{
  GtkApplication *app;
  int status;
  struct context context;

  init_context(&context);

  parse_command_line(&context,argc,argv);

  app = gtk_application_new ("org.gtk.sfwbar", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), &context);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
