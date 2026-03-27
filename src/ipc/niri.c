/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "input.h"
#include "wintree.h"
#include "workspace.h"
#include "util/json.h"
#include "util/string.h"

static gchar **niri_layouts;
static GIOChannel *niri_command_ipc;
static GList *niri_ipc_workspaces;

static void niri_ipc_action ( char *cmd, ... )
{
  va_list args;
  gchar *buf;

  g_return_if_fail(cmd);

  va_start(args, cmd);
  buf = g_strdup_vprintf(cmd, args);
  g_debug("niri request: %s", buf);
  g_message("niri request: %s", buf);
  g_io_channel_write_chars(niri_command_ipc, "{\"Action\":{", 11, NULL, NULL);
  g_io_channel_write_chars(niri_command_ipc, buf, -1, NULL, NULL);
  g_io_channel_write_chars(niri_command_ipc, "}}\n", 3, NULL, NULL);
  g_io_channel_flush(niri_command_ipc, NULL);
  g_free(buf);
  va_end(args);
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

static struct wintree_api niri_wintree_api = {
  .custom_ipc = "niri",
//  .minimize = niri_ipc_minimize,
//  .unminimize = niri_ipc_unminimize,
  .maximize = niri_ipc_maximize,
  .unmaximize = niri_ipc_unmaximize,
  .close = niri_ipc_close,
  .focus = niri_ipc_focus,
/*  .move_to = sway_ipc_move_to,*/
};

/* workspace API */
static guint niri_ipc_get_geom ( gpointer wid, GdkRectangle *place,
    gpointer wsid, GdkRectangle **wins, GdkRectangle *space, gint *focus )
{
  return 0;
}

static void niri_ipc_set_workspace ( workspace_t *ws )
{
  niri_ipc_action("\"FocusWorkspace\":{\"reference\":{\"Index\":%d}}",
      GPOINTER_TO_INT(ws->id) );
}

static gboolean niri_ipc_get_can_create ( void )
{
  return FALSE;
}

static struct workspace_api niri_workspace_api = {
  .set_workspace = niri_ipc_set_workspace,
//  .get_geom = sway_ipc_get_geom
  .get_can_create = niri_ipc_get_can_create,
  .free_data = g_free,
};

static void niri_ipc_window_handle ( struct json_object *json, gpointer d)
{
  window_t *win;
  gpointer wid;

  if( !(wid = json_int_ptr_by_name(json, "id", 0)) )
    return;

  if( !(win = wintree_from_id(wid)) )
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
  workspace_set_active(ws, ws->data);
  workspace_commit(ws);
}

static gboolean niri_ipc_event ( GIOChannel *chan, GIOCondition cond,
    gpointer d )
{
  json_object *obj, *data, *sub;

  if( !(obj = json_recv_channel(chan)) )
    return FALSE;

  if(json_object_object_get_ex(obj, "WorkspacesChanged", &data))
    niri_ipc_workspaces_changed_handle(data);
  else if(json_object_object_get_ex(obj, "WindowsChanged", &data))
    json_foreach(json_array_by_name(data, "windows"),
        niri_ipc_window_handle, NULL);
  else if(json_object_object_get_ex(obj, "KeyboardLayoutsChanged", &data) &&
      json_object_object_get_ex(data, "keyboard_layouts", &sub) &&
      json_array_to_strv(json_array_by_name(sub, "names"), &niri_layouts))
    input_layout_list_set(niri_layouts);
  else if(json_object_object_get_ex(obj, "WindowFocusChanged", &data))
    wintree_set_focus(json_int_ptr_by_name(data, "id", 0));
  else if(json_object_object_get_ex(obj, "WindowClosed", &data))
    wintree_window_delete(json_int_ptr_by_name(data, "id", 0));
  else if(json_object_object_get_ex(obj, "WindowOpenedOrChanged", &data))
    niri_ipc_window_handle(json_object_object_get(data, "window"), NULL);
  else if(json_object_object_get_ex(obj, "WorkspaceActivated", &data))
    niri_ipc_workspace_activated(data);

  json_object_put(obj);

//  g_message("%s", json_object_to_json_string(obj));

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
  else if(chan)
    g_io_channel_unref(chan);
  wintree_api_register(&niri_wintree_api);
  workspace_api_register(&niri_workspace_api);
  g_free(r);
}
