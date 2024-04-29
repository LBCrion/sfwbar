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

G_DEFINE_TYPE_WITH_CODE (Pager, pager, FLOW_GRID_TYPE, G_ADD_PRIVATE (Pager))

static GList *pagers;

static GtkWidget *pager_mirror ( GtkWidget *src )
{
  GtkWidget *self;
  PagerPrivate *spriv, *dpriv;

  g_return_val_if_fail(IS_PAGER(src),NULL);
  self = pager_new();
  dpriv = pager_get_instance_private(PAGER(self));
  spriv = pager_get_instance_private(PAGER(src));

  g_object_set_data(G_OBJECT(self), "preview",
      g_object_get_data(G_OBJECT(src), "preview"));
  g_object_set_data(G_OBJECT(self), "sort_numeric",
      g_object_get_data(G_OBJECT(src), "sort_numeric"));
  dpriv->pins = g_list_copy_deep(spriv->pins, (GCopyFunc)g_strdup,NULL);

  flow_grid_copy_properties(self, src);

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
  BASE_WIDGET_CLASS(kclass)->mirror = pager_mirror;
  GTK_WIDGET_CLASS(kclass)->destroy = pager_destroy;
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;
}

static void pager_init ( Pager *self )
{
  GList *iter;

  pagers = g_list_prepend(pagers, self);
  g_object_set_data(G_OBJECT(self), "sort_numeric", GINT_TO_POINTER(TRUE));

  for(iter = workspace_get_list(); iter; iter=g_list_next(iter))
    pager_item_new(GTK_WIDGET(self), iter->data);
  flow_grid_invalidate(GTK_WIDGET(self));
}

GtkWidget *pager_new ( void )
{
  return GTK_WIDGET(g_object_new(pager_get_type(), NULL));
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
    flow_item_invalidate(flow_grid_find_child(iter->data, ws));
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
