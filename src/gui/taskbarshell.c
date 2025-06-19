/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include "bar.h"
#include "flowgrid.h"
#include "taskbaritem.h"
#include "taskbarpopup.h"
#include "taskbarpager.h"
#include "taskbarshell.h"
#include "taskbar.h"
#include "wintree.h"
#include "config/config.h"
#include "gui/filter.h"

G_DEFINE_TYPE_WITH_CODE (TaskbarShell, taskbar_shell, TASKBAR_TYPE,
    G_ADD_PRIVATE (TaskbarShell))

enum {
  TASKBAR_SHELL_API = 1,
  TASKBAR_SHELL_LABELS,
  TASKBAR_SHELL_ICONS,
  TASKBAR_SHELL_TITLE_WIDTH,
  TASKBAR_SHELL_SORT,
  TASKBAR_SHELL_COLS,
  TASKBAR_SHELL_ROWS,
  TASKBAR_SHELL_PRIMARY_AXIS,
  TASKBAR_SHELL_FILTER,
  TASKBAR_SHELL_STYLE,
  TASKBAR_SHELL_CSS,
};

enum TaskbarShellApi {
  API_NONE,
  API_DEFAULT,
  API_POPUP,
  API_PAGER,
};

GEnumValue taskbar_shell_api[] = {
  { API_NONE, "false", "none" },
  { API_DEFAULT, "true", "default" },
  { API_POPUP, "popup", "popup" },
  { API_PAGER, "pager", "pager" },
};

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
  g_clear_pointer(&priv->css, g_free);
  g_clear_pointer(&priv->style, g_bytes_unref);
  GTK_WIDGET_CLASS(taskbar_shell_parent_class)->destroy(self);
}

static void taskbar_shell_mirror ( GtkWidget *self, GtkWidget *src )
{
  g_return_if_fail(IS_TASKBAR_SHELL(self));
  g_return_if_fail(IS_TASKBAR_SHELL(src));
  BASE_WIDGET_CLASS(taskbar_shell_parent_class)->mirror(self, src);

  g_object_bind_property(G_OBJECT(src), "group",
      G_OBJECT(self), "group", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_cols",
      G_OBJECT(self), "group_cols", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_rows",
      G_OBJECT(self), "group_rows", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_title_width",
      G_OBJECT(self), "group_title_width", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_icons",
      G_OBJECT(self), "group_icons", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_labels",
      G_OBJECT(self), "group_labels", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_sort",
      G_OBJECT(self), "group_sort", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_style",
      G_OBJECT(self), "group_style", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "group_css",
      G_OBJECT(self), "group_css", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "tooltips",
      G_OBJECT(self), "tooltips", G_BINDING_SYNC_CREATE);
}

static void taskbar_shell_item_init ( window_t *win, GtkWidget *self )
{
  TaskbarShellPrivate *priv;
  GtkWidget *taskbar;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));
  if( (taskbar = priv->get_taskbar(self, win, TRUE)) )
    taskbar_item_new(win, taskbar);
}

static void taskbar_shell_item_invalidate ( window_t *win, GtkWidget *self )
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

static void taskbar_shell_ws_invalidate ( workspace_t *ws, GtkWidget *self )
{
  taskbar_shell_invalidate(self);
}

static workspace_listener_t taskbar_shell_workspace_listener = {
  .workspace_invalidate = (void (*)(workspace_t *, void *))taskbar_shell_ws_invalidate,
};

static void taskbar_shell_set_api ( GtkWidget *self, gint id)
{
  TaskbarShellPrivate *priv;
  GtkWidget *(*get_taskbar)(GtkWidget *, window_t *, gboolean);
  GList *iter;

  g_return_if_fail(IS_TASKBAR_SHELL(self));
  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  if(id == API_NONE)
    get_taskbar = taskbar_get_taskbar;
  else if(id == API_DEFAULT || id == API_POPUP)
    get_taskbar = taskbar_popup_get_taskbar;
  else if(id == API_PAGER)
    get_taskbar = taskbar_pager_get_taskbar;
  else
    return;

  if(priv->get_taskbar == get_taskbar)
    return;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_shell_item_destroy(iter->data, self);

  priv->get_taskbar = get_taskbar;
  priv->api_id = id;
  
  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_shell_item_init(iter->data, self);
}

static void taskbar_shell_get_property ( GObject *self, guint id,
    GValue *value, GParamSpec *spec )
{
  TaskbarShellPrivate *priv;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));

  switch(id)
  {
    case TASKBAR_SHELL_API:
      g_value_set_enum(value, priv->api_id);
      break;
    case TASKBAR_SHELL_LABELS:
      g_value_set_boolean(value, priv->labels);
      break;
    case TASKBAR_SHELL_ICONS:
      g_value_set_boolean(value, priv->icons);
      break;
    case TASKBAR_SHELL_TITLE_WIDTH:
      g_value_set_int(value, priv->title_width);
      break;
    case TASKBAR_SHELL_SORT:
      g_value_set_boolean(value, priv->sort);
      break;
    case TASKBAR_SHELL_COLS:
      g_value_set_int(value, priv->cols);
      break;
    case TASKBAR_SHELL_ROWS:
      g_value_set_int(value, priv->rows);
      break;
    case TASKBAR_SHELL_PRIMARY_AXIS:
      g_value_set_enum(value, priv->primary_axis);
      break;
    case TASKBAR_SHELL_STYLE:
      g_value_set_boxed(value, priv->style);
      break;
    case TASKBAR_SHELL_CSS:
      g_value_set_string(value, priv->css);
      break;
    case TASKBAR_SHELL_FILTER:
      g_value_set_enum(value, priv->filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void taskbar_shell_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  TaskbarShellPrivate *priv;

  priv = taskbar_shell_get_instance_private(TASKBAR_SHELL(self));
  switch(id)
  {
    case TASKBAR_SHELL_API:
      taskbar_shell_set_api(GTK_WIDGET(self), g_value_get_enum(value));
      break;
    case TASKBAR_SHELL_LABELS:
      priv->labels = g_value_get_boolean(value);
      break;
    case TASKBAR_SHELL_ICONS:
      priv->icons = g_value_get_boolean(value);
      break;
    case TASKBAR_SHELL_TITLE_WIDTH:
      priv->title_width = g_value_get_int(value);
      break;
    case TASKBAR_SHELL_SORT:
      priv->sort = g_value_get_boolean(value);
      break;
    case TASKBAR_SHELL_COLS:
      priv->cols = g_value_get_int(value);
      break;
    case TASKBAR_SHELL_ROWS:
      priv->rows = g_value_get_int(value);
      break;
    case TASKBAR_SHELL_PRIMARY_AXIS:
      priv->primary_axis = g_value_get_enum(value);
      break;
    case TASKBAR_SHELL_STYLE:
      if(priv->style != g_value_get_boxed(value))
      {
        g_bytes_unref(priv->style);
        priv->style = g_bytes_ref(g_value_get_boxed(value));
      }
      break;
    case TASKBAR_SHELL_CSS:
      g_clear_pointer(&priv->css, g_free);
      priv->css = g_strdup(g_value_get_string(value));
      break;
    case TASKBAR_SHELL_FILTER:
      priv->filter |= g_value_get_enum(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void taskbar_shell_class_init ( TaskbarShellClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = taskbar_shell_destroy;
  BASE_WIDGET_CLASS(kclass)->mirror = taskbar_shell_mirror;
  G_OBJECT_CLASS(kclass)->get_property = taskbar_shell_get_property;
  G_OBJECT_CLASS(kclass)->set_property = taskbar_shell_set_property;

  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_API,
      g_param_spec_enum("group", "group", "sfwbar_config",
        g_enum_register_static("taskbar_shell_api", taskbar_shell_api),
        API_NONE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_LABELS,
      g_param_spec_boolean("group_labels", "labels", "sfwbar_config", FALSE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_ICONS,
      g_param_spec_boolean("group_icons", "icons", "sfwbar_config", FALSE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      TASKBAR_SHELL_TITLE_WIDTH, g_param_spec_int("group_title_width",
        "title_width", "sfwbar_config", -1, INT_MAX, -1,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_STYLE,
      g_param_spec_boxed("group_style", "style", "sfwbar_config", G_TYPE_BYTES,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_CSS,
      g_param_spec_string("group_css", "css", "sfwbar_config", NULL,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_SORT,
      g_param_spec_boolean("group_sort", "sort", "sfwbar_config", FALSE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_COLS,
    g_param_spec_int("group_cols", "cols", "sfwbar_config", 0, INT_MAX, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_ROWS,
    g_param_spec_int("group_rows", "rows", "sfwbar_config", 0, INT_MAX, 1,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      TASKBAR_SHELL_PRIMARY_AXIS, g_param_spec_enum("group_primary_axis",
        "primary_axis", "sfwbar_config", g_type_from_name("flow_grid_axis"),
        FLOW_GRID_AXIS_DEFAULT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), TASKBAR_SHELL_FILTER,
      g_param_spec_enum("filter", "filter", "sfwbar_config", filter_type_get(),
        0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
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
  g_return_if_fail(IS_TASKBAR_SHELL(self));
  g_return_if_fail(IS_FLOW_GRID(child));

  g_object_bind_property(G_OBJECT(self), "group_cols",
      G_OBJECT(child), "cols", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "group_rows",
      G_OBJECT(child), "rows", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "group_sort",
      G_OBJECT(child), "sort", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "group_labels",
      G_OBJECT(child), "labels", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "group_icons",
      G_OBJECT(child), "icons", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "tooltips",
      G_OBJECT(child), "tooltips", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "title_width",
      G_OBJECT(child), "title_width", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "group_style",
      G_OBJECT(child), "style", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(self), "group_css",
      G_OBJECT(child), "css", G_BINDING_SYNC_CREATE);
}

void taskbar_shell_invalidate ( GtkWidget *self )
{
  GList *iter;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_shell_item_invalidate(iter->data, self);
}
