/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"

G_DEFINE_TYPE_WITH_CODE (TaskbarItem, taskbar_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (TaskbarItem));

static gboolean taskbar_item_click_cb ( GtkWidget *widget, GdkEventButton *ev,
    gpointer self )
{
  TaskbarItemPrivate *priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  if(GTK_IS_BUTTON(widget) && ev->button != 1)
    return FALSE;

  if(ev->type == GDK_BUTTON_PRESS && ev->button >= 1 && ev->button <= 3)
    action_exec(gtk_bin_get_child(GTK_BIN(widget)),
        priv->actions[ev->button],(GdkEvent *)ev,
        wintree_from_id(priv->win->uid),NULL);
  return TRUE;
}

static gboolean taskbar_item_scroll_cb ( GtkWidget *w, GdkEventScroll *event,
    gpointer self )
{
  TaskbarItemPrivate *priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));
  gint button;

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
      button = 0;
  }
  if(button)
    action_exec(gtk_bin_get_child(GTK_BIN(w)),
        priv->actions[button], (GdkEvent *)event,
        wintree_from_id(priv->win->uid),NULL);

  return TRUE;
}

static void taskbar_item_button_cb( GtkWidget *widget, gpointer self )
{
  TaskbarItemPrivate *priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  if(priv->actions[1])
    action_exec(widget,priv->actions[1],NULL,priv->win,NULL);
  else
  {
    if ( wintree_is_focused(priv->win->uid) )
      wintree_minimize(priv->win->uid);
    else
      wintree_focus(priv->win->uid);
  }

  taskbar_invalidate_all();
}

static void taskbar_item_destroy ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_TASKBAR_ITEM(self));

  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));
  iter = g_list_find(g_object_get_data(G_OBJECT(priv->taskbar),"items"),self);

  if(iter)
    g_object_set_data(G_OBJECT(priv->taskbar),"items",g_list_delete_link(
      g_object_get_data(G_OBJECT(priv->taskbar),"items"),iter));

}

void taskbar_item_update ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;

  g_return_if_fail(IS_TASKBAR_ITEM(self));

  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  if(priv->label)
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)),priv->win->title))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->win->title);

  if(priv->icon)
    scale_image_set_image(priv->icon,priv->win->appid,NULL);

  if ( wintree_is_focused(taskbar_item_get_window(self)->uid) )
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "taskbar_active");
  else
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "taskbar_normal");

  gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(self)),
      GTK_STATE_FLAG_PRELIGHT);

  widget_set_css(self,NULL);
}

window_t *taskbar_item_get_window ( GtkWidget *self )
{
  TaskbarItemPrivate *priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  return priv->win;
}

static void taskbar_item_class_init ( TaskbarItemClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = taskbar_item_destroy;
  FLOW_ITEM_CLASS(kclass)->update = taskbar_item_update;
  FLOW_ITEM_CLASS(kclass)->get_parent = (void * (*)(GtkWidget *))taskbar_item_get_window;
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

  g_return_val_if_fail(taskbar,NULL);

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
  g_object_set_data(G_OBJECT(taskbar),"items", g_list_append(
      g_object_get_data(G_OBJECT(taskbar),"items"), self));

  g_signal_connect(button,"clicked",G_CALLBACK(taskbar_item_button_cb),self);
  g_signal_connect(self,"button_press_event",
      G_CALLBACK(taskbar_item_click_cb),self);
  gtk_widget_add_events(GTK_WIDGET(self),GDK_SCROLL_MASK);
  g_signal_connect(self,"scroll-event",
      G_CALLBACK(taskbar_item_scroll_cb),self);
  return self;
}

