/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include "flowgrid.h"
#include "taskbaritem.h"
#include "taskbarpopup.h"
#include "taskbarpager.h"
#include "taskbarshell.h"
#include "taskbar.h"
#include "wintree.h"
#include "config/config.h"

G_DEFINE_TYPE_WITH_CODE (TaskbarShell, taskbar_shell, TASKBAR_TYPE,
    G_ADD_PRIVATE (TaskbarShell))

static void taskbar_shell_destroy ( GtkWidget *self )
{
  TaskbarShellPrivate *priv;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  wintree_listener_remove(self);
  workspace_listener_remove(self);
  if(priv->timer_h)
  {
    g_source_remove(priv->timer_h);
    priv->timer_h = 0;
  }
  GTK_WIDGET_CLASS(taskbar_shell_parent_class)->destroy(self);
}

static void taskbar_shell_mirror ( GtkWidget *self, GtkWidget *src )
{
  TaskbarShellPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  g_return_if_fail(IS_TASKBAR_SHELL(src));
  BASE_WIDGET_CLASS(taskbar_shell_parent_class)->mirror(self, src);
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(src));

  taskbar_shell_set_api(self, priv->get_taskbar);
  taskbar_shell_set_group_title_width(self, priv->title_width);
  taskbar_shell_set_group_cols(self, priv->cols);
  taskbar_shell_set_group_rows(self, priv->rows);
  taskbar_shell_set_group_icons(self, priv->icons);
  taskbar_shell_set_group_labels(self, priv->labels);
  taskbar_shell_set_group_sort(self, priv->sort);
  for(iter=priv->css; iter; iter=g_list_next(iter))
    taskbar_shell_set_group_css(self, iter->data);
  taskbar_shell_set_group_style(self, priv->style);
}

static void taskbar_shell_item_init ( window_t *win, GtkWidget *self )
{
  TaskbarShellPrivate *priv;
  GtkWidget *taskbar;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));
  if( (taskbar = priv->get_taskbar(self, win, TRUE)) )
    taskbar_item_new(win, taskbar);
}

void taskbar_shell_item_invalidate ( window_t *win, GtkWidget *self )
{
  TaskbarShellPrivate *priv;
  GtkWidget *taskbar;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));
  if( (taskbar = priv->get_taskbar(self, win, FALSE)) )
  {
    flow_item_invalidate(flow_grid_find_child(taskbar, win));
    flow_item_invalidate(taskbar_get_parent(taskbar));
  }
}

static void taskbar_shell_item_destroy ( window_t *win, GtkWidget *self )
{
  TaskbarShellPrivate *priv;
  GtkWidget *taskbar, *parent;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));
  if( (taskbar = priv->get_taskbar(self, win, FALSE)) )
  {
    flow_grid_delete_child(taskbar, win);
    if(!flow_grid_n_children(taskbar) && taskbar != self)
      flow_grid_delete_child(self,
          flow_item_get_source(taskbar_get_parent(taskbar)));
    else if( (parent = taskbar_get_parent(taskbar)) )
      flow_item_invalidate(parent);
  }
}

static window_listener_t taskbar_shell_window_listener = {
  .window_new = (void (*)(window_t *, void *))taskbar_shell_item_init,
  .window_invalidate = (void (*)(window_t *, void *))taskbar_shell_item_invalidate,
  .window_destroy = (void (*)(window_t *, void *))taskbar_shell_item_destroy,
};

void taskbar_shell_ws_invalidate ( workspace_t *ws, GtkWidget *self )
{
  taskbar_shell_invalidate(self);
}

static workspace_listener_t taskbar_shell_workspace_listener = {
  .workspace_invalidate = (void (*)(workspace_t *, void *))taskbar_shell_ws_invalidate,
};

static void taskbar_shell_class_init ( TaskbarShellClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = taskbar_shell_destroy;
  BASE_WIDGET_CLASS(kclass)->mirror = taskbar_shell_mirror;
}

static void taskbar_shell_init ( TaskbarShell *self )
{
  TaskbarShellPrivate *priv;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));
  priv->get_taskbar = taskbar_get_taskbar;
  priv->timer_h = g_timeout_add(100, (GSourceFunc)flow_grid_update, self);
  priv->title_width = -1;
  wintree_listener_register(&taskbar_shell_window_listener, self);
  workspace_listener_register(&taskbar_shell_workspace_listener, self);
}

void taskbar_shell_init_child ( GtkWidget *self, GtkWidget *child )
{
  TaskbarShellPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  g_return_if_fail(IS_FLOW_GRID(child));
  priv = taskbar_shell_get_instance_private(
      TASKBAR_SHELL(base_widget_get_mirror_parent(self)));

  flow_grid_set_title_width(child, priv->title_width);
  if(priv->cols>0)
    flow_grid_set_cols(child, priv->cols);
  if(priv->rows>0)
    flow_grid_set_rows(child, priv->rows);
  flow_grid_set_icons(child, priv->icons);
  flow_grid_set_labels(child, priv->labels);
  flow_grid_set_sort(child, priv->sort);
  for(iter=priv->css; iter; iter=g_list_next(iter))
    base_widget_set_css(child, g_strdup(iter->data));
  base_widget_set_style_static(child, g_strdup(priv->style));
}

void taskbar_shell_invalidate ( GtkWidget *self )
{
  GList *iter;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_shell_item_invalidate(iter->data, self);
}

void taskbar_shell_set_filter ( GtkWidget *self, gint filter )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  self = base_widget_get_mirror_parent(self);
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  if(filter == G_TOKEN_FLOATING)
    priv->floating_filter = TRUE;
  else
    priv->filter = filter;

  taskbar_shell_invalidate(self);
}

gint taskbar_shell_get_filter ( GtkWidget *self, gboolean *floating )
{
  TaskbarShellPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_SHELL(self), 0);
  self = base_widget_get_mirror_parent(self);
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  *floating = priv->floating_filter;
  return priv->filter;
}

void taskbar_shell_set_api ( GtkWidget *self, GtkWidget *(*get_taskbar)
    (GtkWidget *, window_t *, gboolean) )
{
  TaskbarShellPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  if(priv->get_taskbar == get_taskbar)
    return;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_shell_item_destroy(iter->data, self);

  priv->get_taskbar = get_taskbar;
  
  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_shell_item_init(iter->data, self);

  for(iter=base_widget_get_mirror_children(self); iter; iter=g_list_next(iter))
    taskbar_shell_set_api(iter->data, get_taskbar);
}

void taskbar_shell_set_group_style ( GtkWidget *self, gchar *style )
{
  TaskbarShellPrivate *priv;
  GtkWidget *taskbar;
  GList *iter;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  g_free(priv->style);
  priv->style = g_strdup(style);

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    if( (taskbar=priv->get_taskbar(self, iter->data, FALSE)) && taskbar!=self )
      base_widget_set_style_static(taskbar, style);

  g_list_foreach(base_widget_get_mirror_children(self),
      (GFunc)taskbar_shell_set_group_style, style);
}

void taskbar_shell_set_group_css ( GtkWidget *self, gchar *css )
{
  TaskbarShellPrivate *priv;
  GtkWidget *taskbar;
  GList *iter;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  if(!css || g_list_find_custom(priv->css, css, (GCompareFunc)g_strcmp0))
    return;

  priv->css = g_list_append(priv->css, g_strdup(css));
  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    if( (taskbar=priv->get_taskbar(self, iter->data, FALSE)) && taskbar!=self )
      base_widget_set_css(taskbar, css);

  g_list_foreach(base_widget_get_mirror_children(self),
      (GFunc)taskbar_shell_set_group_css, g_strdup(css));
}

static void taskbar_shell_propagate ( GtkWidget *self, gint value,
    void (*grid_func)(GtkWidget *, gint) )
{
  TaskbarShellPrivate *priv;
  GtkWidget *taskbar;
  GList *iter, *sub;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  self = base_widget_get_mirror_parent(self);
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    if( (taskbar=priv->get_taskbar(self, iter->data, FALSE)) && taskbar!=self )
      grid_func(taskbar, value);
  for(sub=base_widget_get_mirror_children(self); sub; sub=g_list_next(sub))
  {
    for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
      if( (taskbar=priv->get_taskbar(sub->data, iter->data, FALSE)) &&
          taskbar!=sub->data )
        grid_func(taskbar, value);
  }
}

void taskbar_shell_set_group_cols ( GtkWidget *self, gint cols )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(
      TASKBAR_SHELL(base_widget_get_mirror_parent(self)));

  priv->cols = cols;
  taskbar_shell_propagate(self, cols, flow_grid_set_cols);
}

void taskbar_shell_set_group_rows ( GtkWidget *self, gint rows )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(
      TASKBAR_SHELL(base_widget_get_mirror_parent(self)));

  priv->rows = rows;
  taskbar_shell_propagate(self, rows, flow_grid_set_rows);
}

void taskbar_shell_set_group_sort ( GtkWidget *self, gboolean sort )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(
      TASKBAR_SHELL(base_widget_get_mirror_parent(self)));

  priv->sort = sort;
  taskbar_shell_propagate(self, sort, flow_grid_set_sort);
}

void taskbar_shell_set_group_title_width ( GtkWidget *self, gint width )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(
      TASKBAR_SHELL(base_widget_get_mirror_parent(self)));

  if(!width)
    width = -1;

  priv->title_width = width;
  taskbar_shell_propagate(self, width, flow_grid_set_title_width);
}

void taskbar_shell_set_group_labels ( GtkWidget *self, gboolean labels )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(
      TASKBAR_SHELL(base_widget_get_mirror_parent(self)));

  priv->labels = labels;
  taskbar_shell_propagate(self, labels, flow_grid_set_labels);
}

void taskbar_shell_set_group_icons ( GtkWidget *self, gboolean icons )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(
      TASKBAR_SHELL(base_widget_get_mirror_parent(self)));

  priv->icons = icons;
  taskbar_shell_propagate(self, icons, flow_grid_set_icons);
}

void taskbar_shell_set_tooltips ( GtkWidget *self, gboolean tooltips )
{
  TaskbarShellPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  if(priv->tooltips == tooltips)
    return;

  priv->tooltips = tooltips;
  taskbar_shell_invalidate(self);
  g_list_foreach(base_widget_get_mirror_children(self),
      (GFunc)taskbar_shell_invalidate, NULL);
}

gboolean taskbar_shell_get_tooltips ( GtkWidget *self )
{
  TaskbarShellPrivate *priv;
  GtkWidget *parent;

  if( (parent = taskbar_get_parent(self)) )
    self = flow_item_get_parent(parent);

  g_return_val_if_fail(IS_TASKBAR_SHELL(self), FALSE);
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  return priv->tooltips;
}
