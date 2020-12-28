/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include "sfwbar.h"
#include <json.h>
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

void layout_config_iter ( struct context *context, json_object *obj, GtkWidget *parent )
{
  json_object *ptr,*arr;
  gchar *type,*name,*expr,*dir;
  GtkWidget *widget=NULL;
  gint64 freq;
  int x,y,w,h,i;

  json_object_object_get_ex(obj,"type",&ptr);
  type = g_strdup(json_object_get_string(ptr));

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
  if(g_strcmp0(type,"button")==0)
  {
    GtkWidget *img;
    widget = gtk_button_new();
    json_object_object_get_ex(obj,"icon",&ptr);
    img = gtk_image_new_from_icon_name(json_object_get_string(ptr),GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(img),json_int_by_name(obj,"isize",48));
    gtk_button_set_image(GTK_BUTTON(widget),img);
    g_signal_connect(widget,"clicked",G_CALLBACK(widget_action),NULL);
    g_object_set_data(G_OBJECT(widget),"action", json_string_by_name(obj,"action"));
  }
  if(g_strcmp0(type,"taskbar")==0)
  {
    if(json_bool_by_name(obj,"icon",TRUE))
      context->features |= F_TB_ICON;
    if(json_bool_by_name(obj,"title",TRUE))
      context->features |= F_TB_LABEL;
    if(!(context->features & F_TB_ICON))
      context->features |= F_TB_LABEL;
    context->tb_isize = json_int_by_name(obj,"isize",16);
    context->tb_rows = json_int_by_name(obj,"rows",1);
    if(context->tb_rows<1)
      context->tb_rows = 1;
    widget = taskbar_init(context);
  }
  if(g_strcmp0(type,"pager")==0)
  {
    context->pager_rows = json_int_by_name(obj,"rows",1);
    if(context->pager_rows<1)
      context->pager_rows = 1;
    widget = pager_init(context);
  }

  if(widget==NULL)
    return;

  name = json_string_by_name(obj,"style");
  if(name!=NULL)
    gtk_widget_set_name(widget,name);
  g_free(name);

  expr = json_string_by_name(obj,"value");
  if(expr!=NULL)
    g_object_set_data(G_OBJECT(widget),"expr",expr);

  freq = json_int_by_name(obj,"freq",0)*1000;
  g_object_set_data(G_OBJECT(widget),"freq",g_memdup(&freq,sizeof(freq)));
  g_object_set_data(G_OBJECT(widget),"next_poll",g_try_malloc0(sizeof(gint64)));

  if(GTK_IS_ORIENTABLE(widget))
  {
    dir = json_string_by_name(obj,"direction");
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
    json_object_object_get_ex(obj,"xalign",&ptr);
    if(ptr!=NULL)
      gtk_label_set_xalign(GTK_LABEL(widget),json_object_get_double(ptr));
  }

  json_object_object_get_ex(obj,"x",&ptr);
  x = json_object_get_int(ptr);
  json_object_object_get_ex(obj,"y",&ptr);
  y = json_object_get_int(ptr);
  json_object_object_get_ex(obj,"w",&ptr);
  w = json_object_get_int(ptr);
  json_object_object_get_ex(obj,"h",&ptr);
  h = json_object_get_int(ptr);
  if(w<1)
    w=1;
  if(h<1)
    h=1;
  gtk_widget_set_hexpand(GTK_WIDGET(widget),json_bool_by_name(obj,"expand",FALSE));
  if((x<1)||(y<1))
    gtk_container_add(GTK_CONTAINER(parent),widget);
  else
    gtk_grid_attach(GTK_GRID(parent),widget,x,y,w,h);

  context->widgets = g_list_append(context->widgets,widget);

  if(g_strcmp0(type,"grid")==0)
  {
    json_object_object_get_ex(obj,"children",&arr);
    if( json_object_is_type(arr, json_type_array))
      for(i=0;i<json_object_array_length(arr);i++)
      {
      ptr = json_object_array_get_idx(arr,i);
      layout_config_iter(context,ptr,widget);
      }
  }

  g_free(type);
}

GtkWidget *layout_init ( struct context *context, json_object *obj )
{
  GtkWidget *grid;
  json_object *arr;
  int i;

  json_object_object_get_ex(obj,"layout",&arr);
  if(!json_object_is_type(arr, json_type_array))
    return NULL;

  grid = gtk_grid_new();
  gtk_widget_set_name(grid,"layout");

  for(i=0;i<json_object_array_length(arr);i++)
    layout_config_iter(context,json_object_array_get_idx(arr,i),grid);

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
