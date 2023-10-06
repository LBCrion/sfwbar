/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "taskbaritem.h"
#include "taskbargroup.h"
#include "taskbar.h"
#include "wintree.h"
#include "config.h"

G_DEFINE_TYPE_WITH_CODE (Taskbar, taskbar, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Taskbar));

static GList *taskbars;

static GtkWidget *taskbar_get_child ( GtkWidget *self )
{
  TaskbarPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR(self),NULL);
  priv = taskbar_get_instance_private(TASKBAR(self));

  return priv->taskbar;
}

static GtkWidget *taskbar_mirror ( GtkWidget *src )
{
  GtkWidget *self;
  TaskbarPrivate *dpriv, *spriv;

  g_return_val_if_fail(IS_TASKBAR(src),NULL);
  spriv = taskbar_get_instance_private(TASKBAR(src));

  self = taskbar_new(TRUE);
  dpriv = taskbar_get_instance_private(TASKBAR(self));
  dpriv->filter = spriv->filter;
  dpriv->floating_filter = spriv->floating_filter;
  g_object_set_data(G_OBJECT(self),"title_width",
      g_object_get_data(G_OBJECT(src),"title_width"));
  g_object_set_data(G_OBJECT(self),"group",
      g_object_get_data(G_OBJECT(src),"group"));
  g_object_set_data(G_OBJECT(self),"g_cols",
      g_object_get_data(G_OBJECT(src),"g_cols"));
  g_object_set_data(G_OBJECT(self),"g_rows",
      g_object_get_data(G_OBJECT(src),"g_rows"));
  g_object_set_data(G_OBJECT(self),"g_icons",
      g_object_get_data(G_OBJECT(src),"g_icons"));
  g_object_set_data(G_OBJECT(self),"g_labels",
      g_object_get_data(G_OBJECT(src),"g_labels"));
  g_object_set_data_full(G_OBJECT(self),"g_css",
      g_strdup(g_object_get_data(G_OBJECT(src),"g_css")),g_free);
  g_object_set_data_full(G_OBJECT(self),"g_style",
      g_strdup(g_object_get_data(G_OBJECT(src),"g_style")),g_free);
  g_object_set_data(G_OBJECT(self),"g_title_width",
      g_object_get_data(G_OBJECT(src),"g_title_width"));
  g_object_set_data(G_OBJECT(self),"g_sort",
      g_object_get_data(G_OBJECT(src),"g_sort"));
  g_object_set_data(G_OBJECT(self),"title_width",
      g_object_get_data(G_OBJECT(src),"title_width"));
  flow_grid_copy_properties(self, src);
  base_widget_copy_properties(self,src);
  taskbar_populate();

  return self;
}

static void taskbar_destroy ( GtkWidget *self )
{
  taskbars = g_list_remove(taskbars,self);
  GTK_WIDGET_CLASS(taskbar_parent_class)->destroy(self);
}

static void taskbar_class_init ( TaskbarClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->get_child = taskbar_get_child;
  BASE_WIDGET_CLASS(kclass)->mirror = taskbar_mirror;
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;
  GTK_WIDGET_CLASS(kclass)->destroy = taskbar_destroy;
}

static void taskbar_init ( Taskbar *self )
{
  TaskbarPrivate *priv;

  priv = taskbar_get_instance_private(TASKBAR(self));
  priv->taskbar = flow_grid_new(TRUE);
  gtk_container_add(GTK_CONTAINER(self),priv->taskbar);
  flow_grid_invalidate(priv->taskbar);
}

GtkWidget *taskbar_new ( gboolean toplevel )
{
  GtkWidget *self;

  self = GTK_WIDGET(g_object_new(taskbar_get_type(), NULL));
  if(toplevel)
    taskbars = g_list_prepend(taskbars, self);

  return self;
}

gpointer taskbar_group_id ( GtkWidget *self, window_t *win )
{
  return win->appid;
}

gpointer taskbar_holder_new ( GtkWidget *self, window_t *win )
{
  return taskbar_group_new (win->appid, self);
}

GtkWidget *taskbar_holder_get ( GtkWidget *self, window_t *win, gboolean new )
{
  GtkWidget *taskbar, *holder;

  g_return_val_if_fail(IS_TASKBAR(self), NULL);
  if(!g_object_get_data(G_OBJECT(self),"group"))
    return self;

  holder = flow_grid_find_child(self, taskbar_group_id(self, win));
  taskbar = holder?base_widget_get_child(holder):NULL;
  if(!taskbar && new)
    taskbar = taskbar_holder_new(self, win);

  return taskbar;
}

void taskbar_set_filter ( GtkWidget *self, gint filter )
{
  TaskbarPrivate *priv;

  g_return_if_fail(IS_TASKBAR(self));
  priv = taskbar_get_instance_private(TASKBAR(self));

  if(filter == G_TOKEN_FLOATING)
    priv->floating_filter = TRUE;
  else
    priv->filter = filter;
}

gint taskbar_get_filter ( GtkWidget *self, gboolean *floating )
{
  TaskbarPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR(self),0);
  priv = taskbar_get_instance_private(TASKBAR(self));

  *floating = priv->floating_filter;
  return priv->filter;
}

void taskbar_invalidate_item ( window_t *win )
{
  GList *iter;
  GtkWidget *taskbar;

  for(iter=taskbars; iter; iter=g_list_next(iter))
  {
    taskbar = taskbar_holder_get(iter->data, win, FALSE);
    if(taskbar)
      flow_item_invalidate(flow_grid_find_child(taskbar, win));
    if(taskbar!=iter->data)
      flow_item_invalidate(
          flow_grid_find_child(iter->data, taskbar_group_id(taskbar, win)));
  }
}

void taskbar_invalidate_all ( void )
{
  GList *iter;

  for(iter=wintree_get_list();iter;iter=g_list_next(iter))
    taskbar_invalidate_item(iter->data);
}

void taskbar_init_item ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    taskbar_item_new(win, taskbar_holder_get(iter->data, win, TRUE));
}

void taskbar_destroy_item ( window_t *win )
{
  GList *iter;
  GtkWidget *taskbar;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if( (taskbar = taskbar_holder_get(iter->data, win, FALSE)) )
    {
      flow_grid_delete_child(taskbar, win);
      if(!flow_grid_n_children(taskbar) && taskbar!=iter->data)
      {
        taskbars = g_list_remove(taskbars, taskbar);
        flow_grid_delete_child(iter->data, taskbar_group_id(taskbar, win));
      }
    }
}

void taskbar_populate ( void )
{
  GList *iter;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_init_item (iter->data);
}

void taskbar_update_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    flow_grid_update(iter->data);
}
