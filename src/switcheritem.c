/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"
#include "switcheritem.h"

G_DEFINE_TYPE_WITH_CODE (SwitcherItem, switcher_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (SwitcherItem));

static void switcher_item_destroy ( GtkWidget *self )
{
}

void switcher_item_update ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;

  g_return_if_fail(IS_SWITCHER_ITEM(self));

  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  if(priv->label)
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)),priv->win->title))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->win->title);

  if(priv->icon)
    scale_image_set_image(priv->icon,priv->win->appid,NULL);

  if ( wintree_is_focused(switcher_item_get_window(self)->uid) )
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "switcher_active");
  else
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "switcher_normal");

  gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(self)),
      GTK_STATE_FLAG_PRELIGHT);

  widget_set_css(self,NULL);
}

window_t *switcher_item_get_window ( GtkWidget *self )
{
  SwitcherItemPrivate *priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  return priv->win;
}

static void switcher_item_class_init ( SwitcherItemClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = switcher_item_destroy;
  FLOW_ITEM_CLASS(kclass)->update = switcher_item_update;
  FLOW_ITEM_CLASS(kclass)->get_parent = (void * (*)(GtkWidget *))switcher_item_get_window;
}

static void switcher_item_init ( SwitcherItem *cgrid )
{
}

GtkWidget *switcher_item_new( window_t *win, GtkWidget *switcher )
{
  GtkWidget *self,*grid;
  SwitcherItemPrivate *priv;
  gboolean icons,labels;
  gint dir;
  gint title_width;

  if(!switcher)
  {
    win->switcher = NULL;
    return NULL;
  }

  self = GTK_WIDGET(g_object_new(switcher_item_get_type(), NULL));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  priv->win = win;
  priv->switcher = switcher;

  icons = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(switcher),"icons"));
  labels = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(switcher),"labels"));
  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(switcher),"title_width"));
  if(!title_width)
    title_width = -1;
  if(!icons)
    labels = TRUE;

  win->switcher = self;
  grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(self),grid);
  gtk_widget_set_name(grid, "switcher_normal");
  gtk_widget_style_get(grid,"direction",&dir,NULL);
  g_object_ref(G_OBJECT(self));
  if(icons)
  {
    priv->icon = scale_image_new();
    scale_image_set_image(priv->icon,win->appid,NULL);
    gtk_grid_attach_next_to(GTK_GRID(grid),priv->icon,NULL,dir,1,1);
  }
  else
    priv->icon = NULL;
  if(labels)
  {
    priv->label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(priv->label),PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label),title_width);
    gtk_grid_attach_next_to(GTK_GRID(grid),priv->label,
        priv->icon,dir,1,1);
  }
  else
    priv->label = NULL;

  return self;
}

gint switcher_item_compare ( GtkWidget *a, GtkWidget *b, GtkWidget *parent )
{
  SwitcherItemPrivate *p1,*p2;

  g_return_val_if_fail(IS_SWITCHER_ITEM(a),0);
  g_return_val_if_fail(IS_SWITCHER_ITEM(b),0);

  p1 = switcher_item_get_instance_private(SWITCHER_ITEM(a));
  p2 = switcher_item_get_instance_private(SWITCHER_ITEM(b));
  return wintree_compare(p1->win,p2->win);
}
