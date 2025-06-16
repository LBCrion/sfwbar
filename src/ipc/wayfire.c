/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include "wintree.h"
#include "util/json.h"
#include "gui/monitor.h"
#include <sys/socket.h>

extern gchar *sockname;

typedef struct _wayfire_ipc_wset {
  gint id;
  gint index;
  gint output;
  gint w, h, x, y;
} wayfire_ipc_wset_t;

typedef struct _wayfire_ipc_output {
  gchar *name;
  gint id;
  GdkRectangle geo;
  GdkRectangle work;
} wayfire_ipc_output_t;

typedef struct _wayfire_ipc_view {
  gint id, wsetid;
  gint deltax, deltay, wx, wy;
  GdkRectangle rect;
} wayfire_ipc_view_t;

#define WAYFIRE_VIEW(x) ((wayfire_ipc_view_t *)(x))
#define WAYFIRE_WSET(x) ((wayfire_ipc_wset_t *)(x))
#define WAYFIRE_OUTPUT(x) ((wayfire_ipc_output_t *)(x))
#define WAYFIRE_WORKSPACE_ID(wset,x,y) GINT_TO_POINTER((wset->id<<16)+((y)<<8)+x)

static gint main_ipc;
static GList *wset_list, *output_list, *view_list;
static gint focused_output;

static gint wayfire_ipc_send_req ( gint sock, gchar *method,
    struct json_object *data )
{
  struct json_object *json;
  const gchar *str;
  guint32 len;
  gint res = -1;
 
  json = json_object_new_object();
  json_object_object_add(json, "method", json_object_new_string(method));
  json_object_object_add(json, "data", data);

  str = json_object_to_json_string_ext(json, 0);
  len = GUINT32_TO_LE((guint32)strlen(str));

  if( write(sock, &len, sizeof(len))!=-1 )
    res = write(sock, str, len);

  json_object_put(json);
  return res;
}

static struct json_object *wayfire_ipc_recv_msg ( gint sock )
{
  gint32 len;

  if(recv(sock, &len, 4, 0) == 4)
    return recv_json(sock, GUINT32_FROM_LE(len));
  return NULL;
}

static void wayfire_ipc_wm_action ( void *wid, gchar *action, gboolean state )
{
  struct json_object *json;

  json = json_object_new_object();
  json_object_object_add(json, "view_id",
      json_object_new_int(GPOINTER_TO_INT(wid)));
  json_object_object_add(json, "state",
      json_object_new_boolean(state));

  wayfire_ipc_send_req(main_ipc, action, json);
}

static wayfire_ipc_view_t *wayfire_ipc_view_get ( gint id )
{
  GList *iter;

  for(iter=view_list; iter; iter=g_list_next(iter))
    if(((wayfire_ipc_view_t *)(iter->data))->id == id)
      return iter->data;
  return NULL;
}

static gboolean wayfire_ipc_wset_by_output ( const void *wset, const void *o )
{
  return WAYFIRE_WSET(wset)->output != GPOINTER_TO_INT(o);
}

static wayfire_ipc_wset_t *wayfire_ipc_wset_get ( gint id )
{
  GList *iter;

  for(iter=wset_list; iter; iter=g_list_next(iter))
    if(WAYFIRE_WSET(iter->data)->id == id)
      return iter->data;
  return NULL;
}

static wayfire_ipc_output_t *wayfire_ipc_output_get ( gint id )
{
  GList *iter;

  for(iter=output_list; iter; iter=g_list_next(iter))
    if(WAYFIRE_OUTPUT(iter->data)->id == id)
      return iter->data;
  return NULL;
}

static void wayfire_ipc_minimize ( gpointer wid )
{
  wayfire_ipc_wm_action(wid, "wm-actions/set-minimized", TRUE);
}

static void wayfire_ipc_unminimize ( gpointer wid )
{
  wayfire_ipc_wm_action(wid, "wm-actions/set-minimized", FALSE);
}

static void wayfire_ipc_maximize ( gpointer wid )
{
  wayfire_ipc_wm_action(wid, "wm-actions/set-fullscreen", TRUE);
}

static void wayfire_ipc_unmaximize ( gpointer wid )
{
  wayfire_ipc_wm_action(wid, "wm-actions/set-fullscreen", FALSE);
}

static void wayfire_ipc_focus ( gpointer wid )
{
  struct json_object *json;

  json = json_object_new_object();
  json_object_object_add(json, "id",
      json_object_new_int(GPOINTER_TO_INT(wid)));
  wayfire_ipc_send_req(main_ipc, "window-rules/focus-view", json);
}

static void wayfire_ipc_close ( gpointer wid )
{
  struct json_object *json;

  json = json_object_new_object();
  json_object_object_add(json, "id",
      json_object_new_int(GPOINTER_TO_INT(wid)));
  wayfire_ipc_send_req(main_ipc, "window-rules/close-view", json);
}

static void wayfire_ipc_move_to ( gpointer wid, gpointer wsid )
{
  wayfire_ipc_view_t *view;
  wayfire_ipc_wset_t *wset;
  wayfire_ipc_output_t *output;
  struct json_object *json, *geo;

  if( !(view = wayfire_ipc_view_get(GPOINTER_TO_INT(wid))) )
    return;
  if( !(wset = wayfire_ipc_wset_get(GPOINTER_TO_INT(wsid)>>16)) )
    return;
  if( !(output  = wayfire_ipc_output_get(GPOINTER_TO_INT(wset->output))) )
    return;

  json = json_object_new_object();
  json_object_object_add(json, "id",
      json_object_new_int(GPOINTER_TO_INT(wid)));
  json_object_object_add(json, "output_id",
      json_object_new_int(GPOINTER_TO_INT(wset->output)));
  json_object_object_add(json, "sticky",
      json_object_new_boolean(FALSE));
  geo = json_object_new_object();
  json_object_object_add(geo, "width",
      json_object_new_int(view->rect.width));
  json_object_object_add(geo, "height",
      json_object_new_int(view->rect.height));
  json_object_object_add(geo, "x", json_object_new_int(view->deltax +
        output->geo.width *((GPOINTER_TO_INT(wsid) & 0xff) - wset->x)));
  json_object_object_add(geo, "y", json_object_new_int(view->deltay +
        output->geo.height *(((GPOINTER_TO_INT(wsid)& 0xff00) >>8) -
          wset->y)));
  json_object_object_add(json, "geometry", geo);
  wayfire_ipc_send_req(main_ipc, "window-rules/configure-view", json);
}

static struct wintree_api wayfire_wintree_api = {
  .minimize = wayfire_ipc_minimize,
  .unminimize = wayfire_ipc_unminimize,
  .maximize = wayfire_ipc_maximize,
  .unmaximize = wayfire_ipc_unmaximize,
  .close = wayfire_ipc_close,
  .focus = wayfire_ipc_focus,
  .move_to = wayfire_ipc_move_to,
};

static void wayfire_ipc_set_workspace ( workspace_t *ws )
{
  wayfire_ipc_wset_t *wset;
  struct json_object *json;

  if( !(wset = wayfire_ipc_wset_get(GPOINTER_TO_INT(ws->id)>>16)) )
    return;

  json = json_object_new_object();
  json_object_object_add(json, "x",
      json_object_new_int(GPOINTER_TO_INT(ws->id)&0xff));
  json_object_object_add(json, "y",
      json_object_new_int((GPOINTER_TO_INT(ws->id)&0xff00)>>8));
  json_object_object_add(json, "output-id", json_object_new_int(wset->output));
  wayfire_ipc_send_req(main_ipc, "vswitch/set-workspace", json);
}

static gboolean wayfire_ipc_get_can_create ( void )
{
  return FALSE;
}

static gboolean wayfire_ipc_workspace_check_monitor ( void *wsid, gchar *name )
{
  wayfire_ipc_wset_t *wset;
  wayfire_ipc_output_t *output;

  if( !(wset = wayfire_ipc_wset_get(GPOINTER_TO_INT(wsid)>>16)) )
    return FALSE;
  if( !(output = wayfire_ipc_output_get(wset->output)) )
    return FALSE;

  return !g_strcmp0(output->name, name);
}

static guint wayfire_ipc_get_geom ( gpointer wid, GdkRectangle *place,
    gpointer wsid, GdkRectangle **wins, GdkRectangle *space, gint *focus )
{
  wayfire_ipc_wset_t *wset;
  wayfire_ipc_output_t *output;
  GList *iter;
  gint n, c, wx, wy;

  if( !(wset = wayfire_ipc_wset_get(GPOINTER_TO_INT(wsid)>>16)) )
    return 0;
  if( !(output = wayfire_ipc_output_get(wset->output)) )
    return 0;

  n=0;
  wx = ((GPOINTER_TO_INT(wsid) & 0xff) - wset->x) * output->geo.width;
  wy = (((GPOINTER_TO_INT(wsid) & 0xff00)>>8) - wset->y) * output->geo.height;
  for(iter=view_list; iter; iter=g_list_next(iter))
    if(WAYFIRE_VIEW(iter->data)->wsetid == wset->id &&
        WAYFIRE_VIEW(iter->data)->rect.x >= wx &&
        WAYFIRE_VIEW(iter->data)->rect.y >= wy &&
        WAYFIRE_VIEW(iter->data)->rect.x < wx + output->geo.width &&
        WAYFIRE_VIEW(iter->data)->rect.y < wy + output->geo.height &&
        (!wid || WAYFIRE_VIEW(iter->data)->id != GPOINTER_TO_INT(wid)))
      n++;

  space->x = 0;
  space->y = 0;
  space->width = output->work.width;
  space->height = output->work.height;

  *wins = g_malloc0(n * sizeof(GdkRectangle));
  c = 0;
  for(iter=view_list; iter; iter=g_list_next(iter))
    if(WAYFIRE_VIEW(iter->data)->wsetid == wset->id &&
        WAYFIRE_VIEW(iter->data)->rect.x >= wx &&
        WAYFIRE_VIEW(iter->data)->rect.y >= wy &&
        WAYFIRE_VIEW(iter->data)->rect.x < wx + output->geo.width &&
        WAYFIRE_VIEW(iter->data)->rect.y < wy + output->geo.height &&
        c < n)
    {
      if(!wid || WAYFIRE_VIEW(iter->data)->id != GPOINTER_TO_INT(wid))
      {
        (*wins)[c].x = WAYFIRE_VIEW(iter->data)->rect.x - wx;
        (*wins)[c].y = WAYFIRE_VIEW(iter->data)->rect.y - wy;
        (*wins)[c].width = WAYFIRE_VIEW(iter->data)->rect.width;
        (*wins)[c].height = WAYFIRE_VIEW(iter->data)->rect.height;
        c++;
      }
      else if(place)
      {
        place->x = WAYFIRE_VIEW(iter->data)->rect.x - wx;
        place->y = WAYFIRE_VIEW(iter->data)->rect.y - wy;
        place->width = WAYFIRE_VIEW(iter->data)->rect.width;
        place->height = WAYFIRE_VIEW(iter->data)->rect.height;
      }
    }

  return c;
}

static struct workspace_api wayfire_workspace_api = {
  .set_workspace = wayfire_ipc_set_workspace,
  .get_geom = wayfire_ipc_get_geom,
  .get_can_create = wayfire_ipc_get_can_create,
  .check_monitor = wayfire_ipc_workspace_check_monitor,
};

static void wayfire_ipc_window_workspace_track ( struct json_object *json )
{
  wayfire_ipc_output_t *output;
  wayfire_ipc_wset_t *wset;
  wayfire_ipc_view_t *view;
  struct json_object *geo;
  GList *iter;
  gint wid, wsetidx, cx, cy;

  if( !(wid = json_int_by_name(json, "id", 0)) )
    return;
  if( !(wsetidx = json_int_by_name(json, "wset-index", 0)) )
    return;
  if(!json_object_object_get_ex(json, "geometry", &geo))
    return;

  for(iter=wset_list; iter; iter=g_list_next(iter))
    if(WAYFIRE_WSET(iter->data)->index == wsetidx)
      break;
  if(!iter)
    return;
  wset = iter->data;
  if( !(output = wayfire_ipc_output_get(wset->output)) )
    return;

  if( !(view = wayfire_ipc_view_get(wid)) )
  {
    view = g_malloc0(sizeof(wayfire_ipc_view_t));
    view->id = GPOINTER_TO_INT(wid);
    view_list = g_list_prepend(view_list, view);
  }

  view->rect = json_rect_get(geo);
  view->deltax = view->rect.x % output->geo.width;
  view->deltay = view->rect.y % output->geo.height;
  cx = view->deltax<0?1:0;
  cy = view->deltay<0?1:0;
  view->deltax += output->geo.width * cx;
  view->deltay += output->geo.height * cy;
  view->wx = view->rect.x / output->geo.width - cx;
  view->wy = view->rect.y / output->geo.height - cy;
  view->wsetid = wset->id;

  wintree_set_workspace(GINT_TO_POINTER(wid), WAYFIRE_WORKSPACE_ID(wset,
        view->wx + wset->x, view->wy + wset->y));
}

static void wayfire_ipc_window_place ( gpointer wid )
{
  wayfire_ipc_view_t *view;
  GdkRectangle wloc;
  window_t *win;

  if( !(win = wintree_from_id(wid)) )
    return;
  if( !(view = wayfire_ipc_view_get(GPOINTER_TO_INT(wid))) )
    return;
  if(!wintree_placer_calc(wid, &wloc))
    return;
  view->deltax = wloc.x;
  view->deltay = wloc.y;
  if(win->workspace)
    wayfire_ipc_move_to(wid, win->workspace->id);
}

static void wayfire_ipc_window_new ( struct json_object *view )
{
  window_t *win;

  if(g_strcmp0(json_string_by_name(view, "type"), "toplevel"))
    return;

  win = wintree_window_init();
  win->uid = GINT_TO_POINTER(json_int_by_name(view, "id", G_MININT64));
  win->pid = json_int_by_name(view, "pid", G_MININT64);
  wintree_window_append(win);
  wintree_set_app_id(win->uid, json_string_by_name(view, "app-id"));
  wintree_set_title(win->uid, json_string_by_name(view, "title"));
  if(json_bool_by_name(view, "activated", FALSE))
    wintree_set_focus(win->uid);
  set_bit(win->state, WS_MINIMIZED,
      json_bool_by_name(view, "minimized", FALSE));
  set_bit(win->state, WS_MAXIMIZED | WS_FULLSCREEN,
      json_bool_by_name(view, "fullscreen", FALSE));
  wintree_log(win->uid);
  wayfire_ipc_window_workspace_track(view);
}

static void wayfire_ipc_window_delete ( gpointer wid )
{
  wintree_window_delete(wid);
  view_list = g_list_remove(view_list,
      wayfire_ipc_view_get(GPOINTER_TO_INT(wid)));
}

static void wayfire_ipc_workspace_set_visible ( gpointer id )
{
  wayfire_ipc_wset_t *wset;
  workspace_t *ws;
  gint x, y;

  if( !(wset = wayfire_ipc_wset_get(GPOINTER_TO_INT(id)>>16)) )
    return;

  for(y=0; y<wset->h; y++)
    for(x=0; x<wset->w; x++)
      if( (ws=workspace_from_id(WAYFIRE_WORKSPACE_ID(wset, x, y))) )
        set_bit(ws->state, WS_STATE_VISIBLE, id == WAYFIRE_WORKSPACE_ID(wset, x, y));
}

static wayfire_ipc_output_t *wayfire_ipc_output_new ( struct json_object *json,
   gint wsetid )
{
  wayfire_ipc_output_t *output;
  struct json_object *ptr;
  gint id;

  if(! (id = json_int_by_name(json, "id", 0)))
    return NULL;
  if( !(output = wayfire_ipc_output_get(id)) )
  {
    output = g_malloc0(sizeof(wayfire_ipc_output_t));
    output->name = g_strdup(json_string_by_name(json, "name"));
    output->id = id;
    output_list = g_list_prepend(output_list, output);
    g_debug("wayfire: new output: %s, id: %d", output->name, output->id);
  }
  if(json_object_object_get_ex(json, "geometry", &ptr))
    output->geo = json_rect_get(ptr);
  if(json_object_object_get_ex(json, "workarea", &ptr))
    output->work = json_rect_get(ptr);
  return output;
}

static wayfire_ipc_wset_t *wayfire_ipc_wset_new ( struct json_object *json )
{
  workspace_t *ws;
  wayfire_ipc_wset_t *wset;
  struct json_object *wspace;
  const gchar *name, *output_name;
  gchar *ws_name;
  gpointer id;
  gint x, y, wsetid;

  if(!json_object_object_get_ex(json, "workspace", &wspace))
    return NULL;

  if( !(name = json_string_by_name(json, "name")) )
    return NULL;
  wsetid = atoi(name);
  if( !(wset = wayfire_ipc_wset_get(wsetid)) )
  {
    wset = g_malloc(sizeof(wayfire_ipc_wset_t));
    wset->id = wsetid;
    wset->index = json_int_by_name(json, "index", 0);
    wset->output = json_int_by_name(json, "output-id", 0);
    wset->w = json_int_by_name(wspace, "grid_width", 0); 
    wset->h = json_int_by_name(wspace, "grid_height", 0);
    wset_list = g_list_prepend(wset_list, wset);
  }
  wset->x = json_int_by_name(wspace, "x", 0);
  wset->y = json_int_by_name(wspace, "y", 0);
  output_name = json_string_by_name(json, "output-name");

  for(y=0; y<wset->h; y++)
    for(x=0; x<wset->w; x++)
    {
      id = WAYFIRE_WORKSPACE_ID(wset, x, y);
      ws = workspace_new(id);
      ws_name = g_strdup_printf("<span alpha=\"1\" size=\"1\">%s:</span>%d",
          name, y*wset->w+x+1);
      workspace_set_name(ws, ws_name);
      g_free(ws_name);
      if(x==wset->x && y==wset->y)
      {
        if(wset->output == focused_output)
          workspace_set_focus(id);
        workspace_set_active(ws, output_name);
        wayfire_ipc_workspace_set_visible(id);
      }
    }
  g_debug("wayfire: new wset: %d, w: %d, h: %d, x: %d, y %d, output: %s",
      wsetid, wset->w, wset->h, wset->x, wset->y, output_name);
  return wset;
}

static void wayfire_ipc_workspace_changed ( struct json_object *json )
{
  wayfire_ipc_wset_t *wset;
  struct json_object *ptr;
  gint wsetid, output, x, y;

  if(!json_object_object_get_ex(json, "new-workspace", &ptr))
    return;
  x = json_int_by_name(ptr, "x", -1);
  y = json_int_by_name(ptr, "y", -1);
  wsetid = json_int_by_name(json, "wset", 0);
  output = json_int_by_name(json, "output", 0);
  g_debug("wayfire: active workspace: %d, %d wset: %d", x, y, wsetid);
  if(x<0 || y<0 || !output || !(wset = wayfire_ipc_wset_get(wsetid)))
    return;

  wset->x = x;
  wset->y = y;

  if(json_object_object_get_ex(json, "output-data", &ptr))
    wayfire_ipc_output_new(ptr, wsetid);

  if(wset->output == focused_output)
    workspace_set_focus(WAYFIRE_WORKSPACE_ID(wset, x, y));
  wayfire_ipc_workspace_set_visible(WAYFIRE_WORKSPACE_ID(wset, x, y));

  GList *iter;
  for(iter=view_list; iter; iter=g_list_next(iter))
    if(WAYFIRE_VIEW(iter->data)->wsetid == wset->id)
      wintree_set_workspace(GINT_TO_POINTER(WAYFIRE_VIEW(iter->data)->id), WAYFIRE_WORKSPACE_ID(wset,
          (WAYFIRE_VIEW(iter->data)->wx) + wset->x, WAYFIRE_VIEW(iter->data)->wy + wset->y));
}

static void wayfire_ipc_set_focused_output ( struct json_object *json )
{
  wayfire_ipc_wset_t *wset;
  GList *link;
  gint focus;

  if(!json || !(focus = json_int_by_name(json, "id", 0)) )
    return;
  focused_output = focus;

  if( !(link = g_list_find_custom(wset_list, GINT_TO_POINTER(focused_output),
          wayfire_ipc_wset_by_output)) )
    return;
  wset = link->data;
  workspace_set_focus(WAYFIRE_WORKSPACE_ID(wset, wset->x, wset->y));
}

static void wayfire_ipc_output_set_wset ( struct json_object *json )
{
  struct json_object *ptr;
  wayfire_ipc_output_t *output;
  wayfire_ipc_wset_t *wset;

  if( !(ptr = json_node_by_name(json, "new-wset-data")) ||
      !(wset = wayfire_ipc_wset_new(ptr)) )
    return;
  if( !(ptr = json_node_by_name(json, "output-data")) ||
      !(output = wayfire_ipc_output_new(ptr, wset->id)) )
    return;

  if(focused_output == output->id)
    wayfire_ipc_set_focused_output(json_node_by_name(json, "output-data"));
}

static gboolean wayfire_ipc_event ( GIOChannel *chan, GIOCondition cond,
    gpointer data )
{
  struct json_object *json, *view;
  window_t *win;
  const gchar *event;
  gpointer wid;
  gint sock;

  sock = g_io_channel_unix_get_fd(chan);
  if( !(json = wayfire_ipc_recv_msg(sock)) )
    return TRUE;

  if( (event = json_string_by_name(json, "event")) )
  {
    if( (view = json_object_object_get(json, "view")) &&
     !g_strcmp0(json_string_by_name(view, "type"), "toplevel") )
    {
      wid = GINT_TO_POINTER(json_int_by_name(view, "id", G_MININT64));

      if(!g_strcmp0(event, "view-mapped"))
      {
        wayfire_ipc_window_new(view);
        wayfire_ipc_window_place(wid);
      }
      else if(!g_strcmp0(event, "view-unmapped"))
        wayfire_ipc_window_delete(wid);
      else if(!g_strcmp0(event, "view-focused"))
        wintree_set_focus(wid);
      else if(!g_strcmp0(event, "view-geometry-changed"))
        wayfire_ipc_window_workspace_track(view);
      else if(!g_strcmp0(event, "view-minimized"))
      {
        if( (win = wintree_from_id(wid)) )
          set_bit(win->state, WS_MINIMIZED,
              json_bool_by_name(view, "minimized", FALSE));
      }
      else if(!g_strcmp0(event, "view-fullscreened"))
      {
        if( (win = wintree_from_id(wid)) )
          set_bit(win->state, WS_MAXIMIZED | WS_FULLSCREEN,
              json_bool_by_name(view, "fullscreen", FALSE));
      }
      else if(!g_strcmp0(event, "view-app-id-changed"))
        wintree_set_app_id(wid, json_string_by_name(view, "app-id"));
      else if(!g_strcmp0(event, "view-title-changed"))
        wintree_set_title(wid, json_string_by_name(view, "title"));
    }
    else if(!g_strcmp0(event, "wset-workspace-changed"))
      wayfire_ipc_workspace_changed(json);
    else if(!g_strcmp0(event, "output-wset-changed"))
      wayfire_ipc_output_set_wset(json);
    else if(!g_strcmp0(event, "output-gain-focus"))
      wayfire_ipc_set_focused_output(json_node_by_name(json, "output"));

  }
  json_object_put(json);

  return TRUE;
}

static void wayfire_ipc_monitor_removed ( GdkDisplay *disp, GdkMonitor *mon )
{
  wayfire_ipc_output_t *output;
  const gchar *output_name;
  GList *iter, *link;
 
  if( !(output_name = monitor_get_name(mon)) )
    return;

  for(iter=output_list; iter; iter=g_list_next(iter))
    if(!g_strcmp0(WAYFIRE_OUTPUT(iter->data)->name, output_name))
      break;
  if(!iter)
    return;

  output = iter->data;
  while( (link = g_list_find_custom(wset_list, GINT_TO_POINTER(output->id),
          wayfire_ipc_wset_by_output)) )
  {
    g_free(link->data);
    wset_list = g_list_delete_link(wset_list, link);
  }
      
  output_list = g_list_delete_link(output_list, iter);
  g_free(output->name);
  g_free(output);
}

void wayfire_ipc_init ( void )
{
  struct json_object *json, *events;
  GIOChannel *chan;
  GdkDisplay *disp;
  const gchar *sock_file;
  gint i;

  if( !(sock_file = sockname?sockname:g_getenv("WAYFIRE_SOCKET")) )
    return;
  if(wintree_api_check())
    return;

  g_debug("wayfire-ipc: socket: %s", sock_file);

  if( (main_ipc = socket_connect(sock_file, 3000))<=0 )
    return;
  wintree_api_register(&wayfire_wintree_api);
  workspace_api_register(&wayfire_workspace_api);

  disp = gdk_display_get_default();
  g_signal_connect(G_OBJECT(disp), "monitor-removed",
      G_CALLBACK(wayfire_ipc_monitor_removed), NULL);

  wayfire_ipc_send_req(main_ipc, "window-rules/list-outputs", NULL);
  json = wayfire_ipc_recv_msg(main_ipc);
  if(json && json_object_is_type(json, json_type_array))
    for(i=0; i<json_object_array_length(json); i++)
      wayfire_ipc_output_new(json_object_array_get_idx(json, i), 0);
  json_object_put(json);

  wayfire_ipc_send_req(main_ipc, "window-rules/list-wsets", NULL);
  json = wayfire_ipc_recv_msg(main_ipc);
  if(json && json_object_is_type(json, json_type_array))
    for(i=0; i<json_object_array_length(json); i++)
      wayfire_ipc_wset_new(json_object_array_get_idx(json, i));
  json_object_put(json);

  wayfire_ipc_send_req(main_ipc, "window-rules/get-focused-output", NULL);
  json = wayfire_ipc_recv_msg(main_ipc);
  wayfire_ipc_set_focused_output(json_node_by_name(json, "info"));
  json_object_put(json);

  wayfire_ipc_send_req(main_ipc, "window-rules/list-views", NULL);
  json = wayfire_ipc_recv_msg(main_ipc);

  if(json_object_is_type(json, json_type_array))
    for(i=0; i<json_object_array_length(json); i++)
      wayfire_ipc_window_new(json_object_array_get_idx(json, i));
  json_object_put(json);

  events = json_object_new_array();
  json_object_array_add(events, json_object_new_string("view-focused"));
  json_object_array_add(events, json_object_new_string("view-mapped"));
  json_object_array_add(events, json_object_new_string("view-unmapped"));
  json_object_array_add(events, json_object_new_string("view-minimized"));
  json_object_array_add(events, json_object_new_string("view-fullscreened"));
  json_object_array_add(events, json_object_new_string("view-title-changed"));
  json_object_array_add(events, json_object_new_string("view-app-id-changed"));
  json_object_array_add(events, json_object_new_string("view-workspace-changed"));
  json_object_array_add(events, json_object_new_string("view-geometry-changed"));
  json_object_array_add(events, json_object_new_string("wset-workspace-changed"));
  json_object_array_add(events, json_object_new_string("output-gain-focus"));

  json = json_object_new_object();
  json_object_object_add(json, "events", events);
  wayfire_ipc_send_req(main_ipc, "window-rules/events/watch", json);
  json_object_put(wayfire_ipc_recv_msg(main_ipc));

  chan = g_io_channel_unix_new(main_ipc);
  g_io_add_watch(chan, G_IO_IN, wayfire_ipc_event, NULL);
}
