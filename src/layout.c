/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include "sfwbar.h"
#include <gtk/gtk.h>

void widget_update_all( struct context *context )
{
  GList *iter;
  gint64 ctime = g_get_real_time();
  gchar *expr;

  for(iter=context->widgets;iter!=NULL;iter=g_list_next(iter))
  {
    gint64 *poll = g_object_get_data(G_OBJECT(iter->data),"next_poll");
    if(*poll <= ctime)
    {
      gint64 *freq = g_object_get_data(G_OBJECT(iter->data),"freq");
      *poll = ctime+(*freq);
      expr = g_object_get_data(G_OBJECT(iter->data),"expr");
      if(expr!=NULL)
      {
        gchar *eval=NULL;
        if(GTK_IS_LABEL(GTK_WIDGET(iter->data)))
          gtk_label_set_text(GTK_LABEL(iter->data),eval=parse_expr(context,expr));
        if(GTK_IS_PROGRESS_BAR(iter->data))
        {
          eval = parse_expr(context,expr);
          if(!g_strrstr(eval,"nan"))
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(iter->data),g_ascii_strtod(eval,NULL));
        }
        if(GTK_IS_IMAGE(iter->data))
          gtk_image_set_from_file(GTK_IMAGE(iter->data),eval=parse_expr(context,expr));
        g_free(eval);
      }
    }
  }
}

void layout_config_iter ( struct context *context, const ucl_object_t *obj, GtkWidget *parent )
{
  const ucl_object_t *ptr,*arr;
  ucl_object_iter_t *itp;
  gchar *type,*name,*expr,*dir,*css;
  GtkWidget *widget=NULL;
  gint64 freq;
  int x,y,w,h;

  type = ucl_string_by_name(obj,"type");

  if(type==NULL)
    return;

  if(g_strcmp0(type,"label")==0)
    widget = gtk_label_new("");
  if(g_strcmp0(type,"scale")==0)
    widget = gtk_progress_bar_new();
  if(g_strcmp0(type,"grid")==0)
    widget = gtk_grid_new();
  if(g_strcmp0(type,"image")==0)
    widget = gtk_image_new();
  if(g_ascii_strcasecmp(type,"button")==0)
  {
    GtkWidget *img;
    widget = gtk_button_new();
    ptr = ucl_object_lookup(obj,"icon");
    img = gtk_image_new_from_icon_name(ucl_object_tostring(ptr),GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(img),ucl_int_by_name(obj,"isize",48));
//    gtk_button_set_image(GTK_BUTTON(widget),img);
    gtk_container_add(GTK_CONTAINER(widget),img);
    g_signal_connect(widget,"clicked",G_CALLBACK(widget_action),NULL);
    g_object_set_data(G_OBJECT(widget),"action", ucl_string_by_name(obj,"action"));
  }
  if(g_strcmp0(type,"taskbar")==0)
  {
    if(ucl_bool_by_name(obj,"icon",TRUE))
      context->features |= F_TB_ICON;
    if(ucl_bool_by_name(obj,"title",TRUE))
      context->features |= F_TB_LABEL;
    if(!(context->features & F_TB_ICON))
      context->features |= F_TB_LABEL;
    context->tb_isize = ucl_int_by_name(obj,"isize",16);
    context->tb_rows = ucl_int_by_name(obj,"rows",1);
    if(context->tb_rows<1)
      context->tb_rows = 1;
    widget = taskbar_init(context);
  }
  if(g_strcmp0(type,"pager")==0)
  {
    context->pager_rows = ucl_int_by_name(obj,"rows",1);
    if(context->pager_rows<1)
      context->pager_rows = 1;
    widget = pager_init(context);
  }

  if(widget==NULL)
    return;

  name = ucl_string_by_name(obj,"style");
  if(name!=NULL)
    gtk_widget_set_name(widget,name);
  g_free(name);

  css = ucl_string_by_name(obj,"css");
  if(css!=NULL)
  {
    GtkStyleContext *context = gtk_widget_get_style_context (widget);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
    gtk_style_context_add_provider (context,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(css);
  }

  expr = ucl_string_by_name(obj,"value");
  if(expr!=NULL)
    g_object_set_data(G_OBJECT(widget),"expr",expr);

  freq = ucl_int_by_name(obj,"freq",0)*1000;
  g_object_set_data(G_OBJECT(widget),"freq",g_memdup(&freq,sizeof(freq)));
  g_object_set_data(G_OBJECT(widget),"next_poll",g_try_malloc0(sizeof(gint64)));

  if(GTK_IS_ORIENTABLE(widget))
  {
    dir = ucl_string_by_name(obj,"direction");
    if(g_strcmp0(dir,"vertical")==0)
    {
      gtk_orientable_set_orientation(GTK_ORIENTABLE(widget),GTK_ORIENTATION_VERTICAL);
      if(!g_strcmp0(type,"scale"))
        gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(widget),TRUE);
    }
    else
      gtk_orientable_set_orientation(GTK_ORIENTABLE(widget),GTK_ORIENTATION_HORIZONTAL);
    g_free(dir);
  }
  else
  {
    ptr = ucl_object_lookup(obj,"xalign");
    if(ptr!=NULL)
      gtk_label_set_xalign(GTK_LABEL(widget),ucl_object_todouble(ptr));
  }

  x = ucl_int_by_name(obj,"x",0);
  y = ucl_int_by_name(obj,"y",0);
  w = ucl_int_by_name(obj,"w",1);
  h = ucl_int_by_name(obj,"h",1);
  if(w<1)
    w=1;
  if(h<1)
    h=1;
  gtk_widget_set_hexpand(GTK_WIDGET(widget),ucl_bool_by_name(obj,"expand",FALSE));
  if((x<1)||(y<1))
    gtk_container_add(GTK_CONTAINER(parent),widget);
  else
    gtk_grid_attach(GTK_GRID(parent),widget,x,y,w,h);

  context->widgets = g_list_append(context->widgets,widget);

  if(g_strcmp0(type,"grid")==0)
  {
    arr = ucl_object_lookup(obj,"children");
    if( arr )
    {
      itp = ucl_object_iterate_new(arr);
      while((ptr = ucl_object_iterate_safe(itp,true))!=NULL)
        layout_config_iter(context,ptr,widget);
      ucl_object_iterate_free(itp);
    }
  }
  g_free(type);
}

GtkWidget *layout_init ( struct context *context, const ucl_object_t *obj )
{
  GtkWidget *grid;
  const ucl_object_t *arr,*ptr;
  ucl_object_iter_t *itp;

  arr = ucl_object_lookup(obj,"layout");
  if(arr==NULL)
    return NULL;

  grid = gtk_grid_new();
  gtk_widget_set_name(grid,"layout");

  itp = ucl_object_iterate_new(arr);
  while((ptr = ucl_object_iterate_safe(itp,true))!=NULL)
    layout_config_iter(context,ptr,grid);

  ucl_object_iterate_free(itp);

  return grid;
}

void widget_action ( GtkWidget *widget, gpointer data )
{
  gchar *cmd;
  gchar *argv[] = {NULL,NULL};
  GPid pid;
  cmd = g_object_get_data(G_OBJECT(widget),"action");
  if(cmd!=NULL)
  {
    argv[0]=cmd;
    g_spawn_async(NULL,argv,NULL,G_SPAWN_DEFAULT,NULL,NULL,&pid,NULL);
  }
}
