/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "flowgrid.h"
#include "taskbaritem.h"
#include "taskbarpopup.h"
#include "taskbarpager.h"
#include "taskbarshell.h"
#include "taskbar.h"
#include "wintree.h"
#include "config.h"

G_DEFINE_TYPE_WITH_CODE (Taskbar, taskbar, FLOW_GRID_TYPE,
    G_ADD_PRIVATE (Taskbar))

GtkWidget *taskbar_get_taskbar ( GtkWidget *shell, window_t *win,
    gboolean create )
{
  return shell;
}

static void taskbar_class_init ( TaskbarClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;
}

static void taskbar_init ( Taskbar *self )
{
  action_t *action;

  flow_grid_invalidate(GTK_WIDGET(self));
  action = action_new();
  action->id = g_intern_static_string("taskbaritemdefault");
  base_widget_set_action(GTK_WIDGET(self), 1, 0, action);
}

GtkWidget *taskbar_new ( GtkWidget *parent )
{
  GtkWidget *self;
  TaskbarPrivate *priv;

  self = GTK_WIDGET(g_object_new(taskbar_get_type(), NULL));
  priv = taskbar_get_instance_private(TASKBAR(self));
  priv->parent = parent;

  return self;
}

GtkWidget *taskbar_get_parent ( GtkWidget *self )
{
  TaskbarPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR(self), NULL);
  priv = taskbar_get_instance_private(TASKBAR(self));

  return priv->parent;
}
