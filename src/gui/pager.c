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

enum {
  PAGER_PREVIEW = 1,
  PAGER_PINS,
};

static void pager_mirror ( GtkWidget *self, GtkWidget *src )
{
  g_return_if_fail(IS_PAGER(self));
  g_return_if_fail(IS_PAGER(src));

  BASE_WIDGET_CLASS(pager_parent_class)->mirror(self, src);
  g_object_bind_property(G_OBJECT(src), "preview", G_OBJECT(self), "preview",
      G_BINDING_SYNC_CREATE);
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
  g_clear_pointer(&priv->pins, g_ptr_array_unref);
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

static void pager_get_property ( GObject *self, guint id, GValue *value,
    GParamSpec *spec )
{
  PagerPrivate *priv;

  priv = pager_get_instance_private(PAGER(self));
  switch(id)
  {
    case PAGER_PREVIEW:
      g_value_set_boolean(value, priv->preview);
      break;
    case PAGER_PINS:
      g_value_set_boxed(value, priv->pins);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void pager_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  PagerPrivate *priv;

  priv = pager_get_instance_private(PAGER(self));
  switch(id)
  {
    case PAGER_PREVIEW:
      priv->preview = g_value_get_boolean(value);
      break;
    case PAGER_PINS:
      pager_add_pins(GTK_WIDGET(self), g_value_get_boxed(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void pager_class_init ( PagerClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->mirror = pager_mirror;
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;

  GTK_WIDGET_CLASS(kclass)->destroy = pager_destroy;

  G_OBJECT_CLASS(kclass)->get_property = pager_get_property;
  G_OBJECT_CLASS(kclass)->set_property = pager_set_property;

  g_object_class_install_property(G_OBJECT_CLASS(kclass), PAGER_PREVIEW,
      g_param_spec_boolean("preview", "preview", "sfwbar_config", FALSE,
        G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), PAGER_PINS,
      g_param_spec_boxed("pins", "pins", "sfwbar_config", G_TYPE_PTR_ARRAY,
        G_PARAM_READWRITE));
}

static void pager_init ( Pager *self )
{
  PagerPrivate *priv;

  priv = pager_get_instance_private(PAGER(self));
  priv->timer_h = g_timeout_add(100, (GSourceFunc)flow_grid_update, self);
  priv->pins = g_ptr_array_new();

  if(!workspace_api_check())
    css_add_class(GTK_WIDGET(self), "hidden");
  flow_grid_invalidate(GTK_WIDGET(self));
  workspace_listener_register(&pager_listener, self);
}

static gboolean pager_pin_cmp ( gconstpointer p1, gconstpointer p2 )
{
  return !g_strcmp0(p1, p2);
}

void pager_add_pins ( GtkWidget *self, GPtrArray *pins )
{
  PagerPrivate *priv;
  guint i;

  g_return_if_fail(IS_PAGER(self));
  priv = pager_get_instance_private(PAGER(self));

  if(workspace_api_check())
  {
    for(i=0; i<pins->len; i++)
      if(!g_ptr_array_find_with_equal_func(priv->pins, pins->pdata[i],
            pager_pin_cmp, NULL))
      {
        g_ptr_array_add(priv->pins, g_strdup(pins->pdata[i]));
        workspace_pin_add(pins->pdata[i]);
      }
  }
  g_ptr_array_unref(pins);
}

gboolean pager_check_pins ( GtkWidget *self, gchar *pin )
{
  PagerPrivate *priv;

  g_return_val_if_fail(IS_PAGER(self), FALSE);
  priv = pager_get_instance_private(PAGER(base_widget_get_mirror_parent(self)));

  return priv->pins && g_ptr_array_find_with_equal_func(priv->pins, pin,
      pager_pin_cmp, NULL);
}
