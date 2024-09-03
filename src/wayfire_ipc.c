/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include "sfwbar.h"
#include "wintree.h"
#include <sys/socket.h>
#include <json.h>

static gint main_ipc;

static void wayfire_ipc_send_req ( gint sock, gchar *method,
    struct json_object *data )
{
  struct json_object *json;
  const gchar *str;
  guint32 len;
 
  json = json_object_new_object();
  json_object_object_add(json, "method", json_object_new_string(method));
  json_object_object_add(json, "data", data);

  str = json_object_to_json_string_ext(json, 0);
  len = GUINT32_TO_LE((guint32)strlen(str));

//  g_message("Sending: %s", str);

  if( write(sock, &len, sizeof(len))!=-1 )
    write(sock, str, len);

  json_object_put(json);
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
  struct json_object *json;

  json = json_object_new_object();
  json_object_object_add(json, "view-id",
      json_object_new_int(GPOINTER_TO_INT(wid)));
  json_object_object_add(json, "wset-index",
      json_object_new_int(GPOINTER_TO_INT(wsid)));
  wayfire_ipc_send_req(main_ipc, "wsets/send-view-to-wset", json);
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

static void wayfire_ipc_new_window ( struct json_object *view )
{
  window_t *win;

  if(g_strcmp0(json_string_by_name(view, "type"), "toplevel"))
    return;

//  g_message("%s", json_object_to_json_string_ext(view,0));
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

//  g_message("event: %s", json_object_to_json_string_ext(json, 0));

  if( (event = json_string_by_name(json, "event")) )
  {
    if( (view = json_object_object_get(json, "view")) &&
     !g_strcmp0(json_string_by_name(view, "type"), "toplevel") )
    {
      wid = GINT_TO_POINTER(json_int_by_name(view, "id", G_MININT64));

      if(!g_strcmp0(event, "view-mapped"))
        wayfire_ipc_new_window(view);
      else if(!g_strcmp0(event, "view-unmapped"))
        wintree_window_delete(wid);
      else if(!g_strcmp0(event, "view-focused"))
        wintree_set_focus(wid);
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
    {
      g_message("new ws: %s", json_object_to_json_string_ext(json,0));
    }
    else if(!g_strcmp0(event, "output-wset-changed"))
      g_message("new ws: %s", json_object_to_json_string_ext(json,0));
  }
  json_object_put(json);

  return TRUE;
}

void wayfire_ipc_init ( void )
{
  struct json_object *json, *events;
  GIOChannel *chan;
  const gchar *sock_file;
  gint i;

  if( !(sock_file = g_getenv("WAYFIRE_SOCKET")) )
    return;

  g_debug("wayfire-ipc: socket: %s", sock_file);

  if( (main_ipc = socket_connect(sock_file, 3000))<=0 )
    return;
  wintree_api_register(&wayfire_wintree_api);

  events = json_object_new_array();
  json_object_array_add(events, json_object_new_string("view-focused"));
  json_object_array_add(events, json_object_new_string("view-mapped"));
  json_object_array_add(events, json_object_new_string("view-unmapped"));
  json_object_array_add(events, json_object_new_string("view-minimized"));
  json_object_array_add(events, json_object_new_string("view-fullscreened"));
  json_object_array_add(events, json_object_new_string("view-title-changed"));
  json_object_array_add(events, json_object_new_string("view-app-id-changed"));
  json_object_array_add(events, json_object_new_string("view-workspace-changed"));
  json_object_array_add(events, json_object_new_string("wset-workspace-changed"));
  json_object_array_add(events, json_object_new_string("output-gain-focus"));

  json = json_object_new_object();
  json_object_object_add(json, "events", events);
  wayfire_ipc_send_req(main_ipc, "window-rules/events/watch", json);
  json_object_put(wayfire_ipc_recv_msg(main_ipc));

  wayfire_ipc_send_req(main_ipc, "window-rules/list-views", NULL);
  json = wayfire_ipc_recv_msg(main_ipc);

  if(json_object_is_type(json, json_type_array))
    for(i=0; i<json_object_array_length(json); i++)
      wayfire_ipc_new_window(json_object_array_get_idx(json, i));
  json_object_put(json);

  wayfire_ipc_send_req(main_ipc, "window-rules/list-wsets", NULL);
  json = wayfire_ipc_recv_msg(main_ipc);
  g_message("%s", json_object_to_json_string_ext(json,0));

  chan = g_io_channel_unix_new(main_ipc);
  g_io_add_watch(chan, G_IO_IN, wayfire_ipc_event, NULL);
}
