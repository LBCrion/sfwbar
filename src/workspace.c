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

  g_debug("Workspace: destroying workspace: '%s'", str_get(ws->name));

  if(ws == focus)
    focus = NULL;

  if(g_list_find_custom(global_pins, ws->name, (GCompareFunc)g_strcmp0))
  {
    g_debug("Workspace: workspace returned to a pin: '%s'", str_get(ws->name));
    ws->id = PAGER_PIN_ID;
    ws->state = 0;
    LISTENER_CALL(workspace_destroy, ws);
  }
  else
  {
    workspaces = g_list_remove(workspaces, ws);
    LISTENER_CALL(workspace_destroy, ws);
    g_free(str_get(ws->name));
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

gboolean workspace_check_monitor ( gpointer wsid, gchar *output )
{
  return api->check_monitor? api->check_monitor(wsid, output) : TRUE;
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
  GList *iter;

  for(iter=workspaces; iter; iter=g_list_next(iter))
    if(!g_strcmp0(WORKSPACE(iter->data)->name, pin) &&
        WORKSPACE(iter->data)->id == PAGER_PIN_ID)
      break;

  if(!iter)
    return;

  ws = iter->data;
  str_assign(&ws->name, "");
  LISTENER_CALL(workspace_destroy, ws);
  workspaces = g_list_remove(workspaces, ws);
  g_free(ws);
}

static void workspace_pin_restore ( const gchar *pin )
{
  workspace_t *ws;

  if(workspace_from_name(pin))
    return;

  if(!g_list_find_custom(global_pins, pin, (GCompareFunc)g_strcmp0))
    return;

  ws = g_malloc0(sizeof(workspace_t));
  ws->id = PAGER_PIN_ID;
  str_assign(&ws->name, g_strdup(pin));
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

gpointer workspace_get_focused ( void )
{
  return focus? focus->id : NULL;
}

void workspace_commit ( workspace_t *ws )
{
  if(!ws || !(ws->state & WS_STATE_INVALID))
    return;

  ws->state &= ~WS_STATE_INVALID;
  if(ws->state & WS_STATE_FOCUSED)
    focus = ws;

  LISTENER_CALL(workspace_invalidate, ws);
}

void workspace_mod_state ( gpointer id, gint32 mask, gboolean state )
{
  workspace_t *ws;

  if( !(ws = workspace_from_id(id)) )
    return;
  if((ws->state & WS_STATE_ALL) == state)
    return;

  if(state)
    ws->state |= (mask & WS_STATE_ALL);
  else
    ws->state &= ~mask;
  ws->state |= WS_STATE_INVALID;
}

void workspace_change_focus ( gpointer id )
{
  if( focus && (id == focus->id) )
    return;

  if(focus)
  {
    workspace_mod_state(focus->id, WS_STATE_FOCUSED, FALSE);
    workspace_commit(focus);
  }
  workspace_mod_state(id, WS_STATE_FOCUSED, TRUE);
}

void workspace_set_name ( workspace_t *ws, const gchar *name )
{
  workspace_t *old;
  GList *old_pin;

  if(!g_strcmp0(str_get(ws->name), name))
    return;

  old = workspace_from_name(name);
  workspace_pin_remove(name);

  old_pin = g_list_find_custom(global_pins, str_get(ws->name),
      (GCompareFunc)g_strcmp0);

  g_debug("Workspace: '%s' (pin: %s)  name change to: '%s' (duplicate: %s)",
      str_get(ws->name), old_pin?"yes":"no", name, old?"yes":"no");
  str_assign(&ws->name, g_strdup(name));
  ws->state |= WS_STATE_INVALID;

  if(old_pin && !workspace_from_name(old_pin->data))
    workspace_pin_restore(old_pin->data);
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
