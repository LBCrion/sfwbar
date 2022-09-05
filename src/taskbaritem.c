/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "taskbaritem.h"
#include "taskbar.h"
#include "scaleimage.h"
#include "action.h"
#include "bar.h"
#include "wintree.h"
#include "config.h"

G_DEFINE_TYPE_WITH_CODE (TaskbarItem, taskbar_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (TaskbarItem));

static gboolean taskbar_item_click_cb ( GtkWidget *widget, GdkEventButton *ev,
    gpointer self )
{
  TaskbarItemPrivate *priv;
  action_t *action;

  g_return_val_if_fail(IS_TASKBAR_ITEM(self),FALSE);
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  if(ev->type != GDK_BUTTON_PRESS || ev->button < 1 || ev->button > 3)
    return FALSE;
  if(GTK_IS_BUTTON(widget) && ev->button == 1)
    return FALSE;

  action = base_widget_get_action(priv->taskbar,ev->button);
  if(!action)
    return FALSE;

  action_exec(gtk_bin_get_child(GTK_BIN(widget)), action,(GdkEvent *)ev,
      wintree_from_id(priv->win->uid),NULL);
  return TRUE;
}

static gboolean taskbar_item_scroll_cb ( GtkWidget *w, GdkEventScroll *event,
    gpointer self )
{
  TaskbarItemPrivate *priv;
  gint button;
  action_t *action;

  g_return_val_if_fail(IS_TASKBAR_ITEM(self),FALSE);
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  switch(event->direction)
  {
    case GDK_SCROLL_UP:
      button = 4;
      break;
    case GDK_SCROLL_DOWN:
      button = 5;
      break;
    case GDK_SCROLL_LEFT:
      button = 6;
      break;
    case GDK_SCROLL_RIGHT:
      button = 7;
      break;
    default:
      return FALSE;
  }
  action = base_widget_get_action(priv->taskbar,button);
  if(!action)
    return FALSE;

  action_exec(gtk_bin_get_child(GTK_BIN(w)), action, (GdkEvent *)event,
      wintree_from_id(priv->win->uid),NULL);

  return TRUE;
}

static void taskbar_item_button_cb( GtkWidget *widget, gpointer self )
{
  TaskbarItemPrivate *priv; 
  action_t *action;

  g_return_if_fail(IS_TASKBAR_ITEM(self));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  action = base_widget_get_action(priv->taskbar,1);

  if(action)
    action_exec(gtk_bin_get_child(GTK_BIN(widget)),action,NULL,priv->win,NULL);
  else
  {
    if ( wintree_is_focused(priv->win->uid) )
      wintree_minimize(priv->win->uid);
    else
      wintree_focus(priv->win->uid);
  }

  taskbar_invalidate_all(priv->win);
}

static void taskbar_item_destroy ( GtkWidget *self )
{
  GTK_WIDGET_CLASS(taskbar_item_parent_class)->destroy(self);
}

static window_t *taskbar_item_get_window ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_ITEM(self),NULL);
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  return priv->win;
}

static void taskbar_item_update ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;
  gchar *appid;

  g_return_if_fail(IS_TASKBAR_ITEM(self));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));
  if(!priv->invalid)
    return;

  if(priv->label)
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)),priv->win->title))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->win->title);

  if(priv->icon)
  {
    if(priv->win->appid && *(priv->win->appid))
      appid = priv->win->appid;
    else
      appid = wintree_appid_map_lookup(priv->win->title);
    scale_image_set_image(priv->icon,appid,NULL);
  }

  if ( wintree_is_focused(taskbar_item_get_window(self)->uid) )
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "taskbar_active");
  else
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "taskbar_normal");

  gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(self)),
      GTK_STATE_FLAG_PRELIGHT);

  flow_item_set_active(self, !priv->win->output ||
      !g_object_get_data(G_OBJECT(priv->taskbar),"filter_output") ||
      !g_strcmp0(priv->win->output,
        bar_get_output(base_widget_get_child(priv->taskbar))));
  css_widget_cascade(self,NULL);

  priv->invalid = FALSE;
}

static gint taskbar_item_compare ( GtkWidget *a, GtkWidget *b, GtkWidget *parent )
{
  TaskbarItemPrivate *p1,*p2;

  g_return_val_if_fail(IS_TASKBAR_ITEM(a),0);
  g_return_val_if_fail(IS_TASKBAR_ITEM(b),0);

  p1 = taskbar_item_get_instance_private(TASKBAR_ITEM(a));
  p2 = taskbar_item_get_instance_private(TASKBAR_ITEM(b));
  return wintree_compare(p1->win,p2->win);
}

static void taskbar_item_class_init ( TaskbarItemClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = taskbar_item_destroy;
  FLOW_ITEM_CLASS(kclass)->update = taskbar_item_update;
  FLOW_ITEM_CLASS(kclass)->invalidate = taskbar_item_invalidate;
  FLOW_ITEM_CLASS(kclass)->get_parent = (void * (*)(GtkWidget *))taskbar_item_get_window;
  FLOW_ITEM_CLASS(kclass)->compare = taskbar_item_compare;
}

static void taskbar_item_init ( TaskbarItem *self )
{
}

GtkWidget *taskbar_item_new( window_t *win, GtkWidget *taskbar )
{
  GtkWidget *self;
  TaskbarItemPrivate *priv;
  GtkWidget *box, *button;
  gint dir;
  gboolean icons, labels;
  gint title_width;

  g_return_val_if_fail(IS_TASKBAR(taskbar),NULL);

  if(flow_grid_find_child(taskbar,win))
    return NULL;

  self = GTK_WIDGET(g_object_new(taskbar_item_get_type(), NULL));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  priv->win = win;
  priv->taskbar = taskbar;

  icons = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"icons"));
  labels = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"labels"));
  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"title_width"));
  if(!title_width)
    title_width = -1;
  if(!icons)
    labels = TRUE;

  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(self),button);
  gtk_widget_set_name(button, "taskbar_normal");
  gtk_widget_style_get(button,"direction",&dir,NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(button),box);
  flow_grid_child_dnd_enable(taskbar,self,button);

  if(icons)
  {
    priv->icon = scale_image_new();
    scale_image_set_image(priv->icon,win->appid,NULL);
    gtk_grid_attach_next_to(GTK_GRID(box),priv->icon,NULL,dir,1,1);
  }
  else
    priv->icon = NULL;
  if(labels)
  {
    priv->label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(priv->label),PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label),title_width);
    gtk_grid_attach_next_to(GTK_GRID(box),priv->label,priv->icon,dir,1,1);
  }
  else
    priv->label = NULL;

  priv->actions = g_object_get_data(G_OBJECT(taskbar),"actions");
  g_object_ref(G_OBJECT(self));
  flow_grid_add_child(taskbar,self);

  g_signal_connect(button,"clicked",G_CALLBACK(taskbar_item_button_cb),self);
  g_signal_connect(self,"button_press_event",
      G_CALLBACK(taskbar_item_click_cb),self);
  gtk_widget_add_events(GTK_WIDGET(self),GDK_SCROLL_MASK);
  g_signal_connect(self,"scroll-event",
      G_CALLBACK(taskbar_item_scroll_cb),self);
  taskbar_item_invalidate(self);

  return self;
}

void taskbar_item_invalidate ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;

  if(!self)
    return;

  g_return_if_fail(IS_TASKBAR_ITEM(self));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  flow_grid_invalidate(priv->taskbar);
  priv->invalid = TRUE;
}
