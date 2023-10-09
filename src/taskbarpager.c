/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "taskbarpager.h"
#include "taskbar.h"
#include "pager.h"
#include "scaleimage.h"
#include "action.h"
#include "bar.h"
#include "wintree.h"
#include "menu.h"
#include "popup.h"
#include <gtk-layer-shell.h>

G_DEFINE_TYPE_WITH_CODE (TaskbarPager, taskbar_pager, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE(TaskbarPager));

static void taskbar_pager_destroy ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;

  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  g_clear_pointer(&priv->ws, wintree_workspace_free);
  GTK_WIDGET_CLASS(taskbar_pager_parent_class)->destroy(self);
}

static gpointer taskbar_pager_get_ws ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self),NULL);
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  return priv->ws;
}

static void taskbar_pager_update ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;
  workspace_t *ws;
  gchar *title;

  g_return_if_fail(IS_TASKBAR_PAGER(self));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  if(!priv->invalid)
    return;

  ws = workspace_from_id(priv->ws);
  title = ws?ws->name:NULL;
  if(g_strcmp0(gtk_button_get_label(GTK_BUTTON(priv->button)),title))
    gtk_button_set_label(GTK_BUTTON(priv->button),priv->ws);

  if (flow_grid_find_child(priv->tgroup, wintree_from_id(wintree_get_focus())))
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)),
        "taskbar_pager_active");
  else
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)),
        "taskbar_pager_normal");

  gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(self)),
      GTK_STATE_FLAG_PRELIGHT);

  flow_grid_update(priv->tgroup);
  flow_item_set_active(self, flow_grid_n_children(priv->tgroup)>0 );

  priv->invalid = FALSE;
}

static gint taskbar_pager_compare ( GtkWidget *a, GtkWidget *b,
    GtkWidget *parent )
{
  TaskbarPagerPrivate *p1,*p2;

  g_return_val_if_fail(IS_TASKBAR_PAGER(a),0);
  g_return_val_if_fail(IS_TASKBAR_PAGER(b),0);

  p1 = taskbar_pager_get_instance_private(TASKBAR_PAGER(a));
  p2 = taskbar_pager_get_instance_private(TASKBAR_PAGER(b));

  return wintree_workspace_comp(p1->ws,p2->ws);
}

static gboolean taskbar_pager_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  TaskbarPagerPrivate *priv;
  GdkModifierType mods;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self),FALSE);
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  
  g_message("action");
  if(slot != 1)
    return FALSE;

  mods = base_widget_get_modifiers(self);

  if(!mods && slot==1)
  {
    workspace_activate(workspace_from_id(priv->ws));
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

  flow_grid_invalidate(priv->taskbar);
  priv->invalid = TRUE;
}

static GtkWidget *taskbar_pager_get_taskbar ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;

  if(!self)
    return NULL;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self), NULL);
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  return priv->tgroup;
}
static void taskbar_pager_class_init ( TaskbarPagerClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = taskbar_pager_destroy;
  BASE_WIDGET_CLASS(kclass)->action_exec = taskbar_pager_action_exec;
  BASE_WIDGET_CLASS(kclass)->get_child = taskbar_pager_get_taskbar;
  FLOW_ITEM_CLASS(kclass)->update = taskbar_pager_update;
  FLOW_ITEM_CLASS(kclass)->invalidate = taskbar_pager_invalidate;
  FLOW_ITEM_CLASS(kclass)->comp_parent = (GCompareFunc)g_strcmp0;
  FLOW_ITEM_CLASS(kclass)->get_parent = taskbar_pager_get_ws;
  FLOW_ITEM_CLASS(kclass)->compare = taskbar_pager_compare;
}

static void taskbar_pager_init ( TaskbarPager *self )
{
}

GtkWidget *taskbar_pager_new( gpointer ws, GtkWidget *taskbar )
{
  GtkWidget *self;
  TaskbarPagerPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR(taskbar),NULL);

  self = GTK_WIDGET(g_object_new(taskbar_pager_get_type(), NULL));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  priv->taskbar = taskbar;
  priv->tgroup = taskbar_new(FALSE);
  g_object_set_data(G_OBJECT(priv->tgroup), "parent_taskbar", taskbar);
  priv->ws = wintree_workspace_dup(ws);
  priv->grid = gtk_grid_new();
  priv->button = gtk_button_new_with_label("");
  gtk_widget_set_name(GTK_WIDGET(priv->grid), "taskbar_pager");
  gtk_container_add(GTK_CONTAINER(self), priv->grid);
  gtk_container_add(GTK_CONTAINER(priv->grid), priv->button);
  gtk_container_add(GTK_CONTAINER(priv->grid), priv->tgroup);
  gtk_widget_show_all(self);

  css_widget_apply(priv->tgroup, g_strdup(
        g_object_get_data(G_OBJECT(taskbar), "g_css")));
  base_widget_set_style(priv->tgroup,g_strdup(
        g_object_get_data(G_OBJECT(taskbar), "g_style")));
  gtk_widget_show(priv->tgroup);

  if(g_object_get_data(G_OBJECT(taskbar), "g_cols"))
    flow_grid_set_cols(base_widget_get_child(priv->tgroup), GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(taskbar), "g_cols")));
  if(g_object_get_data(G_OBJECT(taskbar), "g_rows"))
    flow_grid_set_rows(base_widget_get_child(priv->tgroup), GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(taskbar), "g_rows")));
  g_object_set_data(G_OBJECT(priv->tgroup), "labels",
        g_object_get_data(G_OBJECT(taskbar), "g_labels"));
  g_object_set_data(G_OBJECT(priv->tgroup), "icons",
        g_object_get_data(G_OBJECT(taskbar), "g_icons"));
  g_object_set_data(G_OBJECT(priv->tgroup), "title_width",
        g_object_get_data(G_OBJECT(taskbar), "g_title_width"));
  flow_grid_set_sort(base_widget_get_child(priv->tgroup),
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(taskbar), "g_sort")));

  base_widget_copy_actions(priv->tgroup, taskbar);

  g_object_ref_sink(G_OBJECT(self));
  flow_grid_add_child(taskbar, self);
  flow_item_invalidate(self);
  return priv->tgroup;
}
