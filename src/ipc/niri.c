/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "exec.h"
#include "input.h"
#include "wintree.h"
#include "workspace.h"
#include "gui/monitor.h"
#include "util/json.h"
#include "util/string.h"

typedef struct _niri_geom {
  gpointer id;
  GdkRectangle rect;
} niri_geom_t;

static gchar **niri_layouts;
static GIOChannel *niri_command_ipc;
static GList *niri_ipc_workspaces, *niri_ipc_geom;

static void niri_ipc_action ( char *cmd, ... )
{
  va_list args;
  gchar *buf;

  g_return_if_fail(cmd);

  va_start(args, cmd);
  buf = g_strdup_vprintf(cmd, args);
  g_debug("niri request: %s", buf);
  g_io_channel_write_chars(niri_command_ipc, "{\"Action\":{", 11, NULL, NULL);
  g_io_channel_write_chars(niri_command_ipc, buf, -1, NULL, NULL);
  g_io_channel_write_chars(niri_command_ipc, "}}\n", 3, NULL, NULL);
  g_io_channel_flush(niri_command_ipc, NULL);
  g_free(buf);
  va_end(args);
}

static gboolean niri_ipc_geom_cmp ( const void *node, const void *id )
{
  return !(((niri_geom_t *)node)->id == id);
}

static GdkRectangle niri_ipc_geom_parse ( struct json_object *json )
{
  struct json_object *size, *pos;
  GdkRectangle rect = {.x = -1, .y = -1, .width = -1, .height = -1};

  if(!json || !(size = json_array_by_name(json, "window_size")) ||
      !(pos = json_array_by_name(json, "tile_pos_in_workspace_view")) ||
      json_object_array_length(pos)<2 ||
      json_object_array_length(size)<2)
    return rect;

  rect.x = json_object_get_int(json_object_array_get_idx(pos, 0));
  rect.y = json_object_get_int(json_object_array_get_idx(pos, 1));
  rect.width = json_object_get_int(json_object_array_get_idx(size, 0));
  rect.height = json_object_get_int(json_object_array_get_idx(size, 1));

  return rect;
}

static void niri_ipc_geom_update ( gpointer wid, struct json_object *layout )
{
  niri_geom_t *geom;
  GList *lptr;

  if(!wid || !layout)
    return;

  if( (lptr = g_list_find_custom(niri_ipc_geom, wid, niri_ipc_geom_cmp)) )
    geom = lptr->data;
  else
  {
    geom = g_malloc0(sizeof(niri_geom_t));
    geom->id = wid;
    niri_ipc_geom = g_list_prepend(niri_ipc_geom, geom);
  }
  geom->rect = niri_ipc_geom_parse(layout);
}

static void niri_ipc_maximize ( gpointer wid )
{
  window_t *win;

  if((win = wintree_from_id(wid)) && !(win->state & WS_MAXIMIZED))
    niri_ipc_action("\"FullscreenWindow\":{\"id\":%d}", GPOINTER_TO_INT(wid));
}

static void niri_ipc_unmaximize ( gpointer wid )
{
  window_t *win;

  if((win = wintree_from_id(wid)) && (win->state & WS_MAXIMIZED))
    niri_ipc_action("\"FullscreenWindow\":{\"id\":%d}", GPOINTER_TO_INT(wid));
}

static void niri_ipc_close ( gpointer wid )
{
  niri_ipc_action("\"CloseWindow\":{\"id\":%d}", GPOINTER_TO_INT(wid));
}

static void niri_ipc_focus ( gpointer wid )
{
  niri_ipc_action("\"FocusWindow\":{\"id\":%d}", GPOINTER_TO_INT(wid));
}

static void niri_ipc_move_to ( gpointer wid, gpointer wsid )
{
  niri_ipc_action("\"MoveWindowToWorkspace\":{\"window_id\":%d,\
      \"reference\":{\"Id\":%d},\"focus\":false}",
      GPOINTER_TO_INT(wid), GPOINTER_TO_INT(wsid));
}

static struct wintree_api niri_wintree_api = {
  .custom_ipc = "niri",
//  .minimize = niri_ipc_minimize,
//  .unminimize = niri_ipc_unminimize,
  .maximize = niri_ipc_maximize,
  .unmaximize = niri_ipc_unmaximize,
  .close = niri_ipc_close,
  .focus = niri_ipc_focus,
  .move_to = niri_ipc_move_to,
};

/* workspace API */
static guint niri_ipc_get_geom ( gpointer wid, GdkRectangle *place,
    gpointer wsid, GdkRectangle **wins, GdkRectangle *space, gint *focus )
{
  window_t *win;
  workspace_t *ws;
  niri_geom_t *geom;
  GList *l;
  GdkMonitor *mon;
  gint i = 0;

  if( !(ws = workspace_from_id(wsid)) || !(mon = monitor_from_name(ws->data)) )
    return 0;

  gdk_monitor_get_geometry(mon, space);
  *wins = g_malloc0(sizeof(GdkRectangle) * g_list_length(niri_ipc_geom));

  for(l = niri_ipc_geom; l; l=g_list_next(l))
  {
    geom = l->data;
    if( !(win = wintree_from_id(geom->id)) ||
        win->workspace != workspace_from_id(wsid) )
      continue;
    if(win->uid == wintree_get_focus())
      *focus = i;
    if(!wid || win->uid != wid)
      (*wins)[i++] = geom->rect;
    else if(place)
      *place = geom->rect;
  }

  return i;
}

static void niri_ipc_set_workspace ( workspace_t *ws )
{
  niri_ipc_action("\"FocusWorkspace\":{\"reference\":{\"Id\":%d}}", ws->id);
}

static gboolean niri_ipc_get_can_create ( void )
{
  return FALSE;
}

static struct workspace_api niri_workspace_api = {
  .set_workspace = niri_ipc_set_workspace,
  .get_geom = niri_ipc_get_geom,
  .get_can_create = niri_ipc_get_can_create,
  .free_data = g_free,
};

static void niri_ipc_layout_prev ( void )
{
  niri_ipc_action("\"SwitchLayout\":{\"layout\":\"Prev\"}");
}

static void niri_ipc_layout_next ( void )
{
  niri_ipc_action("\"SwitchLayout\":{\"layout\":\"Next\"}");
}

static void niri_ipc_layout_set ( gchar *layout )
{
  niri_ipc_action("\"SwitchLayout\":{\"layout\":{\"Index\":%d}}",
      strv_index(niri_layouts, layout));
}

static struct input_api niri_input_api = {
  .layout_prev = niri_ipc_layout_prev,
  .layout_next = niri_ipc_layout_next,
  .layout_set = niri_ipc_layout_set,
};

static void niri_ipc_exec ( const gchar *cmd )
{
  gchar **argv, *params, *ptr;
  gint argc, i;
  gsize len = 0;

  if(!g_shell_parse_argv(cmd, &argc, &argv, NULL))
    return;

  for(i=0; i<argc; i++)
    len+=strlen(argv[i])+3;

  ptr = params = g_malloc0(len);

  for(i=0; i<argc; i++)
  {
    if(i)
      *(ptr++) = ',';
    *(ptr++) = '"';
    ptr = g_stpcpy(ptr, argv[i]);
    *(ptr++) = '"';
  }

  niri_ipc_action("\"Spawn\":{\"command\":[%s]}", params);

  g_free(params);
  g_strfreev(argv);
}

static void niri_ipc_window_handle ( struct json_object *json, gpointer d)
{
  GdkRectangle place;
  window_t *win;
  gpointer wid;
  gboolean new;

  if( !(wid = json_int_ptr_by_name(json, "id", 0)) )
    return;

  if( (new = !(win = wintree_from_id(wid))) )
  {
    win = wintree_window_init();
    win->uid = wid;
  }

  win->pid = json_int_by_name(json, "pid", -1);
  wintree_window_append(win);
  wintree_set_title(wid, json_string_by_name(json, "title"));
  wintree_set_app_id(wid, json_string_by_name(json, "app_id"));
  wintree_set_float(wid, json_bool_by_name(json, "is_floating", FALSE));
  set_bit(win->state, WS_URGENT, json_bool_by_name(json, "is_urgent", FALSE));
  wintree_set_workspace(wid, json_int_ptr_by_name(json, "workspace_id", 0));
  if(json_bool_by_name(json, "is_focused", FALSE))
    wintree_set_focus(wid);

  niri_ipc_geom_update(wid, json_object_object_get(json, "layout"));

  if(d && win->floating &&
      new && wintree_placer_calc(GINT_TO_POINTER(wid), &place))
    niri_ipc_action("\"MoveFloatingWindow\":{\"id\":%d,\
        \"x\":{\"SetFixed\":%d},\"y\":{\"SetFixed\":%d}}",
        wid, place.x, place.y);
}

static void niri_ipc_window_closed_handle ( struct json_object *json )
{
  GList *lptr;
  gpointer wid;

  if( !(wid = json_int_ptr_by_name(json, "id", 0)) )
    return;

  wintree_window_delete(wid);
  if( (lptr = g_list_find_custom(niri_ipc_geom, wid, niri_ipc_geom_cmp)) )
  {
    g_free(lptr->data);
    niri_ipc_geom = g_list_delete_link(niri_ipc_geom, lptr);
  }
}

static void niri_ipc_window_layout_handle ( struct json_object *json, void *d )
{
  if(!json || json_object_array_length(json)<2 )
    return;

  niri_ipc_geom_update(
      GINT_TO_POINTER(json_object_get_int(json_object_array_get_idx(json, 0))),
      json_object_array_get_idx(json, 1));
}

static void niri_ipc_workspace_handle ( struct json_object *json, GList **l)
{
  workspace_t *ws;
  gpointer id;
  gchar *name;

  if( !(id = json_int_ptr_by_name(json, "id", 0)) )
    return;
  *l = g_list_remove(*l, id);
  if(!g_list_find(niri_ipc_workspaces, id))
    niri_ipc_workspaces = g_list_append(niri_ipc_workspaces, id);
  if( !(name = g_strdup(json_string_by_name(json, "name"))) )
    name = g_strdup_printf("%d", GPOINTER_TO_INT(id));
  if( !(ws = workspace_from_id(id)) && !(ws = workspace_from_name(name)) )
  {
    ws = workspace_new(id);
    workspace_set_name(ws, name);
  }
  else
  {
    workspace_ref(ws);
    ws->id = id;
  }
  g_free(name);

  str_assign((gchar **)&ws->data,
      g_strdup(json_string_by_name(json, "output")));
  if(json_bool_by_name(json, "is_focused", FALSE))
  {
    workspace_change_focus(id);
    workspace_set_active(ws, json_string_by_name(json, "output"));
  }
  workspace_mod_state(id, WS_STATE_URGENT,
      json_bool_by_name(json, "is_urgent", FALSE));
  workspace_mod_state(id, WS_STATE_VISIBLE,
      json_bool_by_name(json, "is_active", FALSE));

  workspace_commit(ws);
}

static void niri_ipc_workspace_delete ( gpointer id, gpointer d )
{
  workspace_unref(id);
  niri_ipc_workspaces = g_list_remove(niri_ipc_workspaces, id);
}

static void niri_ipc_workspaces_changed_handle ( struct json_object *json )
{
  GList *r = g_list_copy(niri_ipc_workspaces);

  json_foreach(json_array_by_name(json, "workspaces"),
      (void (*)(struct json_object *, void *))niri_ipc_workspace_handle, &r);
  g_list_foreach(r, niri_ipc_workspace_delete, NULL);
  g_list_free(r);
}

static void niri_ipc_workspace_activated ( struct json_object *json )
{
  workspace_t *ws;
  if( !(ws = workspace_from_id(json_int_ptr_by_name(json, "id", 0))) )
    return;
  workspace_change_focus(ws->id);
  workspace_set_active(ws, (gchar *)ws->data);
  workspace_commit(ws);
}

static void niri_ipc_layout_switched_handle ( struct json_object *json )
{
  if(niri_layouts)
    input_layout_set(niri_layouts[CLAMP(json_int_by_name(json, "idx", 0),
          0, MAX(1, g_strv_length(niri_layouts))-1)]);
}

static void niri_ipc_layouts_changed_handle ( struct json_object *json )
{
  if(json_array_to_strv(json_array_by_name(json_object_object_get(json,
            "keyboard_layouts"), "names"), &niri_layouts))
    input_layout_list_set(niri_layouts);
}

static gboolean niri_ipc_event ( GIOChannel *chan, GIOCondition cond,
    gpointer d )
{
  json_object *obj, *data;

  if( !(obj = json_recv_channel(chan)) )
    return FALSE;

  if(json_object_object_get_ex(obj, "WorkspacesChanged", &data))
    niri_ipc_workspaces_changed_handle(data);
  else if(json_object_object_get_ex(obj, "WorkspaceActivated", &data))
    niri_ipc_workspace_activated(data);
  else if(json_object_object_get_ex(obj, "WindowsChanged", &data))
    json_foreach(json_array_by_name(data, "windows"),
        niri_ipc_window_handle, NULL);
  else if(json_object_object_get_ex(obj, "WindowFocusChanged", &data))
    wintree_set_focus(json_int_ptr_by_name(data, "id", 0));
  else if(json_object_object_get_ex(obj, "WindowLayoutsChanged", &data))
    json_foreach(json_array_by_name(data, "changes"),
        niri_ipc_window_layout_handle, NULL);
  else if(json_object_object_get_ex(obj, "WindowClosed", &data))
    niri_ipc_window_closed_handle(data);
  else if(json_object_object_get_ex(obj, "WindowOpenedOrChanged", &data))
    niri_ipc_window_handle(json_object_object_get(data, "window"), (void *)1);
  else if(json_object_object_get_ex(obj, "KeyboardLayoutSwitched", &data))
    niri_ipc_layout_switched_handle(data);
  else if(json_object_object_get_ex(obj, "KeyboardLayoutsChanged", &data))
    niri_ipc_layouts_changed_handle(data);

//  g_message("%s", json_object_to_json_string(obj));
  json_object_put(obj);

  return TRUE;
}

void niri_ipc_init ( void )
{
  GIOChannel *chan;
  const gchar *sockaddr;
  gint sock;
  gchar *r = NULL;

  if(wintree_api_check() || !(sockaddr = g_getenv("NIRI_SOCKET")) )
    return;
  if( (sock = socket_connect(sockaddr, 1000))==-1)
    return;

  chan = g_io_channel_unix_new(sock);
  g_io_channel_set_close_on_unref(chan, TRUE);
  if( g_io_channel_write_chars(chan, "\"EventStream\"\n", -1, NULL, NULL) ==
      G_IO_STATUS_NORMAL &&
      g_io_channel_flush(chan, NULL) == G_IO_STATUS_NORMAL &&
      g_io_channel_read_line(chan, &r, NULL, NULL, NULL)==G_IO_STATUS_NORMAL &&
      !g_strcmp0(r, "{\"Ok\":\"Handled\"}\n") )
  {
    g_io_add_watch(chan, G_IO_IN, niri_ipc_event, NULL);
    if( (sock = socket_connect(sockaddr, 1000))!=-1)
      niri_command_ipc = g_io_channel_unix_new(sock);
  }
  else
    g_io_channel_unref(chan);
  wintree_api_register(&niri_wintree_api);
  workspace_api_register(&niri_workspace_api);
  input_api_register(&niri_input_api);
  exec_api_set(niri_ipc_exec);
  g_free(r);
}
