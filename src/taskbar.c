/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "taskbaritem.h"
#include "taskbargroup.h"
#include "taskbar.h"
#include "wintree.h"

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

GtkWidget *taskbar_new ( gboolean toplevel )
{
  GtkWidget *self;
  TaskbarPrivate *priv;

  self = GTK_WIDGET(g_object_new(taskbar_get_type(), NULL));
  priv = taskbar_get_instance_private(TASKBAR(self));

  priv->taskbar = flow_grid_new(TRUE);
  gtk_container_add(GTK_CONTAINER(self),priv->taskbar);
  priv->toplevel = toplevel;
  taskbars = g_list_append(taskbars,self);

  flow_grid_invalidate(priv->taskbar);
  return self;
}

gboolean taskbar_is_toplevel ( GtkWidget *self )
{
  TaskbarPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR(self),FALSE);
  priv = taskbar_get_instance_private(TASKBAR(self));

  return priv->toplevel;
}

void taskbar_invalidate_all ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
  {
    taskbar_item_invalidate(flow_grid_find_child(iter->data,win));
    taskbar_group_invalidate(flow_grid_find_child(iter->data,win->appid));
  }
}

void taskbar_init_item ( window_t *win )
{
  GList *iter;
  GtkWidget *taskbar;
  gboolean group;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data && taskbar_is_toplevel(iter->data))
    {
      taskbar = iter->data;
      group = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(taskbar),"group"));
      if(group)
        taskbar = taskbar_group_new(win->appid,taskbar);

      if(!flow_grid_find_child(taskbar,win))
        taskbar_item_new(win,taskbar);
    }
}

void taskbar_populate ( void )
{
  GList *iter;

  if(!taskbars)
    return;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_init_item (iter->data);
}

void taskbar_reparent_item ( window_t *win, const gchar *new_appid )
{
  GList *iter;
  GtkWidget *taskbar, *group;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data && taskbar_is_toplevel(iter->data))
    {
      taskbar = iter->data;
      if(!GPOINTER_TO_INT(
            g_object_get_data(G_OBJECT(taskbar),"group")))
        break;
      group = taskbar_group_new(win->appid,taskbar);
      flow_grid_delete_child(group,win);
      if(!flow_grid_n_children(group))
        flow_grid_delete_child(taskbar, win->appid);
      group = taskbar_group_new(new_appid,taskbar);
      taskbar_item_new(win,group);
    }
}

void taskbar_destroy_item ( window_t *win )
{
  GList *iter;
  GtkWidget *taskbar, *tgroup;
  gboolean group;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data && taskbar_is_toplevel(iter->data))
    {
      taskbar = iter->data;
      group = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(taskbar),"group"));
      if(group)
        tgroup = taskbar_group_new(win->appid,taskbar);
      else
        tgroup = taskbar;
      flow_grid_delete_child(tgroup,win);
      if(!flow_grid_n_children(tgroup))
        flow_grid_delete_child(taskbar, win->appid);
    }
}

void taskbar_update_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    flow_grid_update(iter->data);
}
