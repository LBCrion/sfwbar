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

static void pager_mirror ( GtkWidget *self, GtkWidget *src )
{
  g_return_if_fail(IS_PAGER(self));
  g_return_if_fail(IS_PAGER(src));

  BASE_WIDGET_CLASS(pager_parent_class)->mirror(self, src);
  g_object_set_data(G_OBJECT(self), "preview",
      g_object_get_data(G_OBJECT(src), "preview"));
}

static void pager_destroy ( GtkWidget *self )
{
  PagerPrivate *priv;

  g_return_if_fail(IS_PAGER(self));
  priv = pager_get_instance_private(PAGER(self));
  pagers = g_list_remove(pagers, self);
  g_list_free_full(g_steal_pointer(&priv->pins), g_free);
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

  if(ipc_get()!=IPC_SWAY && ipc_get()!=IPC_HYPR)
    css_add_class(GTK_WIDGET(self), "hidden");

  for(iter = workspace_get_list(); iter; iter=g_list_next(iter))
    pager_item_new(GTK_WIDGET(self), iter->data);
  flow_grid_invalidate(GTK_WIDGET(self));
}

void pager_add_pins ( GtkWidget *self, GList *pins )
{
  PagerPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_PAGER(self));
  priv = pager_get_instance_private(PAGER(self));

  if(ipc_get()!=IPC_SWAY && ipc_get()!=IPC_HYPR)
  {
    g_list_free_full(pins, (GDestroyNotify)g_free);
    return;
  }

  for(iter=pins; iter; iter=g_list_next(iter))
    if(!g_list_find_custom(priv->pins, iter->data, (GCompareFunc)g_strcmp0))
    {
      priv->pins = g_list_prepend(priv->pins, iter->data);
      workspace_pin_add(iter->data);
    }
    else
      g_free(iter->data);
  g_list_free(pins);

}

gboolean pager_check_pins ( GtkWidget *self, gchar *pin )
{
  PagerPrivate *priv;

  g_return_val_if_fail(IS_PAGER(self), FALSE);
  priv = pager_get_instance_private(PAGER(base_widget_get_mirror_parent(self)));

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
