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
  g_object_set_data(G_OBJECT(self),"g_css",
      g_strdup(g_object_get_data(G_OBJECT(src),"g_css")));
  g_object_set_data(G_OBJECT(self),"g_style",
      g_strdup(g_object_get_data(G_OBJECT(src),"g_style")));
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

static void taskbar_class_init ( TaskbarClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->get_child = taskbar_get_child;
  BASE_WIDGET_CLASS(kclass)->mirror = taskbar_mirror;
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
  taskbars = g_list_prepend(taskbars,self);

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

void taskbar_invalidate_all ( window_t *win, gboolean filter )
{
  GList *iter;
  gboolean floating;

  if(!win)
    return;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(!filter || taskbar_get_filter(iter->data,&floating) || floating)
    {
      taskbar_item_invalidate(flow_grid_find_child(iter->data,win));
      if(taskbar_is_toplevel(iter->data) && win->appid &&
          g_object_get_data(G_OBJECT(iter->data),"group"))
        taskbar_group_invalidate(flow_grid_find_child(iter->data,win->appid));
    }
}

void taskbar_invalidate_conditional ( void )
{
  GList *iter;

  for(iter=wintree_get_list();iter;iter=g_list_next(iter))
    taskbar_invalidate_all(iter->data,TRUE);
}

void taskbar_init_item ( window_t *win )
{
  GList *iter;
  GtkWidget *taskbar;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data && taskbar_is_toplevel(iter->data))
    {
      if(g_object_get_data(G_OBJECT(iter->data),"group"))
        taskbar = taskbar_group_new(win->appid,iter->data);
      else
        taskbar = iter->data;

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
    if(iter->data && taskbar_is_toplevel(iter->data) &&
        g_object_get_data(G_OBJECT(iter->data),"group"))
    {
      taskbar = iter->data;
      group = taskbar_group_new(win->appid,taskbar);
      flow_grid_delete_child(group,win);
      if(!flow_grid_n_children(group))
      {
        taskbars = g_list_remove(taskbars,group);
        flow_grid_delete_child(taskbar,win->appid);
      }
      group = taskbar_group_new(new_appid,taskbar);
      taskbar_item_new(win,group);
    }
}

void taskbar_destroy_item ( window_t *win )
{
  GList *iter;
  GtkWidget *taskbar, *tgroup;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data && taskbar_is_toplevel(iter->data))
    {
      taskbar = iter->data;
      if(g_object_get_data(G_OBJECT(taskbar),"group"))
      {
        tgroup = taskbar_group_new(win->appid,taskbar);
        flow_grid_delete_child(tgroup,win);
        if(!flow_grid_n_children(tgroup))
        {
          taskbars = g_list_remove(taskbars,tgroup);
          flow_grid_delete_child(taskbar, win->appid);
        }
      }
      else
        flow_grid_delete_child(taskbar,win);
    }
}

void taskbar_update_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    flow_grid_update(iter->data);
}
