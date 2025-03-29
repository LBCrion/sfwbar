/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "css.h"
#include "workspace.h"
#include "pager.h"
#include "pageritem.h"
#include "taskbar.h"

G_DEFINE_TYPE_WITH_CODE (Pager, pager, FLOW_GRID_TYPE, G_ADD_PRIVATE (Pager))

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
  workspace_listener_remove(self);
  priv = pager_get_instance_private(PAGER(self));
  if(priv->timer_h)
  {
    g_source_remove(priv->timer_h);
    priv->timer_h = 0;
  }
  g_list_free_full(g_steal_pointer(&priv->pins), g_free);
  GTK_WIDGET_CLASS(pager_parent_class)->destroy(self);
}

void pager_invalidate_item ( workspace_t *ws, GtkWidget *pager )
{
  flow_item_invalidate(flow_grid_find_child(pager, ws));
}

void pager_destroy_item ( workspace_t *ws, void *pager )
{
  if(!pager_check_pins(pager, ws->name))
    flow_grid_delete_child(pager, ws);
}

static workspace_listener_t pager_listener = {
  .workspace_new = (void (*)(workspace_t *, void *))pager_item_new,
  .workspace_invalidate = (void (*)(workspace_t *, void *))pager_invalidate_item,
  .workspace_destroy = pager_destroy_item,
};

static void pager_class_init ( PagerClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->mirror = pager_mirror;
  GTK_WIDGET_CLASS(kclass)->destroy = pager_destroy;
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;
}

static void pager_init ( Pager *self )
{
  PagerPrivate *priv;

  priv = pager_get_instance_private(PAGER(self));
  priv->timer_h = g_timeout_add(100, (GSourceFunc)flow_grid_update, self);

  if(!workspace_api_check())
    css_add_class(GTK_WIDGET(self), "hidden");
  flow_grid_invalidate(GTK_WIDGET(self));
  workspace_listener_register(&pager_listener, self);
}

void pager_add_pins ( GtkWidget *self, GList *pins )
{
  PagerPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_PAGER(self));
  priv = pager_get_instance_private(PAGER(self));

  if(!workspace_api_check())
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
