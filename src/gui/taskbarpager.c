/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "wintree.h"
#include "gui/css.h"
#include "gui/taskbaritem.h"
#include "gui/taskbarpager.h"
#include "gui/taskbarshell.h"
#include "util/string.h"

G_DEFINE_TYPE_WITH_CODE (TaskbarPager, taskbar_pager, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE(TaskbarPager))

GtkWidget *taskbar_pager_get_taskbar ( GtkWidget *shell, window_t *win,
    gboolean create )
{
  TaskbarPagerPrivate *priv;
  GtkWidget *holder;

  if( !(holder = flow_grid_find_child(shell, win->workspace)) )
  {
    if(!create)
      return NULL;
    holder = taskbar_pager_new(win->workspace, shell);
  }

  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(holder));
  return priv->taskbar;
}

static gpointer taskbar_pager_get_ws ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self),NULL);
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  return priv->ws;
}

static void taskbar_pager_decorate ( GtkWidget *parent, GParamSpec *spec,
    GtkWidget *self )
{
  TaskbarPagerPrivate *priv;
  gboolean labels;
  gint title_width;

  g_return_if_fail(IS_TASKBAR_PAGER(self));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  g_object_get(G_OBJECT(parent), "labels", &labels,
      "title-width", &title_width, NULL);

  if(!labels && priv->button)
    gtk_container_remove(GTK_CONTAINER(priv->grid), priv->button);
  else if(labels && !priv->button)
  {
    g_object_ref(priv->taskbar);
    gtk_container_remove(GTK_CONTAINER(priv->grid), priv->taskbar);
    priv->button = gtk_button_new_with_label("");
    gtk_container_add(GTK_CONTAINER(priv->grid), priv->button);
    gtk_container_add(GTK_CONTAINER(priv->grid), priv->taskbar);
    g_object_unref(priv->taskbar);
  }
  if(priv->button)
    gtk_label_set_max_width_chars(
        GTK_LABEL(gtk_bin_get_child(GTK_BIN(priv->button))), title_width);
}

static void taskbar_pager_update ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;
  gchar *title;

  g_return_if_fail(IS_TASKBAR_PAGER(self));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  if(!priv->invalid)
    return;

  title = priv->ws? priv->ws->name: NULL;
  if(priv->button &&
      g_strcmp0(gtk_button_get_label(GTK_BUTTON(priv->button)), title))
    gtk_button_set_label(GTK_BUTTON(priv->button), title);

  css_set_class(base_widget_get_child(self), "focused", !!flow_grid_find_child(
        priv->taskbar, wintree_from_id(wintree_get_focus())));

  gtk_widget_unset_state_flags(base_widget_get_child(self),
      GTK_STATE_FLAG_PRELIGHT);

  flow_grid_update(priv->taskbar);
  flow_item_set_active(self, flow_grid_n_children(priv->taskbar, TRUE)>0 );

  priv->invalid = FALSE;
}

static gint taskbar_pager_compare ( GtkWidget *a, GtkWidget *b,
    GtkWidget *parent )
{
  TaskbarPagerPrivate *p1,*p2;
  gchar *e1, *e2;
  gint n1, n2;

  g_return_val_if_fail(IS_TASKBAR_PAGER(a), 0);
  g_return_val_if_fail(IS_TASKBAR_PAGER(b), 0);

  p1 = taskbar_pager_get_instance_private(TASKBAR_PAGER(a));
  p2 = taskbar_pager_get_instance_private(TASKBAR_PAGER(b));

  n1 = str_ascii_toll(p1->ws->name, &e1, 10);
  n2 = str_ascii_toll(p2->ws->name, &e2, 10);
  if((e1 && *e1) || (e2 && *e2))
    return g_strcmp0(p1->ws->name, p2->ws->name);
  else
    return n1-n2;
}

static gboolean taskbar_pager_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  TaskbarPagerPrivate *priv;
  GdkModifierType mods;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self), FALSE);
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  
  if(slot != 1)
    return FALSE;

  mods = base_widget_get_modifiers(self);

  if(!mods && slot==1)
  {
    workspace_activate(priv->ws);
    return TRUE;
  }

  return FALSE;
}

static void taskbar_pager_invalidate ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;

  if(!self)
    return;

  g_return_if_fail(IS_TASKBAR_PAGER(self));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  flow_grid_invalidate(priv->shell);
  priv->invalid = TRUE;
}

static gboolean taskbar_pager_drag_drop ( GtkWidget *self, GdkDragContext *ctx,
    gint x, gint y, guint time )
{
  GtkWidget *src;
  workspace_t *ws;
  window_t *win;

  if( (src = gtk_drag_get_source_widget(ctx)) &&
      (src = base_widget_get_parent(src)) &&
      IS_TASKBAR_PAGER(self) && IS_FLOW_ITEM(src) &&
      (ws = flow_item_get_source(self)) && (win = flow_item_get_source(src)) )
  {
    wintree_move_to(win->uid, ws->id);
    gtk_drag_finish(ctx, TRUE, FALSE, time);
    return TRUE;
  }
  return GTK_WIDGET_CLASS(taskbar_pager_parent_class)->drag_drop(self, ctx,
      x, y, time);
}

static gboolean taskbar_pager_drag_motion ( GtkWidget *self,
    GdkDragContext *ctx, gint x, gint y, guint time )
{
  TaskbarPagerPrivate *priv;
  GtkWidget *src;
  gboolean zone;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self), FALSE);

  zone = (src = base_widget_get_parent(gtk_drag_get_source_widget(ctx))) &&
      IS_FLOW_ITEM(src) &&
      (priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self))) &&
      !g_list_find(flow_grid_children_get(priv->taskbar), src);

  if(zone)
    css_add_class(self, "drop_target");
  else
    css_remove_class(self, "drop_target");

  return zone ||
    GTK_WIDGET_CLASS(taskbar_pager_parent_class)->drag_motion(self, ctx, x, y,
        time);
}

static void taskbar_pager_class_init ( TaskbarPagerClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->drag_drop = taskbar_pager_drag_drop;
  GTK_WIDGET_CLASS(kclass)->drag_motion = taskbar_pager_drag_motion;
  BASE_WIDGET_CLASS(kclass)->action_exec = taskbar_pager_action_exec;
  FLOW_ITEM_CLASS(kclass)->update = taskbar_pager_update;
  FLOW_ITEM_CLASS(kclass)->invalidate = taskbar_pager_invalidate;
  FLOW_ITEM_CLASS(kclass)->get_source = taskbar_pager_get_ws;
  FLOW_ITEM_CLASS(kclass)->compare = taskbar_pager_compare;
}

static void taskbar_pager_init ( TaskbarPager *self )
{
}

GtkWidget *taskbar_pager_new( workspace_t *ws, GtkWidget *shell )
{
  GtkWidget *self;
  TaskbarPagerPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_SHELL(shell), NULL);

  self = GTK_WIDGET(g_object_new(taskbar_pager_get_type(), NULL));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  priv->shell = shell;
  priv->taskbar = taskbar_new(self);
  taskbar_shell_init_child(shell, priv->taskbar);
  priv->ws = ws;
  priv->grid = gtk_grid_new();
  gtk_widget_set_name(GTK_WIDGET(priv->grid), "taskbar_pager");
  gtk_container_add(GTK_CONTAINER(self), priv->grid);
  gtk_container_add(GTK_CONTAINER(priv->grid), priv->taskbar);
  flow_grid_set_dnd_target(priv->taskbar, flow_grid_get_dnd_target(shell));
  flow_item_dnd_enable(shell, self, priv->grid);
  gtk_widget_show_all(self);

  g_object_ref_sink(G_OBJECT(self));
  flow_grid_add_child(shell, self);

  g_signal_connect(G_OBJECT(shell), "notify::labels",
      G_CALLBACK(taskbar_pager_decorate), self);
  g_signal_connect(G_OBJECT(shell), "notify::title-width",
      G_CALLBACK(taskbar_pager_decorate), self);
  taskbar_pager_decorate(shell, NULL, self);

  flow_item_invalidate(self);
  return self;
}
