/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021-2022 Lev Babiev
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"
#include "switcheritem.h"
#include "switcher.h"

G_DEFINE_TYPE_WITH_CODE (Switcher, switcher, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Switcher));

static GtkWidget *switcher;
static GtkWidget *grid;
static gint interval;
static gchar hstate;
static gint counter;
static gint title_width = -1;
static GList *focus;

static GtkWidget *switcher_get_child ( GtkWidget *self )
{
  SwitcherPrivate *priv;

  g_return_val_if_fail(IS_SWITCHER(self),NULL);
  priv = switcher_get_instance_private(SWITCHER(self));

  return priv->switcher;
}

static void switcher_class_init ( SwitcherClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->get_child = switcher_get_child;
}

static void switcher_init ( Switcher *self )
{
}

GtkWidget *switcher_new ( void )
{
  GtkWidget *self;
  SwitcherPrivate *priv;

  if(grid)
    return grid;

  self = GTK_WIDGET(g_object_new(switcher_get_type(), NULL));
  priv = switcher_get_instance_private(SWITCHER(self));

  priv->switcher = flow_grid_new(FALSE);
  gtk_container_add(GTK_CONTAINER(self),priv->switcher);

  if(!switcher)
  {
    grid = self;
    gtk_widget_set_name(grid, "switcher");
    switcher = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_layer_init_for_window (GTK_WINDOW(switcher));
    gtk_layer_set_layer(GTK_WINDOW(switcher),GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_widget_set_name(switcher, "switcher");
    gtk_container_add(GTK_CONTAINER(switcher),grid);
  }

  hstate = 's';

  return self;
}

void switcher_populate ( void )
{
  GList *iter;

  if(!grid)
    return;

  interval = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(grid),"interval"));
  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(grid),"title_width"));
  if(!title_width)
    title_width = -1;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    switcher_window_init(iter->data);
}

gboolean switcher_event ( struct json_object *obj )
{
  const gchar *state,*id;
  GList *item;
  gboolean event = FALSE;

  if(!switcher)
    return TRUE;

  if(obj)
  {
    state = json_string_by_name(obj,"hidden_state");
    if(state && *state!='h')
    {
      event=TRUE;
      id = json_string_by_name(obj,"id");
      if(id)
        sway_ipc_command("bar %s hidden_state hide",id);
    }
  }
  else
    event = TRUE;

  if(event)
  {
    counter = interval;
    focus = NULL;
    for (item = wintree_get_list(); item; item = g_list_next(item) )
      if ( wintree_is_focused(((window_t *)item->data)->uid) )
        focus = g_list_next(item);
    if(focus==NULL)
      focus=wintree_get_list();
    if(focus!=NULL)
      wintree_set_focus(((window_t *)focus->data)->uid);
//    switcher_invalidate();
  }

  return TRUE;
}

void switcher_window_init ( window_t *win)
{
  if(!grid)
    return;
  flow_grid_add_child(grid,switcher_item_new(win,grid));
}

void switcher_update ( void )
{
  window_t *win;

  if(!switcher)
    return;

  if(counter <= 0)
    return;
  counter--;

  if(counter > 0)
  {
    flow_grid_update(grid);
    gtk_widget_show_all(switcher);
    widget_set_css(switcher,NULL);
  }
  else
  {
    gtk_widget_hide(switcher);
    win = focus->data;
    wintree_focus(win->uid);
  }
}

void switcher_invalidate ( window_t *win )
{
  if(grid)
    switcher_item_invalidate(flow_grid_find_child(grid,win));
}

