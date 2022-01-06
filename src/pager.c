/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */

#include <gtk/gtk.h>
#include "sfwbar.h"

static GtkWidget *pager;
static GList *pager_pins;
static gboolean preview;

void pager_init ( GtkWidget *widget )
{
  if(!pager)
    pager = widget;
  pager_update();
}

void pager_set_preview ( gboolean pv )
{
  preview = pv;
}

void pager_add_pin ( gchar *pin )
{
  pager_pins = g_list_append(pager_pins,pin);
}

void pager_button_click( GtkWidget *widget, gpointer data )
{
  gchar *label;
  if(!sway_ipc_active())
    return;
  label = (gchar *)gtk_button_get_label(GTK_BUTTON(widget));
  if(label==NULL)
    return;
  sway_ipc_command("workspace '%s'",label);
}

gboolean pager_draw_preview ( GtkWidget *widget, cairo_t *cr, gchar *desk )
{
  guint w,h;
  GtkStyleContext *style;
  GdkRGBA fg;
  gint sock;
  gint32 etype;
  struct json_object *obj = NULL;
  struct json_object *iter,*fiter,*arr;
  gint i,j;
  struct rect wr,cw;
  gchar *response, *label;

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
  if(response!=NULL)
    obj = json_tokener_parse(response);

  if( (obj) && json_object_is_type(obj,json_type_array) )
  {
    for(i=0;i<json_object_array_length(obj);i++)
    {
      iter = json_object_array_get_idx(obj,i);
      label = json_string_by_name(iter,"name");
      wr = parse_rect(iter);
      if(g_strcmp0(desk,label)==0)
      {
        json_object_object_get_ex(iter,"floating_nodes",&arr);
        if(json_object_is_type(arr,json_type_array))
          for(j=0;j<json_object_array_length(arr);j++)
          {
            fiter = json_object_array_get_idx(arr,j);
            if(json_bool_by_name(fiter,"focused",FALSE))
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

  json_object_put(obj);
  g_free(response);
  return TRUE;
}

gboolean pager_draw_tooltip ( GtkWidget *widget, gint x, gint y,
    gboolean kbmode, GtkTooltip *tooltip, gpointer data )
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

void pager_update ( void )
{
  gint sock;
  gint32 etype;
  struct json_object *obj = NULL;
  struct json_object *iter;
  gint i;
  GList *wslist = NULL;
  GList *visible = NULL;
  GList *node;
  gchar *response;
  GtkWidget *widget;
  gchar *label;
  gchar *focus=NULL;

  if(!pager)
    return;

  sock=sway_ipc_open(3000);
  if(sock==-1)
    return;
  
  sway_ipc_send(sock,1,"");
  response = sway_ipc_poll(sock,&etype);
  close(sock);
  if(response!=NULL)
    obj = json_tokener_parse(response);

  if(obj && json_object_is_type(obj,json_type_array))
  {
    for(i=0;i<json_object_array_length(obj);i++)
      {
        iter = json_object_array_get_idx(obj,i);
        label = json_string_by_name(iter,"name");
        if(label!=NULL)
        {
          wslist = g_list_append(wslist,label);
          if(json_bool_by_name(iter,"visible",FALSE)==TRUE)
            visible = g_list_append(visible,label);
          if(json_bool_by_name(iter,"focused",FALSE)==TRUE)
            focus = label;
        }
      }
    json_object_put(obj);
  }

  for(node=pager_pins;node!=NULL;node=g_list_next(node))
    if(!g_list_find_custom(wslist,node->data,(GCompareFunc)g_strcmp0))
      wslist = g_list_append(wslist,g_strdup(node->data));
  wslist = g_list_sort(wslist,(GCompareFunc)g_strcmp0);

  if(wslist)
  {
    flow_grid_clean(pager);
    for(node=wslist;node!=NULL;node=g_list_next(node))
      {
        label = node->data;
        widget = gtk_button_new_with_label(label);
        if(preview)
        {
          gtk_widget_set_has_tooltip(widget,TRUE);
          g_signal_connect(widget,"query-tooltip",
              G_CALLBACK(pager_draw_tooltip),NULL);
        }
        gtk_widget_set_name(widget, "pager_normal");
        if(g_list_find_custom(visible,label,(GCompareFunc)g_strcmp0)!=NULL)
          gtk_widget_set_name(widget, "pager_visible");
        if(focus==label)
          gtk_widget_set_name(widget, "pager_focused");
        g_signal_connect(widget,"clicked",G_CALLBACK(pager_button_click),NULL);
        widget_set_css(widget);
        flow_grid_attach(pager,widget);
      }
    flow_grid_pad(pager);
    gtk_widget_show_all(pager);
  }

  g_list_free(visible);
  g_list_free_full(wslist,g_free);
  g_free(response);
}
