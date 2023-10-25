/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include "sfwbar.h"
#include "workspace.h"
#include "pager.h"
#include "pageritem.h"
#include "taskbar.h"

G_DEFINE_TYPE_WITH_CODE (Pager, pager, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Pager));

static GList *pagers;


static GtkWidget *pager_get_child ( GtkWidget *self )
{
  PagerPrivate *priv;

  g_return_val_if_fail(IS_PAGER(self),NULL);
  priv = pager_get_instance_private(PAGER(self));

  return priv->pager;
}

static GtkWidget *pager_mirror ( GtkWidget *src )
{
  GtkWidget *self;
  PagerPrivate *spriv, *dpriv;

  g_return_val_if_fail(IS_PAGER(src),NULL);
  self = pager_new();
  dpriv = pager_get_instance_private(PAGER(self));
  spriv = pager_get_instance_private(PAGER(src));

  g_object_set_data(G_OBJECT(dpriv->pager),"preview",
      g_object_get_data(G_OBJECT(spriv->pager),"preview"));
  g_object_set_data(G_OBJECT(dpriv->pager),"sort_numeric",
      g_object_get_data(G_OBJECT(spriv->pager),"sort_numeric"));
  dpriv->pins = g_list_copy_deep(spriv->pins, (GCopyFunc)g_strdup,NULL);

  flow_grid_copy_properties(self,src);
  base_widget_copy_properties(self,src);

  return self;
}

static void pager_destroy ( GtkWidget *self )
{
  PagerPrivate *priv;

  g_return_if_fail(IS_PAGER(self));
  priv = pager_get_instance_private(PAGER(self));
  pagers = g_list_remove(pagers,self);
  g_list_free_full(g_steal_pointer(&priv->pins),g_free);
  GTK_WIDGET_CLASS(pager_parent_class)->destroy(self);
}

static void pager_class_init ( PagerClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->get_child = pager_get_child;
  BASE_WIDGET_CLASS(kclass)->mirror = pager_mirror;
  GTK_WIDGET_CLASS(kclass)->destroy = pager_destroy;
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;
}

static void pager_init ( Pager *self )
{
}

GtkWidget *pager_new ( void )
{
  PagerPrivate *priv;
  GtkWidget *self;
  GList *iter;

  self = GTK_WIDGET(g_object_new(pager_get_type(), NULL));
  priv = pager_get_instance_private(PAGER(self));

  priv->pager = flow_grid_new(TRUE);
  gtk_container_add(GTK_CONTAINER(self),priv->pager);
  pagers = g_list_prepend(pagers,self);
  g_object_set_data(G_OBJECT(priv->pager),"sort_numeric",GINT_TO_POINTER(TRUE));

  for(iter = workspace_get_list(); iter; iter=g_list_next(iter))
    pager_item_new(self, iter->data);
  flow_grid_invalidate(self);

  return self;
}

void pager_add_pin ( GtkWidget *self, gchar *pin )
{
  PagerPrivate *priv;

  g_return_if_fail(IS_PAGER(self));
  priv = pager_get_instance_private(PAGER(self));

  if(ipc_get()==IPC_SWAY || ipc_get()==IPC_HYPR)
  {
    if(!g_list_find_custom(priv->pins, pin, (GCompareFunc)g_strcmp0))
      priv->pins = g_list_prepend(priv->pins, g_strdup(pin));
    workspace_pin_add(pin);
  }
  g_free(pin);
}

gboolean pager_check_pins ( GtkWidget *self, gchar *pin )
{
  PagerPrivate *priv;

  g_return_val_if_fail(IS_PAGER(self), FALSE);
  priv = pager_get_instance_private(PAGER(self));

  return !!g_list_find_custom(priv->pins, pin, (GCompareFunc)g_strcmp0);
}

void pager_invalidate_all ( workspace_t *ws )
{
  GList *iter;

  for(iter=pagers; iter; iter=g_list_next(iter))
    flow_item_invalidate(flow_grid_find_child(iter->data,ws));
}

void pager_item_delete ( workspace_t *ws )
{
  GList *iter;

  for(iter=pagers; iter; iter=g_list_next(iter))
    if(!pager_check_pins(iter->data, ws->name))
        flow_grid_delete_child(iter->data, ws);
}

void pager_item_add ( workspace_t *ws )
{
  g_list_foreach(pagers, (GFunc)pager_item_new, ws);
}

void pager_update_all ( void )
{
  g_list_foreach(pagers, (GFunc)flow_grid_update, NULL);
}
