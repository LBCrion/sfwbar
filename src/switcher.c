/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- Lev Babiev
 */

#include <glib.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

void switcher_event ( struct context *context, const ucl_object_t *obj )
{
  char *mode;
  GList *item, *focus;
  char buff[256];

  if(obj==NULL)
    return;

  mode = ucl_string_by_name(obj,"mode");
  if(mode!=NULL)
  {
    if(*mode=='h')
      gtk_widget_hide(GTK_WIDGET(context->window));
    else
      gtk_widget_show(GTK_WIDGET(context->window));
    g_free(mode);
  }

  if(!(context->features & F_SWITCHER))
    return;

  mode = ucl_string_by_name(obj,"hidden_state");
  if(mode!=NULL)
  {
    if(*mode!=context->sw_hstate)
    {
      context->sw_hstate = *mode;
      context->sw_count = context->sw_max;
      focus = NULL;
      for (item = context->buttons; item!= NULL; item = g_list_next(item) )
        if (AS_BUTTON(item->data)->wid == context->tb_focus)
          focus = g_list_next(item);
      if(focus==NULL)
        focus=context->buttons;
      if(focus!=NULL)
      {
        snprintf(buff,255,"[con_id=%ld] focus",AS_BUTTON(focus->data)->wid);
        ipc_send ( context->ipc, 0, buff );
      }
    }
    g_free(mode);
  }
}

void switcher_delete ( GtkWidget *w, gpointer data )
{
  gtk_widget_destroy(w);
}


void switcher_update ( struct context *context )
{
  GList *item;
  GtkWidget *box;
  int i = 1;
  if(context->sw_count <= 0)
    return;
  context->sw_count--;

  if(context->sw_count > 0)
  {
    gtk_container_foreach(GTK_CONTAINER(context->sw_box),(GtkCallback)switcher_delete,NULL);
    for (item = context->buttons; item!= NULL; item = g_list_next(item) )
    {
      box = gtk_grid_new();
      gtk_grid_attach(GTK_GRID(context->sw_box),box,1,i,1,1);
      gtk_grid_attach(GTK_GRID(box),gtk_label_new(AS_BUTTON(item->data)->title),2,1,1,1);
      gtk_grid_attach(GTK_GRID(box),gtk_image_new_from_icon_name(AS_BUTTON(item->data)->appid,
            GTK_ICON_SIZE_SMALL_TOOLBAR),1,1,1,1);
//      if (AS_BUTTON(item->data)->pid == context->tb_focus)
//        gtk_widget_set_name(box, "switcher_active");
//      else
//        gtk_widget_set_name(box, "switcher_normal");

      i++;
    }
    gtk_widget_show_all(GTK_WIDGET(context->sw_win));
  }
  else
    gtk_widget_hide(GTK_WIDGET(context->sw_win));
}


