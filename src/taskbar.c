/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include "sfwbar.h"
#include "taskbaritem.h"
#include "taskbar.h"

G_DEFINE_TYPE_WITH_CODE (Taskbar, taskbar, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Taskbar));

static GList *taskbars;

static GtkWidget *taskbar_get_child ( GtkWidget *self )
{
  TaskbarPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR(self),NULL);
  priv = taskbar_get_instance_private(TASKBAR(self));

  return priv->taskbar;
}

static void taskbar_class_init ( TaskbarClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->get_child = taskbar_get_child;
}

static void taskbar_init ( Taskbar *self )
{
}

GtkWidget *taskbar_new ( void )
{
  GtkWidget *self;
  TaskbarPrivate *priv;

  self = GTK_WIDGET(g_object_new(taskbar_get_type(), NULL));
  priv = taskbar_get_instance_private(TASKBAR(self));

  priv->taskbar = flow_grid_new(TRUE);
  gtk_container_add(GTK_CONTAINER(self),priv->taskbar);
  taskbars = g_list_append(taskbars,self);

  flow_grid_invalidate(priv->taskbar);
  return self;
}

void taskbar_invalidate_all ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    taskbar_item_invalidate(flow_grid_find_child(iter->data,win));
}

void taskbar_init_item ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_item_new(win,iter->data);
}

void taskbar_populate ( void )
{
  GList *iter;

  if(!taskbars)
    return;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_init_item (iter->data);
}

void taskbar_destroy_item ( window_t *win )
{
  g_list_foreach(taskbars,(GFunc)flow_grid_delete_child,win);
}

void taskbar_update_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
    {
      flow_grid_update(iter->data);
      gtk_widget_show_all(iter->data);
    }
}
