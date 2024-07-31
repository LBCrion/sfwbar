/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "workspace.h"
#include "pager.h"
#include "taskbarshell.h"

static struct workspace_api api;
static GList *global_pins;
static workspace_t *focus;
static GList *workspaces;
static GHashTable *actives;

void workspace_api_register ( struct workspace_api *new )
{
  if (!api.set_workspace)
    api = *new;
}

gboolean workspaces_supported ( void )
{
  return api.set_workspace != NULL;
}

void workspace_ref ( gpointer id )
{
  workspace_t *ws;

  ws = workspace_from_id(id);
  if(ws)
    ws->refcount++;
}

void workspace_unref ( gpointer id )
{
  workspace_t *ws;

  ws = workspace_from_id(id);
  if(!ws)
    return;

  ws->refcount--;
  if(ws->refcount)
    return;

  if(ws->custom_data && api.free_data)
    api.free_data(ws->custom_data);

  if(g_list_find_custom(global_pins, ws->name, (GCompareFunc)g_strcmp0))
  {
    ws->id = PAGER_PIN_ID;
    ws->state = 0;
    pager_item_delete(ws);
  }
  else
  {
    workspaces = g_list_remove(workspaces, ws);
    pager_item_delete(ws);
    g_free(ws->name);
    g_free(ws);
  }
}

workspace_t *workspace_from_id ( gpointer id )
{
  GList *iter;

  for(iter=workspaces; iter; iter=g_list_next(iter))
    if(((workspace_t *)iter->data)->id == id)
      return iter->data;

  return NULL;
}

workspace_t *workspace_from_name ( const gchar *name )
{
  GList *iter;

  for(iter=workspaces; iter; iter=g_list_next(iter))
    if(!g_strcmp0(((workspace_t *)iter->data)->name, name))
      return iter->data;
  return NULL;
}

gpointer workspace_id_from_name ( const gchar *name )
{
  workspace_t *ws;

  ws = workspace_from_name(name);
  return ws? ws->id: NULL;
}

void workspace_activate ( workspace_t *ws )
{
  if(api.set_workspace && ws)
    api.set_workspace(ws);
}

guint workspace_get_geometry ( workspace_t *ws, GdkRectangle **wins,
    GdkRectangle *spc, gint *focus)
{
  if(api.get_geom && ws)
    return api.get_geom(ws, wins, spc, focus);
  return 0;
}

void workspace_pin_add ( gchar *pin )
{
  workspace_t *ws;

  if(g_list_find_custom(global_pins, pin, (GCompareFunc)g_strcmp0))
    return;

  global_pins = g_list_prepend(global_pins, g_strdup(pin));
  if(!workspace_from_name(pin))
  {
    ws = g_malloc0(sizeof(workspace_t));
    ws->id = PAGER_PIN_ID;
    ws->name = g_strdup(pin);
    workspaces = g_list_prepend(workspaces, ws);
    pager_item_add(ws);
  }
}

GList *workspace_get_list ( void )
{
  return workspaces;
}

gpointer workspace_get_active ( GtkWidget *widget )
{
  GdkMonitor *mon;

  if(!actives)
    return NULL;

  mon = widget_get_monitor(widget);
  if(!mon)
    return NULL;
  return g_hash_table_lookup(actives,
      g_object_get_data(G_OBJECT(mon),"xdg_name"));
}

void workspace_set_active ( workspace_t *ws, const gchar *output )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon;
  gchar *name;
  gint i;

  if(!output || !ws)
    return;

  if(!actives)
    actives = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, NULL);

  gdisp = gdk_display_get_default();
  for(i=gdk_display_get_n_monitors(gdisp)-1; i>=0; i--)
  {
    gmon = gdk_display_get_monitor(gdisp, i);
    name = g_object_get_data(G_OBJECT(gmon), "xdg_name");
    if(name && !g_strcmp0(name, output))
      g_hash_table_insert(actives, g_strdup(name), ws->id);
  }
}

gboolean workspace_is_focused ( workspace_t *ws )
{
  return (ws == focus);
}

gpointer workspace_get_focused ( void )
{
  if(!focus)
    return NULL;
  else
    return focus->id;
}

void workspace_set_focus ( gpointer id )
{
  workspace_t *ws;

  ws = workspace_from_id(id);
  if(!ws || ws==focus)
    return;

  pager_invalidate_all(focus);
  focus = ws;
  pager_invalidate_all(focus);
  taskbar_shell_invalidate_all();
}

void workspace_set_name ( workspace_t *ws, const gchar *name )
{
  workspace_t *pin;
  gchar *old;

  if(!g_strcmp0(ws->name, name))
    return;

  if( (pin = workspace_from_name(name)) && pin->id != PAGER_PIN_ID)
  {
    g_message("duplicate workspace names with differing ids ('%s'/%p/%p)",
        name, pin->id, ws->id);
    return;
  }

  if(g_list_find_custom(global_pins, ws->name, (GCompareFunc)g_strcmp0))
  {
    old = ws->name;
    ws->name = (gchar *)name;
    workspace_new(ws);
    ws->name = old;
    workspace_unref(ws);
    return;
  }

  if(!pin)
  {
    g_free(ws->name);
    ws->name = g_strdup(name);
    pager_invalidate_all(ws);
    return;
  }

  pager_item_delete(ws);
  ws->name = (gchar *)name;
  workspace_new(ws);
  g_free(ws);
}

void workspace_new ( workspace_t *new )
{
  workspace_t *ws;

  ws = workspace_from_id(new->id);
  if(!ws)
  {
    ws = workspace_from_name(new->name);
    if(ws && ws->id != PAGER_PIN_ID)
      g_message("duplicate workspace names with differing ids ('%s'/%p/%p)",
          new->name, ws->id, new->id);
  }
  if(!ws)
  {
    ws = g_malloc0(sizeof(workspace_t));
    ws->refcount = 0;
    workspaces = g_list_prepend(workspaces, ws);
  }

  if(g_strcmp0(ws->name, new->name))
  {
    g_free(ws->name);
    ws->name = g_strdup(new->name);
    pager_invalidate_all(ws);
  }
  if(ws->id != new->id || ws->state != new->state)
  {
    ws->id = new->id;
    ws->state = new->state;
    pager_invalidate_all(ws);
  }
  if(ws->custom_data && api.free_data)
    api.free_data(ws->custom_data);
  ws->custom_data = new->custom_data;

  workspace_ref(ws->id);
  pager_item_add(ws);
  if(new->state & WORKSPACE_FOCUSED)
    workspace_set_focus(ws->id);
}
