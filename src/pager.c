/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */


#include <gtk/gtk.h>
#include "sfwbar.h"

GtkWidget *pager_init ( struct context *context )
{
  context->pager = gtk_grid_new();
  gtk_widget_set_name(context->pager, "pager");
  context->features |= F_PAGER;
  pager_update(context);
  return context->pager;
}

void pager_remove_button ( GtkWidget *widget, struct context *context )
{
  gtk_container_remove ( GTK_CONTAINER(context->pager), widget );
}

void pager_button_click( GtkWidget *widget, struct context *context )
{
  gchar *cmd;
  gchar *label;
  if(context->ipc==-1)
    return;
  label = (gchar *)gtk_button_get_label(GTK_BUTTON(widget));
  if(label==NULL)
    return;
  cmd = g_strdup_printf("workspace '%s'",label);
  sway_ipc_send ( context->ipc, 0, cmd );
  g_free( cmd );
}

gboolean pager_draw_preview ( GtkWidget *widget, cairo_t *cr, gchar *desk )
{
  guint w,h;
  GtkStyleContext *style;
  GdkRGBA fg;
  gint sock;
  gint32 etype;
  const ucl_object_t *obj,*iter,*fiter,*arr;
  ucl_object_iter_t *itp,*fitp;
  struct ucl_parser *parse;
  struct rect wr,cw;
  gchar *response,*label;

  w = gtk_widget_get_allocated_width (widget);
  h = gtk_widget_get_allocated_height (widget);
  style = gtk_widget_get_style_context(widget);
  gtk_style_context_get_color (style,GTK_STATE_FLAG_NORMAL, &fg);
  cairo_set_line_width(cr,1);

  sock=sway_ipc_open(3000);
  if(sock==-1)
    return FALSE;
  
  sway_ipc_send(sock,1,"");
  response = sway_ipc_poll(sock,&etype);
  close(sock);
  parse = ucl_parser_new(0);
  if(response!=NULL)
    ucl_parser_add_string(parse,response,strlen(response));
  obj = ucl_parser_get_object(parse);

  if(obj)
  {
    itp = ucl_object_iterate_new(obj);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
    {
      label = ucl_string_by_name(iter,"name");
      wr = parse_rect(iter);
      if(g_strcmp0(desk,label)==0)
      {
        arr = ucl_object_lookup(iter,"floating_nodes");
        fitp = ucl_object_iterate_new(arr);
        while((fiter = ucl_object_iterate_safe(fitp,true))!=NULL)
        {
          if(ucl_bool_by_name(fiter,"focused",FALSE))
            cairo_set_source_rgba(cr,fg.red,fg.blue,fg.green,1);
          else
            cairo_set_source_rgba(cr,fg.red,fg.blue,fg.green,0.5);
          cw = parse_rect(fiter);
          cairo_rectangle(cr,
              (int)(cw.x*w/wr.w),
              (int)(cw.y*h/wr.h),
              (int)(cw.w*w/wr.w),
              (int)(cw.h*h/wr.h));
          cairo_fill(cr);
          gtk_render_frame(style,cr,
              (int)(cw.x*w/wr.w),
              (int)(cw.y*h/wr.h),
              (int)(cw.w*w/wr.w),
              (int)(cw.h*h/wr.h));
          cairo_stroke(cr);
        }
      }
      g_free(label);
    }
  }

  ucl_parser_free(parse);
  g_free(response);
  return TRUE;
}

gboolean pager_draw_tooltip ( GtkWidget *widget, gint x, gint y, gboolean kbmode, 
    GtkTooltip *tooltip, struct context *context )
{
  GtkWidget *button;
  gchar *desk;

  desk = (gchar *)gtk_button_get_label(GTK_BUTTON(widget));
  button = gtk_button_new();
  g_signal_connect(button,"draw",G_CALLBACK(pager_draw_preview),desk);
  gtk_widget_set_name(button, "pager_preview");
  gtk_tooltip_set_custom(tooltip,button);
  return TRUE;
}

void pager_update ( struct context *context )
{
  gint c=0;
  gint sock;
  gint32 etype;
  const ucl_object_t *obj,*iter;
  struct ucl_parser *parse;
  GList *wslist = NULL;
  GList *visible = NULL;
  GList *node;
  gchar *response;
  ucl_object_iter_t *itp;
  GtkWidget *widget;
  gchar *label;
  gchar *focus=NULL;

  sock=sway_ipc_open(3000);
  if(sock==-1)
    return;
  
  sway_ipc_send(sock,1,"");
  response = sway_ipc_poll(sock,&etype);
  close(sock);
  parse = ucl_parser_new(0);
  if(response!=NULL)
    ucl_parser_add_string(parse,response,strlen(response));
  obj = ucl_parser_get_object(parse);

  if(obj)
  {
    itp = ucl_object_iterate_new(obj);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
      {
        label = ucl_string_by_name(iter,"name");
        if(label!=NULL)
        {
          wslist = g_list_append(wslist,label);
          if(ucl_bool_by_name(iter,"visible",FALSE)==TRUE)
            visible = g_list_append(visible,label);
          if(ucl_bool_by_name(iter,"focused",FALSE)==TRUE)
            focus = label;
        }
      }
    ucl_object_unref((ucl_object_t *)obj);
  }

  for(node=context->pager_pins;node!=NULL;node=g_list_next(node))
    if(g_list_find_custom(wslist,node->data,(int (*)(const void *, const void *))g_strcmp0)==NULL)
      wslist = g_list_append(wslist,g_strdup(node->data));
  wslist = g_list_sort(wslist,(int (*)(const void *, const void *))g_strcmp0);

  if(wslist)
  {
    gtk_container_foreach(GTK_CONTAINER(context->pager),(GtkCallback)pager_remove_button,context);
    for(node=wslist;node!=NULL;node=g_list_next(node))
      {
        label = node->data;
        widget = gtk_button_new_with_label(label);
        if(context->features & F_PA_RENDER)
        {
          gtk_widget_set_has_tooltip(widget,TRUE);
          g_signal_connect(widget,"query-tooltip",G_CALLBACK(pager_draw_tooltip),context);
        }
        gtk_widget_set_name(widget, "pager_normal");
        if(g_list_find_custom(visible,label,(int (*)(const void *, const void *))g_strcmp0)!=NULL)
          gtk_widget_set_name(widget, "pager_visible");
        if(focus==label)
          gtk_widget_set_name(widget, "pager_focused");
        g_signal_connect(widget,"clicked",G_CALLBACK(pager_button_click),context);
        widget_set_css(widget);
        if(context->pager_rows>0)
          gtk_grid_attach(GTK_GRID(context->pager),widget, c/(context->pager_rows),
            c%(context->pager_rows),1,1);
        else
          gtk_grid_attach(GTK_GRID(context->pager),widget, c%(context->pager_cols),
            c/(context->pager_cols),1,1);
        c++;
      }
    gtk_widget_show_all(context->pager);
  }

  g_list_free(visible);
  g_list_free_full(wslist,g_free);
  ucl_parser_free(parse);
  g_free(response);
}
