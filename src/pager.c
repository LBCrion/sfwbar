#include <gtk/gtk.h>
#include <json.h>
#include "sfwbar.h"

GtkWidget *pager_init ( struct context *context )
{
  context->pager = gtk_grid_new();
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
  char buff[256];
  gchar *label;
  label = (gchar *)gtk_button_get_label(GTK_BUTTON(widget));
  if(label==NULL)
    return;
  snprintf(buff,255,"workspace '%s'",label);
  ipc_send ( context->ipc, 0, buff );
}

void pager_update ( struct context *context )
{
  int i,c=0;
  int sock;
  gint32 etype;
  json_object *obj,*iter;
  GtkWidget *widget;
  gchar *label;

  sock=ipc_open(3000);
  if(sock==-1)
    return;
  
  ipc_send(sock,1,"");
  obj = ipc_poll(sock,&etype);
  close(sock);

  if(!json_object_is_type(obj, json_type_array))
    return;
  gtk_container_foreach(GTK_CONTAINER(context->pager),(GtkCallback)pager_remove_button,context);
  for(i=0;i<json_object_array_length(obj);i++)
    {
      iter = json_object_array_get_idx(obj,i);
      label = json_string_by_name(iter,"name");
      if(label!=NULL)
      {
        widget = gtk_button_new_with_label(label);
        g_free(label);
        gtk_widget_set_name(widget, "pager_normal");
        if(json_bool_by_name(iter,"visible",FALSE)==TRUE)
          gtk_widget_set_name(widget, "pager_visible");
        if(json_bool_by_name(iter,"focused",FALSE)==TRUE)
          gtk_widget_set_name(widget, "pager_focused");
        g_signal_connect(widget,"clicked",G_CALLBACK(pager_button_click),context);
        gtk_grid_attach(GTK_GRID(context->pager),widget, c/(context->pager_rows),
          c%(context->pager_rows),1,1);
        c++;
      }
    }
  gtk_widget_show_all(context->pager);
  json_object_put(obj);
}
