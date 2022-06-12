/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

static GList *taskbars;

void taskbar_invalidate ( GtkWidget *taskbar )
{
  g_object_set_data(G_OBJECT(taskbar),"invalid",GINT_TO_POINTER(TRUE));
}

void taskbar_invalidate_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_invalidate(((widget_t *)iter->data)->widget);
}

GtkWidget *taskbar_item_lookup ( GtkWidget *taskbar, void *parent )
{
  GList *iter;

  iter = g_object_get_data(G_OBJECT(taskbar),"items");
  for(;iter;iter=g_list_next(iter))
    if(taskbar_item_get_window(iter->data) == parent)
      return iter->data;

  return NULL;
}

void taskbar_item_init_for_all ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_item_new(win,((widget_t *)iter->data)->widget);
}

void taskbar_item_destroy ( GtkWidget *taskbar, window_t *win )
{
  GtkWidget *item;

  if(!taskbar)
    return;

  item = taskbar_item_lookup(taskbar,win);

  if(item)
    gtk_widget_destroy(item);
}

void taskbar_item_destroy_for_all ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_item_destroy(((widget_t *)iter->data)->widget, win );
}

void taskbar_update( GtkWidget *taskbar )
{
  GtkWidget *item;
  GList *iter;
  gchar *output;
  gboolean filter_output;

  g_return_if_fail(taskbar);

  if(!GPOINTER_TO_INT(g_object_get_data(
          G_OBJECT(taskbar),"invalid")))
    return;

  output = bar_get_output(taskbar);
  flow_grid_clean(taskbar);
  filter_output = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"filter_output"));
  iter = g_object_get_data(G_OBJECT(taskbar),"items");
  for (; iter; iter = g_list_next(iter) )
  {
    item = iter->data;
    if(item)
    {
      if( !filter_output || !!taskbar_item_get_window(item)->output ||
          g_strcmp0(taskbar_item_get_window(item)->output,output))
      {
        taskbar_item_update(item);
        flow_grid_attach(taskbar,item);
      }
    }
  }
  flow_grid_pad(taskbar);
  gtk_widget_show_all(taskbar);

  g_object_set_data(G_OBJECT(taskbar),"invalid", GINT_TO_POINTER(FALSE));
}

void taskbar_update_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_update(((widget_t *)iter->data)->widget);
}

void taskbar_init ( widget_t *lw )
{
  GList *iter;

  taskbars = g_list_append(taskbars,lw);
  g_object_set_data(G_OBJECT(lw->widget),"actions",lw->actions);

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_item_new(iter->data,lw->widget);

  taskbar_invalidate(lw->widget);
}
