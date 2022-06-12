/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

static GList *taskbars;

void taskbar_invalidate_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      flow_grid_invalidate(((widget_t *)iter->data)->widget);
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
  flow_grid_delete_child(taskbar,win);
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
  g_return_if_fail(taskbar);

  flow_grid_update(taskbar);
  gtk_widget_show_all(taskbar);
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

  flow_grid_invalidate(lw->widget);
}
