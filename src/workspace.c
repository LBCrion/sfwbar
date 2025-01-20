/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "workspace.h"
#include "gui/monitor.h"
#include "util/string.h"

static struct workspace_api *api;
static workspace_t *focus;
static GList *global_pins;
static GList *workspaces;
static GList *workspace_listeners;
static GHashTable *actives;

#define LISTENER_CALL(method, ws) { \
  for(GList *li=workspace_listeners; li; li=li->next) \
    if(WORKSPACE_LISTENER(li->data)->method) \
      WORKSPACE_LISTENER(li->data)->method(ws, \
          WORKSPACE_LISTENER(li->data)->data); \
}

void workspace_api_register ( struct workspace_api *new )
{
  api = new;
}

gboolean workspace_api_check ( void )
{
  return !!api;
}

void workspace_listener_register ( workspace_listener_t *listener, void *data )
{
  workspace_listener_t *copy;
  GList *iter;

  if(!listener)
    return;

  copy = g_memdup(listener, sizeof(workspace_listener_t));
  copy->data = data;
  workspace_listeners = g_list_append(workspace_listeners, copy);

  if(copy->workspace_new)
  {
    for(iter=workspaces; iter; iter=g_list_next(iter))
      copy->workspace_new(iter->data, copy->data);
  }
}

void workspace_listener_remove ( void *data )
{
  GList *iter;

  for(iter=workspace_listeners; iter; iter=g_list_next(iter))
    if(WORKSPACE_LISTENER(iter->data)->data == data)
      break;
  if(iter)
    workspace_listeners = g_list_remove(workspace_listeners, iter->data);
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

  if( !(ws = workspace_from_id(id)) )
    return;

  ws->refcount--;
  if(ws->refcount)
    return;

  g_debug("Workspace: destroying workspace: '%s'", ws->name);

  if(ws == focus)
    focus = NULL;

  if(g_list_find_custom(global_pins, ws->name, (GCompareFunc)g_strcmp0))
  {
    g_debug("Workspace: workspace returned to a pin: '%s'", ws->name);
    ws->id = PAGER_PIN_ID;
    ws->state = 0;
    LISTENER_CALL(workspace_destroy, ws);
  }
  else
  {
    workspaces = g_list_remove(workspaces, ws);
    LISTENER_CALL(workspace_destroy, ws);
    g_free(ws->name);
    g_free(ws);
  }
}

workspace_t *workspace_from_id ( gpointer id )
{
  GList *iter;

  for(iter=workspaces; iter; iter=g_list_next(iter))
    if(WORKSPACE(iter->data)->id == id)
      return iter->data;

  return NULL;
}

workspace_t *workspace_from_name ( const gchar *name )
{
  GList *iter;

  for(iter=workspaces; iter; iter=g_list_next(iter))
    if(!g_strcmp0(WORKSPACE(iter->data)->name, name))
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
  if(api->set_workspace && ws)
    api->set_workspace(ws);
}

guint workspace_get_geometry ( gpointer wid, GdkRectangle *wloc, gpointer wsid,
    GdkRectangle **wins, GdkRectangle *spc, gint *focus)
{
  if(api->get_geom && wsid)
    return api->get_geom(wid, wloc, wsid, wins, spc, focus);
  return 0;
}

gchar *workspace_get_monitor ( gpointer wsid )
{
  if(api->get_monitor && wsid)
    return api->get_monitor(wsid);
  return NULL;
}

gboolean workspace_get_can_create ( void )
{
  if(api->get_can_create)
    return api->get_can_create();
  return TRUE;
}

static void workspace_pin_remove ( const gchar *pin )
{
  workspace_t *ws;

  if( !(ws=workspace_from_name(pin)) || ws->id != PAGER_PIN_ID)
    return;

  g_free(ws->name);
  ws->name = "";
  LISTENER_CALL(workspace_destroy, ws);
  workspaces = g_list_remove(workspaces, ws);
  g_free(ws);
}

static void workspace_pin_restore ( const gchar *pin )
{
  workspace_t *ws;

  if(!g_list_find_custom(global_pins, pin, (GCompareFunc)g_strcmp0))
    return;

  if(workspace_from_name(pin))
    return;

  ws = g_malloc0(sizeof(workspace_t));
  ws->id = PAGER_PIN_ID;
  ws->name = g_strdup(pin);
  workspaces = g_list_prepend(workspaces, ws);
  LISTENER_CALL(workspace_new, ws);
}

void workspace_pin_add ( gchar *pin )
{
  if(g_list_find_custom(global_pins, pin, (GCompareFunc)g_strcmp0))
    return;

  global_pins = g_list_prepend(global_pins, g_strdup(pin));
  workspace_pin_restore(pin);
}

GList *workspace_get_list ( void )
{
  return workspaces;
}

gpointer workspace_get_active ( GtkWidget *widget )
{
  GdkMonitor *mon;

  if(!actives || !(mon = monitor_from_widget(widget)) )
    return NULL;
  return g_hash_table_lookup(actives, monitor_get_name(mon));
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
    name = monitor_get_name(gmon);
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

void workspace_commit ( workspace_t *ws )
{
  if(!ws || !(ws->state & WS_STATE_INVALID))
    return;

  ws->state &= ~WS_STATE_INVALID;

  if(ws->state & WS_STATE_FOCUSED && ws != focus)
  {
    if(focus)
      focus->state &= ~WS_STATE_FOCUSED;
    LISTENER_CALL(workspace_invalidate, focus);
    focus = ws;
    LISTENER_CALL(workspace_invalidate, focus);
  }
  else
    LISTENER_CALL(workspace_invalidate, ws);
}

void workspace_set_focus ( gpointer id )
{
  workspace_t *ws;

  if( !(ws = workspace_from_id(id)) )
    return;

  ws->state |= WS_STATE_FOCUSED | WS_STATE_INVALID;
  workspace_commit(ws);
}

void workspace_set_name ( workspace_t *ws, const gchar *name )
{
  workspace_t *pin;
  GList *oldp;

  if(!g_strcmp0(ws->name, name))
    return;

  if( (pin = workspace_from_name(name)) && pin->id != PAGER_PIN_ID)
  {
    g_message("Workspace: duplicate names with differing ids ('%s'/%p/%p)",
        name, pin->id, ws->id);
    return;
  }

  if(pin)
    workspace_pin_remove(name);

  oldp = g_list_find_custom(global_pins, ws->name, (GCompareFunc)g_strcmp0);

  g_debug("Workspace: '%s' (pin: %s)  name change to: '%s' (pin: %s)",
      ws->name, oldp?"yes":"no", name, pin?"yes":"no");
  g_free(ws->name);
  ws->name = g_strdup(name);
  ws->state |= WS_STATE_INVALID;

  if(oldp && !workspace_from_name(oldp->data))
    workspace_pin_restore(oldp->data);
}

void workspace_set_state ( workspace_t *ws, guint32 state )
{
  if(!ws)
    return;

  if((state & ~WS_STATE_INVALID) ==
      ((ws->state & WS_STATE_ALL) & ~WS_STATE_INVALID) )
    return;

  ws->state = (ws->state & WS_CAP_ALL) | state | WS_STATE_INVALID;
  workspace_commit(ws);

  g_debug("Workspace: '%s' state change: focused: %s, visible: %s, urgent: %s",
      ws->name, ws->state & WS_STATE_FOCUSED ? "yes" : "no",
      ws->state & WS_STATE_VISIBLE ? "yes" : "no",
      ws->state & WS_STATE_URGENT ? "yes" : "no");
}

void workspace_set_caps ( workspace_t *ws, guint32 caps )
{
  if(!ws)
    return;

  ws->state = (ws->state & WS_STATE_ALL) | caps | WS_STATE_INVALID;
}

workspace_t *workspace_new ( gpointer id )
{
  workspace_t *ws;

  if( !(ws = workspace_from_id(id)) )
  {
    ws = g_malloc0(sizeof(workspace_t));
    ws->id = id;
    ws->refcount = 0;
    workspaces = g_list_append(workspaces, ws);
    workspace_ref(id);
    LISTENER_CALL(workspace_new, ws);
  }

  return ws;
}
