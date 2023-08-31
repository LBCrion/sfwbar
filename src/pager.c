/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include "sfwbar.h"
#include "pager.h"
#include "pageritem.h"
#include "taskbar.h"

G_DEFINE_TYPE_WITH_CODE (Pager, pager, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Pager));

static struct pager_api api;
static GList *pagers;
static GList *global_pins;
static workspace_t *focus;
static GList *workspaces;
static GHashTable *actives;

void pager_api_register ( struct pager_api *new )
{
  api = *new;
}

void pager_set_workspace ( workspace_t *ws )
{
  if(api.set_workspace)
    api.set_workspace(ws);
}

guint pager_get_geom ( workspace_t *ws, GdkRectangle **wins, GdkRectangle *spc,
    gint *focus)
{
  if(api.get_geom)
    return api.get_geom(ws,wins,spc,focus);
  return 0;
}

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
  g_object_set_data(G_OBJECT(dpriv->pager),"pins",g_list_copy_deep(
      g_object_get_data(G_OBJECT(spriv->pager),"pins"),
      (GCopyFunc)g_strdup,NULL));

  flow_grid_copy_properties(self,src);
  base_widget_copy_properties(self,src);
  pager_populate();

  return self;
}

static void pager_destroy ( GtkWidget *self )
{
  PagerPrivate *priv;
  priv = pager_get_instance_private(PAGER(self));

  pagers = g_list_remove(pagers,self);
  g_list_free_full(g_object_get_data(G_OBJECT(priv->pager),"pins"),g_free);
  g_object_set_data(G_OBJECT(priv->pager),"pins",NULL);
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
  GtkWidget *self;
  PagerPrivate *priv;

  self = GTK_WIDGET(g_object_new(pager_get_type(), NULL));
  priv = pager_get_instance_private(PAGER(self));

  priv->pager = flow_grid_new(TRUE);
  gtk_container_add(GTK_CONTAINER(self),priv->pager);
  pagers = g_list_prepend(pagers,self);
  g_object_set_data(G_OBJECT(priv->pager),"sort_numeric",GINT_TO_POINTER(TRUE));

  return self;
}

void pager_add_pin ( GtkWidget *pager, gchar *pin )
{
  GObject *child;

  if(ipc_get()!=IPC_SWAY && ipc_get()!=IPC_HYPR)
    return g_free(pin);

  if(!g_list_find_custom(global_pins,pin,(GCompareFunc)g_strcmp0))
    global_pins = g_list_prepend(global_pins,pin);

  child = G_OBJECT(base_widget_get_child(pager));
  if(!child)
    return;

  if(!g_list_find_custom(g_object_get_data(child,"pins"),pin,
        (GCompareFunc)g_strcmp0))
    g_object_set_data(child,"pins",g_list_prepend(
            g_object_get_data(child,"pins"),pin));
}

void pager_invalidate_all ( workspace_t *ws )
{
  GList *iter;

  for(iter=pagers; iter; iter=g_list_next(iter))
    flow_item_invalidate(flow_grid_find_child(iter->data,ws));
}

static gint pager_comp_id ( workspace_t *a, gpointer id )
{
  return GPOINTER_TO_INT(a->id) - GPOINTER_TO_INT(id);
}

static gint pager_comp_name ( workspace_t *a, gchar * b)
{
  return g_strcmp0(a->name,b);
}

workspace_t *pager_workspace_from_id ( gpointer id )
{
  GList *item;

  item = g_list_find_custom(workspaces,id,(GCompareFunc)pager_comp_id);
  if(item)
    return item->data;
  return NULL;
}

workspace_t *pager_workspace_from_name ( gchar *name )
{
  GList *item;

  item = g_list_find_custom(workspaces,name,(GCompareFunc)pager_comp_name);
  if(item)
    return item->data;
  return NULL;
}

gpointer pager_workspace_get_active ( GtkWidget *widget )
{
  if(!actives)
    return NULL;

  return g_hash_table_lookup(actives,
      g_object_get_data(G_OBJECT(widget_get_monitor(widget)),"xdg_name"));
}

void pager_workspace_set_active ( workspace_t *ws, const gchar *output )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon;
  gchar *name;
  gint i;

  if(!output || !ws)
    return;

  if(!actives)
    actives = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal,g_free,NULL);

  gdisp = gdk_display_get_default();
  for(i=gdk_display_get_n_monitors(gdisp)-1;i>=0;i--)
  {
    gmon = gdk_display_get_monitor(gdisp,i);
    name = g_object_get_data(G_OBJECT(gmon),"xdg_name");
    if(name && !g_strcmp0(name,output))
      g_hash_table_insert(actives,g_strdup(name),ws->id);
  }
}

gpointer pager_workspace_id_from_name ( const gchar *name )
{
  GList *iter;

  for(iter=workspaces;iter;iter=g_list_next(iter))
    if(!g_strcmp0(name,((workspace_t *)iter->data)->name))
      return ((workspace_t *)iter->data)->id;
  return NULL;
}

gboolean pager_workspace_is_focused ( workspace_t *ws )
{
  return (ws == focus);
}

gpointer pager_get_focused ( void )
{
  if(!focus)
    return NULL;
  else
    return focus->id;
}

void pager_workspace_set_focus ( gpointer id )
{
  workspace_t *ws;

  ws = pager_workspace_from_id(id);
  if(!ws || ws==focus)
    return;

  pager_invalidate_all(focus);
  focus = ws;
  pager_invalidate_all(focus);
  taskbar_invalidate_conditional();
}

void pager_workspace_delete ( gpointer id )
{
  workspace_t *ws;
  GList *iter;

  ws = pager_workspace_from_id(id);
  if(!ws)
    return;

  if(g_list_find_custom(global_pins,ws->name,(GCompareFunc)g_strcmp0))
  {
    ws->active = FALSE;
    ws->visible = FALSE;
    for(iter=pagers; iter; iter=g_list_next(iter))
      if(!g_list_find_custom(g_object_get_data(
              G_OBJECT(base_widget_get_child(iter->data)),"pins"),
            ws->name,(GCompareFunc)g_strcmp0))
          flow_grid_delete_child(iter->data,ws);
  }
  else
  {
    g_list_foreach(pagers,(GFunc)flow_grid_delete_child,ws);
    ws->active = FALSE;
  }
}

void pager_workspace_new ( workspace_t *new )
{
  workspace_t *ws;
  GList *iter;

  ws = pager_workspace_from_id(new->id);
  if(!ws)
    for(iter=workspaces; iter; iter=g_list_next(iter))
      if(!g_strcmp0(((workspace_t *)iter->data)->name,new->name) &&
          !((workspace_t *)iter->data)->active)
        ws = iter->data;
  if(!ws)
  {
    ws = g_malloc0(sizeof(workspace_t));
    workspaces = g_list_prepend(workspaces,ws);
  }

  if(g_strcmp0(ws->name, new->name))
  {
    g_free(ws->name);
    ws->name = g_strdup(new->name);
    pager_invalidate_all(ws);
  }
  if(ws->id != new->id || ws->visible != new->visible)
  {
    ws->id = new->id;
    ws->visible = new->visible;
    pager_invalidate_all(ws);
  }
  ws->active = TRUE;
  g_list_foreach(pagers,(GFunc)pager_item_new,ws);

  if(new->focused)
    pager_workspace_set_focus(ws->id);
}

void pager_populate ( void )
{
  GList *item;
  workspace_t *ws;
  
  if(!pagers)
    return;

  for(item = workspaces;item;item=g_list_next(item))
    g_list_foreach(pagers,(GFunc)pager_item_new,item->data);

  for(item = global_pins;item;item=g_list_next(item))
    if(!g_list_find_custom(workspaces,item->data,
          (GCompareFunc)pager_comp_name))

    {
      ws = g_malloc(sizeof(workspace_t));
      ws->active = FALSE;
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
