/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "css.h"
#include "grid.h"
#include "flowgrid.h"
#include "taskbaritem.h"
#include "taskbarshell.h"
#include "scaleimage.h"
#include "action.h"
#include "pager.h"
#include "bar.h"
#include "wintree.h"
#include "config.h"

G_DEFINE_TYPE_WITH_CODE (TaskbarItem, taskbar_item, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE (TaskbarItem))

static gboolean taskbar_item_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  TaskbarItemPrivate *priv;
  GdkModifierType mods;
  GtkWidget *shell;
  action_t *action;

  g_return_val_if_fail(IS_TASKBAR_ITEM(self), FALSE);
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  shell = flow_item_get_parent(self);
  if(!base_widget_check_action_slot(shell, slot) && slot != 1)
    return FALSE;

  mods = base_widget_get_modifiers(self);
  action = base_widget_get_action(shell, slot, mods);

  action_exec(self, action, ev, priv->win, NULL);

  return TRUE;
}

void taskbar_item_set_image ( GtkWidget *icon, gchar *appid )
{
  gchar *ptr, *tmp;

  if(!appid || scale_image_set_image(icon, appid, NULL))
    return;
  if( (ptr = strrchr(appid, '.')) &&
    scale_image_set_image(icon, ptr+1, NULL))
    return;
  if( (ptr = strchr(appid, ' ')) )
  {
    tmp = g_strndup(appid, ptr - appid);
    scale_image_set_image(icon, tmp, NULL);
    g_free(tmp);
  }
}

window_t *taskbar_item_get_window ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_ITEM(self),NULL);
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  return priv->win;
}

static gboolean taskbar_item_check ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;
  GtkWidget *taskbar, *holder;
  gboolean floating, result;

  g_return_val_if_fail(IS_TASKBAR_ITEM(self), FALSE);
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  if( (holder = taskbar_get_parent(priv->taskbar)) )
    taskbar = flow_item_get_parent(holder);
  else
    taskbar = priv->taskbar;

  switch(taskbar_shell_get_filter(taskbar, &floating))
  {
    case G_TOKEN_OUTPUT:
      result = (!priv->win->outputs || g_list_find_custom(priv->win->outputs,
          bar_get_output(taskbar), (GCompareFunc)g_strcmp0));
      break;
    case G_TOKEN_WORKSPACE:
      result = (!priv->win->workspace ||
          priv->win->workspace==workspace_get_active(taskbar));
      break;
    default:
      result = TRUE;
      break;
  }
  if(floating)
    result = result & priv->win->floating;

  if(result)
    result = !wintree_is_filtered(priv->win);

  return result;
}

static void taskbar_item_decorate ( GtkWidget *self, gboolean labels,
    gboolean icons )
{
  TaskbarItemPrivate *priv;
  gint dir;

  g_return_if_fail(IS_TASKBAR_ITEM(self));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  if(!labels && !icons)
    labels = TRUE;

  if(!!priv->icon == icons && !!priv->label == labels)
    return;

  g_clear_pointer(&priv->label, gtk_widget_destroy);
  g_clear_pointer(&priv->icon, gtk_widget_destroy);
  gtk_widget_style_get(gtk_bin_get_child(GTK_BIN(self)), "direction", &dir,
      NULL);

  if(icons)
  {
    priv->icon = scale_image_new();
    gtk_grid_attach_next_to(GTK_GRID(priv->box), priv->icon, NULL, dir, 1,1 );
    if(priv->win)
      taskbar_item_set_image(priv->icon, priv->win->appid);
  }
  if(labels)
  {
    priv->label = gtk_label_new(priv->win?priv->win->title:"");
    gtk_label_set_ellipsize (GTK_LABEL(priv->label), PANGO_ELLIPSIZE_END);
    gtk_grid_attach_next_to(GTK_GRID(priv->box), priv->label, priv->icon, dir, 1,1 );
  }
}

static void taskbar_item_set_title_width ( GtkWidget *self, gint title_width )
{
  TaskbarItemPrivate *priv;

  g_return_if_fail(IS_TASKBAR_ITEM(self));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  if(priv->label)
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label), title_width);
}

static void taskbar_item_update ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;
  gchar *appid;

  g_return_if_fail(IS_TASKBAR_ITEM(self));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));
  if(!priv->invalid)
    return;

  css_set_class(gtk_bin_get_child(GTK_BIN(self)), "minimized",
      priv->win->state & WS_MINIMIZED);
  css_set_class(gtk_bin_get_child(GTK_BIN(self)), "maximized",
      priv->win->state & WS_MAXIMIZED);
  css_set_class(gtk_bin_get_child(GTK_BIN(self)), "fullscreen",
      priv->win->state & WS_FULLSCREEN);
  css_set_class(gtk_bin_get_child(GTK_BIN(self)), "urgent",
      priv->win->state & WS_URGENT);
  css_set_class(gtk_bin_get_child(GTK_BIN(self)), "focused",
      wintree_is_focused(priv->win->uid));

  if(priv->label)
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)), priv->win->title))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->win->title);

  if(priv->icon)
  {
    if(priv->win->appid && *(priv->win->appid))
      appid = priv->win->appid;
    else
      appid = wintree_appid_map_lookup(priv->win->title);
    taskbar_item_set_image(priv->icon, appid);
  }

  gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(self)),
      GTK_STATE_FLAG_PRELIGHT);

  flow_item_set_active(self, taskbar_item_check(self));

  priv->invalid = FALSE;
}

static gint taskbar_item_compare ( GtkWidget *a, GtkWidget *b, GtkWidget *parent )
{
  TaskbarItemPrivate *p1,*p2;

  g_return_val_if_fail(IS_TASKBAR_ITEM(a),0);
  g_return_val_if_fail(IS_TASKBAR_ITEM(b),0);

  p1 = taskbar_item_get_instance_private(TASKBAR_ITEM(a));
  p2 = taskbar_item_get_instance_private(TASKBAR_ITEM(b));
  return wintree_compare(p1->win, p2->win);
}

static void taskbar_item_invalidate ( GtkWidget *self )
{
  TaskbarItemPrivate *priv;
  GtkWidget *holder;

  if(!self)
    return;

  g_return_if_fail(IS_TASKBAR_ITEM(self));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  priv->invalid = TRUE;
  flow_grid_invalidate(priv->taskbar);
  if( (holder = taskbar_get_parent(priv->taskbar)) )
      flow_item_invalidate(holder);
}

static void taskbar_item_class_init ( TaskbarItemClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->action_exec = taskbar_item_action_exec;
  FLOW_ITEM_CLASS(kclass)->update = taskbar_item_update;
  FLOW_ITEM_CLASS(kclass)->decorate = taskbar_item_decorate;
  FLOW_ITEM_CLASS(kclass)->set_title_width = taskbar_item_set_title_width;
  FLOW_ITEM_CLASS(kclass)->invalidate = taskbar_item_invalidate;
  FLOW_ITEM_CLASS(kclass)->get_source =
    (void * (*)(GtkWidget *))taskbar_item_get_window;
  FLOW_ITEM_CLASS(kclass)->compare = taskbar_item_compare;
}

static void taskbar_item_init ( TaskbarItem *self )
{
}

GtkWidget *taskbar_item_new( window_t *win, GtkWidget *taskbar )
{
  GtkWidget *self;
  TaskbarItemPrivate *priv;
  GtkWidget *button;
  gint dir;
  gint title_width;

  g_return_val_if_fail(IS_TASKBAR(taskbar),NULL);

  if(flow_grid_find_child(taskbar, win))
    return NULL;

  self = GTK_WIDGET(g_object_new(taskbar_item_get_type(), NULL));
  priv = taskbar_item_get_instance_private(TASKBAR_ITEM(self));

  priv->win = win;
  priv->taskbar = taskbar;

  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar), "title_width"));
  if(!title_width)
    title_width = -1;

  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(self), button);
  gtk_widget_set_name(button, "taskbar_item");
  gtk_widget_style_get(button,"direction", &dir, NULL);
  priv->box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(button), priv->box);
  flow_grid_child_dnd_enable(taskbar, self, button);

  priv->actions = g_object_get_data(G_OBJECT(taskbar), "actions");
  g_object_ref_sink(G_OBJECT(self));
  flow_grid_add_child(taskbar, self);

  gtk_widget_add_events(self, GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
  taskbar_item_invalidate(self);

  return self;
}
