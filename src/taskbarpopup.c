/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "taskbarpopup.h"
#include "taskbar.h"
#include "taskbaritem.h"
#include "scaleimage.h"
#include "action.h"
#include "bar.h"
#include "wintree.h"
#include "menu.h"
#include "popup.h"
#include "window.h"
#include <gtk-layer-shell.h>

G_DEFINE_TYPE_WITH_CODE (TaskbarPopup, taskbar_popup, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE(TaskbarPopup))

static gboolean taskbar_popup_enter_cb ( GtkWidget *widget,
    GdkEventCrossing *event, gpointer self )
{
  TaskbarPopupPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_POPUP(self),FALSE);
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));
  if(priv->single)
    return FALSE;

  if(gtk_widget_is_visible(priv->popover))
  {
    window_ref(priv->popover, widget);
    return FALSE;
  }
  window_ref(priv->popover, widget);

  flow_grid_update(priv->tgroup);

  popup_show(priv->button, priv->popover,
      gdk_device_get_seat(gdk_event_get_device((GdkEvent *)event)));

  return FALSE;
}

static gboolean taskbar_popup_timeout_cb ( GtkWidget *popover )
{
  if(!window_ref_check(popover))
  {
    window_collapse_popups(popover);
    gtk_widget_hide(popover);
  }
  return FALSE;
}

static void taskbar_popup_grab_cb ( GtkWidget *popover, gboolean grab,
    GtkWidget *self )
{
  if(!grab)
  {
    window_unref(self, popover);
    window_unref(popover, popover);
  }
}

static void taskbar_popup_timeout_set ( GtkWidget *popover )
{
  g_timeout_add(10, (GSourceFunc)taskbar_popup_timeout_cb, popover);
}

static gboolean taskbar_popup_leave_cb ( GtkWidget *widget,
    GdkEventCrossing *event, gpointer self )
{
  TaskbarPopupPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_POPUP(self),FALSE);
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));

  window_unref(widget, priv->popover);

  return FALSE;
}

static void taskbar_popup_destroy ( GtkWidget *self )
{
  TaskbarPopupPrivate *priv;

  g_return_if_fail(IS_TASKBAR_POPUP(self));
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));

  gtk_widget_destroy(priv->popover);
  priv->popover = NULL;
  GTK_WIDGET_CLASS(taskbar_popup_parent_class)->destroy(self);
}

static gchar *taskbar_popup_get_appid ( GtkWidget *self )
{
  TaskbarPopupPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_POPUP(self),NULL);
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));

  return priv->appid;
}

static void taskbar_popup_update ( GtkWidget *self )
{
  TaskbarPopupPrivate *priv;

  g_return_if_fail(IS_TASKBAR_POPUP(self));
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));
  if(!priv->invalid)
    return;

  gtk_widget_set_name(priv->button,
      flow_grid_find_child(priv->tgroup, wintree_from_id(wintree_get_focus()))?
        "taskbar_group_active":"taskbar_group_normal");

  if(priv->icon)
    taskbar_item_set_image(priv->icon, priv->appid);

  if(priv->label)
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)), priv->appid))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->appid);

  gtk_widget_unset_state_flags(priv->button, GTK_STATE_FLAG_PRELIGHT);

  flow_grid_update(priv->tgroup);
  flow_item_set_active(self, flow_grid_n_children(priv->tgroup)>0 );
  priv->single = (flow_grid_n_children(priv->tgroup)==1);
  window_collapse_popups(priv->popover);
  gtk_widget_hide(priv->popover);

  priv->invalid = FALSE;
}

static gint taskbar_popup_compare ( GtkWidget *a, GtkWidget *b,
    GtkWidget *parent )
{
  TaskbarPopupPrivate *p1,*p2;

  g_return_val_if_fail(IS_TASKBAR_POPUP(a),0);
  g_return_val_if_fail(IS_TASKBAR_POPUP(b),0);

  p1 = taskbar_popup_get_instance_private(TASKBAR_POPUP(a));
  p2 = taskbar_popup_get_instance_private(TASKBAR_POPUP(b));

  return g_strcmp0(p1->appid,p2->appid);
}

static gboolean taskbar_popup_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  TaskbarPopupPrivate *priv;
  GList *children;
  window_t *win;
  action_t *action;

  g_return_val_if_fail(IS_TASKBAR_POPUP(self),FALSE);
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));

  children = gtk_container_get_children(GTK_CONTAINER(
        base_widget_get_child(priv->tgroup)));
  if(children && !g_list_next(children) &&
      base_widget_check_action_slot(priv->tgroup, slot))
  {
    win = flow_item_get_source(children->data);
    if( (action = base_widget_get_action(priv->tgroup, slot,
        base_widget_get_modifiers(self))) )
      action_exec(self, action, (GdkEvent *)ev,
          win?win:wintree_from_id(wintree_get_focus()), NULL);
  }
  g_list_free(children);

  return TRUE;
}

static void taskbar_popup_invalidate ( GtkWidget *self )
{
  TaskbarPopupPrivate *priv;

  if(!self)
    return;

  g_return_if_fail(IS_TASKBAR_POPUP(self));
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));

  flow_grid_invalidate(priv->taskbar);
  priv->invalid = TRUE;
}

static GtkWidget *taskbar_popup_get_taskbar ( GtkWidget *self )
{
  TaskbarPopupPrivate *priv;

  if(!self)
    return NULL;

  g_return_val_if_fail(IS_TASKBAR_POPUP(self), NULL);
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));
  return priv->tgroup;
}

static void taskbar_popup_class_init ( TaskbarPopupClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = taskbar_popup_destroy;
  BASE_WIDGET_CLASS(kclass)->action_exec = taskbar_popup_action_exec;
  BASE_WIDGET_CLASS(kclass)->get_child = taskbar_popup_get_taskbar;
  FLOW_ITEM_CLASS(kclass)->update = taskbar_popup_update;
  FLOW_ITEM_CLASS(kclass)->invalidate = taskbar_popup_invalidate;
  FLOW_ITEM_CLASS(kclass)->comp_source = (GCompareFunc)g_strcmp0;
  FLOW_ITEM_CLASS(kclass)->compare = taskbar_popup_compare;
  FLOW_ITEM_CLASS(kclass)->get_source =
    (void * (*)(GtkWidget *))taskbar_popup_get_appid;
}

static void taskbar_popup_init ( TaskbarPopup *self )
{
}

GtkWidget *taskbar_popup_new( const gchar *appid, GtkWidget *taskbar )
{
  GtkWidget *self;
  TaskbarPopupPrivate *priv;
  gint dir;
  gboolean icons, labels;
  gint title_width;
  GtkWidget *box;

  g_return_val_if_fail(IS_TASKBAR(taskbar),NULL);

  self = GTK_WIDGET(g_object_new(taskbar_popup_get_type(), NULL));
  priv = taskbar_popup_get_instance_private(TASKBAR_POPUP(self));

  icons = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar), "icons"));
  labels = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar), "labels"));
  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar), "title_width"));
  if(!title_width)
    title_width = -1;
  if(!icons)
    labels = TRUE;

  priv->taskbar = taskbar;
  priv->tgroup = taskbar_new(FALSE);
  g_object_set_data(G_OBJECT(priv->tgroup), "parent_taskbar", taskbar);
  priv->appid = g_strdup(appid);
  priv->button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(self), priv->button);
  gtk_widget_set_name(priv->button, "taskbar_group_normal");
  gtk_widget_style_get(priv->button, "direction", &dir, NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(priv->button), box);

  if(icons)
  {
    priv->icon = scale_image_new();
    gtk_grid_attach_next_to(GTK_GRID(box), priv->icon, NULL, dir, 1, 1);
    taskbar_item_set_image(priv->icon, priv->appid);
  }
  else
    priv->icon = NULL;
  if(labels)
  {
    priv->label = gtk_label_new(priv->appid);
    gtk_label_set_ellipsize (GTK_LABEL(priv->label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label), title_width);
    gtk_grid_attach_next_to(GTK_GRID(box), priv->label, priv->icon, dir, 1, 1);
  }
  else
    priv->label = NULL;

  priv->popover = gtk_window_new(GTK_WINDOW_POPUP);
  window_set_unref_func(priv->popover,
      (void(*)(gpointer))taskbar_popup_timeout_set);
  gtk_widget_set_name(priv->button, "taskbar_group");
  g_object_ref(G_OBJECT(priv->popover));
  gtk_container_add(GTK_CONTAINER(priv->popover), priv->tgroup);
  css_widget_apply(priv->tgroup, g_strdup(
        g_object_get_data(G_OBJECT(taskbar), "g_css")));
  base_widget_set_style(priv->tgroup,g_strdup(
        g_object_get_data(G_OBJECT(taskbar), "g_style")));
  gtk_widget_show(priv->tgroup);

  gtk_widget_add_events(GTK_WIDGET(self),GDK_ENTER_NOTIFY_MASK |
      GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK | GDK_SCROLL_MASK);
  g_signal_connect(self, "enter-notify-event",
      G_CALLBACK(taskbar_popup_enter_cb), self);
  g_signal_connect(priv->button, "enter-notify-event",
      G_CALLBACK(taskbar_popup_enter_cb), self);
  g_signal_connect(priv->popover, "enter-notify-event",
      G_CALLBACK(taskbar_popup_enter_cb), self);
  g_signal_connect(self, "leave-notify-event",
      G_CALLBACK(taskbar_popup_leave_cb), self);
  g_signal_connect(priv->button, "leave-notify-event",
      G_CALLBACK(taskbar_popup_leave_cb), self);
  g_signal_connect(priv->popover, "leave-notify-event",
      G_CALLBACK(taskbar_popup_leave_cb), self);
  g_signal_connect(priv->popover, "grab-notify",
      G_CALLBACK(taskbar_popup_grab_cb), self);

  if(g_object_get_data(G_OBJECT(taskbar), "g_cols"))
    flow_grid_set_cols(priv->tgroup,
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(taskbar), "g_cols")));
  if(g_object_get_data(G_OBJECT(taskbar), "g_rows"))
    flow_grid_set_rows(priv->tgroup,
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(taskbar), "g_rows")));
  g_object_set_data(G_OBJECT(priv->tgroup), "labels",
        g_object_get_data(G_OBJECT(taskbar), "g_labels"));
  g_object_set_data(G_OBJECT(priv->tgroup), "icons",
        g_object_get_data(G_OBJECT(taskbar), "g_icons"));
  g_object_set_data(G_OBJECT(priv->tgroup), "title_width",
        g_object_get_data(G_OBJECT(taskbar), "g_title_width"));
  flow_grid_set_sort(priv->tgroup,
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(taskbar), "g_sort")));

  base_widget_copy_actions(priv->tgroup, taskbar);

  g_object_ref_sink(G_OBJECT(self));
  flow_grid_add_child(taskbar, self);

  flow_item_invalidate(self);
  return priv->tgroup;
}
