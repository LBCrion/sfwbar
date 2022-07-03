/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <gtk/gtk.h>
#include "sfwbar.h"
#include "pager.h"
#include "pageritem.h"

G_DEFINE_TYPE_WITH_CODE (Pager, pager, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Pager));

static GList *pagers;
static GList *global_pins;
static workspace_t *focus;
static GList *workspaces;

static GtkWidget *pager_get_child ( GtkWidget *self )
{
  PagerPrivate *priv;

  g_return_val_if_fail(IS_PAGER(self),NULL);
  priv = pager_get_instance_private(PAGER(self));

  return priv->pager;
}

static void pager_class_init ( PagerClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->get_child = pager_get_child;
}

static void pager_init ( Pager *self )
{
}

GtkWidget *pager_new ( void )
{
  GtkWidget *self;
  PagerPrivate *priv;

  if(!sway_ipc_active())
    return NULL;

  self = GTK_WIDGET(g_object_new(pager_get_type(), NULL));
  priv = pager_get_instance_private(PAGER(self));

  priv->pager = flow_grid_new(TRUE);
  gtk_container_add(GTK_CONTAINER(self),priv->pager);
  pagers = g_list_prepend(pagers,self);
  g_object_set_data(G_OBJECT(self),"sort_numeric",GINT_TO_POINTER(TRUE));

  return self;
}

void pager_add_pin ( GtkWidget *pager, gchar *pin )
{
  GObject *child;

  if(!sway_ipc_active())
    return g_free(pin);

  global_pins = g_list_prepend(global_pins,pin);

  child = G_OBJECT(base_widget_get_child(pager));
  if(!child)
    return;

  if(!g_list_find_custom(g_object_get_data(child,"pins"),pin,
        (GCompareFunc)g_strcmp0))
    g_object_set_data(child,"pins",g_list_append(
            g_object_get_data(child,"pins"),pin));
}

static gint pager_comp_id ( workspace_t *a, gpointer id )
{
  return GPOINTER_TO_INT(a->id) - GPOINTER_TO_INT(id);
}

static gint pager_comp_name ( workspace_t *a, gchar * b)
{
  return g_strcmp0(a->name,b);
}

gboolean workspace_is_focused ( workspace_t *ws )
{
  return (ws == focus);
}

void pager_workspace_set_focus ( gpointer id )
{
  GList *item;

  item = g_list_find_custom(workspaces,id,(GCompareFunc)pager_comp_id);
  if(item)
  {
    focus = item->data;
    g_list_foreach(pagers,(GFunc)flow_grid_invalidate,NULL);
  }
}

void pager_workspace_delete ( gpointer id )
{
  GList *iter, *item;
  workspace_t *ws;

  item = g_list_find_custom(workspaces,id,
          (GCompareFunc)pager_comp_id);
  if(!item)
    return;

  ws = item->data;

  for(iter=global_pins;iter;iter=g_list_next(iter))
    if(!g_strcmp0(iter->data,ws->name))
    {
      ws->id = GINT_TO_POINTER(-1);
      ws->visible = FALSE;
      for(iter=pagers;iter;iter=g_list_next(iter))
        if(!g_list_find_custom(g_object_get_data(
                G_OBJECT(base_widget_get_child(iter->data)),"pins"),
              ws->name,(GCompareFunc)g_strcmp0))
            flow_grid_delete_child(iter->data,ws);
      return;
    }

  g_list_foreach(pagers,(GFunc)flow_grid_delete_child,ws);
  g_free(ws->name);
  g_free(ws);
  workspaces = g_list_delete_link(workspaces,item);
}

void pager_workspace_new ( workspace_t *new )
{
  GList *item;
  workspace_t *ws;

  item = g_list_find_custom(workspaces,new->id,(GCompareFunc)pager_comp_id);
  if(item)
  {
    ws = item->data;
    g_free(ws->name);
    ws->name = g_strdup(new->name);
    g_list_foreach(pagers,(GFunc)pager_item_new,ws);
    g_list_foreach(pagers,(GFunc)flow_grid_invalidate,NULL);
  }
  else
  {
    item = g_list_find_custom(workspaces,new->name,(GCompareFunc)pager_comp_name);
    if(item)
      ws = item->data;
    else
    {
      ws = g_malloc0(sizeof(workspace_t));
      ws->name = g_strdup(new->name);
      workspaces = g_list_prepend(workspaces,ws);
      g_list_foreach(pagers,(GFunc)pager_item_new,ws);
    }
    ws->id = new->id;
  }
  ws->visible = new->visible;
  if(new->focused)
  {
    g_list_foreach(pagers,(GFunc)flow_grid_invalidate,NULL);
    focus = ws;
  }
}

void pager_populate ( void )
{
  GList *item;
  workspace_t *ws;

  sway_ipc_pager_populate();

  for(item = global_pins;item;item=g_list_next(item))
    if(!g_list_find_custom(workspaces,item->data,
          (GCompareFunc)pager_comp_name))

    {
      ws = g_malloc(sizeof(workspace_t));
      ws->id = GINT_TO_POINTER(-1);
      ws->name = g_strdup(item->data);
      ws->visible = FALSE;
      workspaces = g_list_prepend(workspaces,ws);
      g_list_foreach(pagers,(GFunc)pager_item_new,ws);
    }

  g_list_foreach(pagers,(GFunc)flow_grid_update,NULL);
}

void pager_update ( void )
{
  g_list_foreach(pagers,(GFunc)flow_grid_update,NULL);
}
