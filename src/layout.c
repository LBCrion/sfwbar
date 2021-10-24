/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include "sfwbar.h"
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

void widget_update_all( void )
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
      if((expr!=NULL)&&(GTK_IS_LABEL(iter->data)||GTK_IS_PROGRESS_BAR(iter->data)||GTK_IS_IMAGE(iter->data)))
      {
        gchar *eval = expr_parse(expr);

        if(GTK_IS_LABEL(GTK_WIDGET(iter->data)))
          gtk_label_set_text(GTK_LABEL(iter->data),eval);
        if(GTK_IS_PROGRESS_BAR(iter->data))
          if(!g_strrstr(eval,"nan"))
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(iter->data),g_ascii_strtod(eval,NULL));
        if(GTK_IS_IMAGE(iter->data))
        {
          scale_image_set_image(GTK_WIDGET(iter->data),eval);
          scale_image_update(GTK_WIDGET(iter->data));
        }
        g_free(eval);
      }
    }
  }
}

GtkWidget *layout_config_include ( gchar *fname, GtkWidget *parent, GtkWidget *sibling )
{
  const gchar *json;
  struct ucl_parser *uparse;
  const ucl_object_t *obj;
  gchar *fullname;
  GtkWidget *ret;

  fullname = get_xdg_config_file(fname);
  if(fullname==NULL)
    return sibling;
  uparse = ucl_parser_new(0);
  ucl_parser_add_file(uparse,fullname);
  obj = ucl_parser_get_object(uparse);
  g_free(fullname);
  json = ucl_parser_get_error(uparse);
  if(json!=NULL)
    printf("%s\n",json);
  scanner_init(obj);
  ret = layout_init(obj,parent,sibling);

  ucl_object_unref((ucl_object_t *)obj);
  ucl_parser_free(uparse);
  return ret;
}

GtkWidget *layout_config_iter ( const ucl_object_t *obj,
    GtkWidget *parent, GtkWidget *sibling )
{
  const ucl_object_t *ptr,*arr;
  ucl_object_iter_t *itp;
  gchar *type,*name,*expr,*css;
  gint dir;
  GtkWidget *widget=NULL;
  gint64 freq;
  gint x,y,w,h;

  if(ucl_object_type(obj)==UCL_STRING)
    return layout_config_include((char *)ucl_object_tostring_forced(obj),parent,sibling);

  type = ucl_string_by_name(obj,"type");

  if(type==NULL)
    return sibling;

  if(g_ascii_strcasecmp(type,"label")==0)
    widget = gtk_label_new("");
  if(g_ascii_strcasecmp(type,"scale")==0)
    widget = gtk_progress_bar_new();
  if(g_ascii_strcasecmp(type,"grid")==0)
    widget = gtk_grid_new();
  if(g_ascii_strcasecmp(type,"image")==0)
    widget = scale_image_new();
  if(g_ascii_strcasecmp(type,"button")==0)
  {
    widget = gtk_button_new();
    g_signal_connect(widget,"clicked",G_CALLBACK(widget_action),NULL);
    g_object_set_data(G_OBJECT(widget),"action", ucl_string_by_name(obj,"action"));
  }
  if(g_ascii_strcasecmp(type,"taskbar")==0)
  {
    if(ucl_bool_by_name(obj,"icon",TRUE))
      context->features |= F_TB_ICON;
    if(ucl_bool_by_name(obj,"title",TRUE))
      context->features |= F_TB_LABEL;
    if(!(context->features & F_TB_ICON))
      context->features |= F_TB_LABEL;
    context->tb_rows = ucl_int_by_name(obj,"rows",-1);
    context->tb_cols = ucl_int_by_name(obj,"cols",-1);
    if((context->tb_rows<1)&&(context->tb_cols<1))
      context->tb_rows = 1;
    if((context->tb_rows>0)&&(context->tb_cols>0))
      context->tb_cols = -1;
    widget = taskbar_init();
  }
  if(g_ascii_strcasecmp(type,"tray")==0)
    widget = sni_init();
  if(g_ascii_strcasecmp(type,"pager")==0)
  {
    context->pager_rows = ucl_int_by_name(obj,"rows",-1);
    context->pager_cols = ucl_int_by_name(obj,"cols",-1);
    if((context->pager_rows<1)&&(context->pager_cols<1))
      context->pager_rows = 1;
    if((context->pager_rows>0)&&(context->pager_cols>0))
      context->pager_cols = -1;
    if(ucl_bool_by_name(obj,"preview",FALSE)==TRUE)
      context->features |= F_PA_RENDER;
    context->pager_pins = NULL;
    arr = ucl_object_lookup(obj,"pins");
    if( arr )
    {
      itp = ucl_object_iterate_new(arr);
      while((ptr = ucl_object_iterate_safe(itp,true))!=NULL)
      {
        gchar *pin = (char *)ucl_object_tostring(ptr);
        if(pin!=NULL)
          context->pager_pins = g_list_append(context->pager_pins,g_strdup(pin));
      }
      ucl_object_iterate_free(itp);
    }
    widget = pager_init();
  }

  if(widget==NULL)
    return sibling;

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

  if(GTK_IS_PROGRESS_BAR(widget))
  {
    gtk_widget_style_get(widget,"direction",&dir,NULL);
    if((dir==GTK_POS_LEFT)||(dir==GTK_POS_RIGHT))
      gtk_orientable_set_orientation(GTK_ORIENTABLE(widget),GTK_ORIENTATION_HORIZONTAL);
    else
      gtk_orientable_set_orientation(GTK_ORIENTABLE(widget),GTK_ORIENTATION_VERTICAL);
    if((dir==GTK_POS_TOP)||(dir==GTK_POS_LEFT))
      gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(widget),TRUE);
    else
      gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(widget),FALSE);
  }

  if(GTK_IS_BUTTON(widget))
  {
    GtkWidget *img;
    ptr = ucl_object_lookup(obj,"icon");
    img = scale_image_new();
    scale_image_set_image(img,(gchar *)ucl_object_tostring(ptr));
    gtk_container_add(GTK_CONTAINER(widget),img);
  }

  if(GTK_IS_LABEL(widget))
  {
    gdouble xalign;
    gtk_widget_style_get(widget,"align",&xalign,NULL);
    gtk_label_set_xalign(GTK_LABEL(widget),xalign);
  }

  widget_set_css(widget);
  x = ucl_int_by_name(obj,"x",0);
  y = ucl_int_by_name(obj,"y",0);
  w = ucl_int_by_name(obj,"w",1);
  h = ucl_int_by_name(obj,"h",1);
  if(w<1)
    w=1;
  if(h<1)
    h=1;
  if((x<1)||(y<1))
  {
    gtk_widget_style_get(parent,"direction",&dir,NULL);
    gtk_grid_attach_next_to(GTK_GRID(parent),widget,sibling,dir,1,1);
  }
  else
    gtk_grid_attach(GTK_GRID(parent),widget,x,y,w,h);

  context->widgets = g_list_append(context->widgets,widget);

  if(GTK_IS_GRID(widget))
  {
    arr = ucl_object_lookup(obj,"children");
    if( arr )
    {
      GtkWidget *sibling = NULL;
      itp = ucl_object_iterate_new(arr);
      while((ptr = ucl_object_iterate_safe(itp,true))!=NULL)
        sibling = layout_config_iter(ptr,widget,sibling);
      ucl_object_iterate_free(itp);
    }
  }
  g_free(type);
  return widget;
}

GtkWidget *layout_init ( const ucl_object_t *obj, GtkWidget *parent, GtkWidget *sibling )
{
  const ucl_object_t *arr,*ptr;
  ucl_object_iter_t *itp;

  arr = ucl_object_lookup(obj,"layout");
  if(arr==NULL)
    return NULL;

  itp = ucl_object_iterate_new(arr);
  while((ptr = ucl_object_iterate_safe(itp,true))!=NULL)
    sibling = layout_config_iter(ptr,parent,sibling);

  ucl_object_iterate_free(itp);

  return sibling;
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
    g_spawn_async(NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,&pid,NULL);
  }
}

void widget_set_css ( GtkWidget *widget )
{
  gboolean expand;
  GList *l;
  gtk_widget_style_get(widget,"hexpand",&expand,NULL);
  gtk_widget_set_hexpand(GTK_WIDGET(widget),expand);
  gtk_widget_style_get(widget,"vexpand",&expand,NULL);
  gtk_widget_set_vexpand(GTK_WIDGET(widget),expand);
  if(GTK_IS_CONTAINER(widget))
  {
    l = gtk_container_get_children(GTK_CONTAINER(widget));
    for(;l!=NULL;l=g_list_next(l))
      widget_set_css(l->data);
  }

}
