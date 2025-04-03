/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "wintree.h"
#include "util/json.h"

#define hypr_ipc_parse_id(x, y) GSIZE_TO_POINTER(g_ascii_strtoull(x, y, 16))
#define hypr_ipc_parse_ws(x, y) GSIZE_TO_POINTER(g_ascii_strtoll(x, y, 10))

static gchar *ipc_sockaddr;

static gpointer hypr_ipc_window_id ( json_object *json )
{
  const gchar *str;

  str = json_string_by_name(json, "address");
  return str? hypr_ipc_parse_id(str, NULL) : NULL;
}

static gpointer hypr_ipc_workspace_id ( json_object *json )
{
  json_object *ptr;

  if(json_object_object_get_ex(json,"workspace", &ptr) && ptr)
    return GINT_TO_POINTER(json_int_by_name(ptr, "id", 0));
  return NULL;
}

static gboolean hypr_ipc_window_geom ( json_object *json, GdkRectangle *res )
{
  json_object *ptr;

  if(!json_object_object_get_ex(json, "at", &ptr) || !ptr)
    return FALSE;
  res->x = json_object_get_int(json_object_array_get_idx(ptr, 0));
  res->y = json_object_get_int(json_object_array_get_idx(ptr, 1));
  if(!json_object_object_get_ex(json, "size", &ptr) || !ptr)
    return FALSE;
  res->width = json_object_get_int(json_object_array_get_idx(ptr, 0));
  res->height = json_object_get_int(json_object_array_get_idx(ptr, 1));

  return TRUE;
}

static gboolean hypr_ipc_request ( gchar *addr, gchar *command, json_object **json )
{
  gint sock;

  if(!command)
    return FALSE;

  if( (sock = socket_connect(addr, 1000))==-1 )
  {
    g_debug("hypr: can't open socket");
    return FALSE;
  }

  if(write(sock, command, strlen(command))==-1)
  {
    g_debug("hypr: can't write to socket");
    close(sock);
    return FALSE;
  }

  if(json)
    *json = recv_json(sock, -1);

  close(sock);
  return TRUE;
}

static void hypr_ipc_command ( gchar *cmd, ... )
{
  va_list args;
  gchar *buf;
  
  if(!cmd)
    return;

  va_start(args, cmd);
  buf = g_strdup_vprintf(cmd, args);
  g_debug("hypr command: %s", buf);
  if(!hypr_ipc_request (ipc_sockaddr, buf, NULL))
    g_debug("hypr: unable to send command");
  g_free(buf);
  va_end(args);
}

static gchar *hypr_ipc_workspace_data ( workspace_t *ws, gchar *field )
{
  json_object *json, *ptr;
  gint i;
  gchar *res = NULL;

  if(!ws || !hypr_ipc_request(ipc_sockaddr, "j/workspaces", &json) || !json)
    return NULL;

  if(json_object_is_type(json, json_type_array))
    for(i=0; i<json_object_array_length(json); i++)
    {
      ptr = json_object_array_get_idx(json, i);
      if(ws->id==GINT_TO_POINTER(json_int_by_name(ptr, "id", 0)))
        res = g_strdup(json_string_by_name(ptr, field));
    }
  json_object_put(json);
  return res;
}

static void hypr_ipc_handle_window ( json_object *obj )
{
  window_t *win;
  gpointer id;
  gchar *monitor;

  id = hypr_ipc_window_id(obj);
  if(!id)
    return;

  win = wintree_from_id(id);
  if(!win)
  {
    win = wintree_window_init();
    win->uid = id;
    win->pid = json_int_by_name(obj, "pid", 0);
    win->floating = json_bool_by_name(obj, "floating", FALSE);
    wintree_window_append(win);
    wintree_set_app_id(id, json_string_by_name(obj, "class"));
    wintree_set_title(id, json_string_by_name(obj, "title"));
    wintree_log(id);
  }
  else
    wintree_set_title(id,json_string_by_name(obj, "title"));

  if(hypr_ipc_workspace_id(obj)==GINT_TO_POINTER(-99))
    win->state |= WS_MINIMIZED;
  else
  {
    win->state &= ~WS_MINIMIZED;
    wintree_set_workspace(win->uid, hypr_ipc_workspace_id(obj));
    monitor = hypr_ipc_workspace_data(win->workspace, "monitor");
    if(!g_list_find_custom(win->outputs, monitor, (GCompareFunc)g_strcmp0))
    {
      g_list_free_full(win->outputs, g_free);
      win->outputs = g_list_prepend(NULL, monitor);
    }
    else
      g_free(monitor);
  }
}

static gboolean hypr_ipc_get_clients ( gpointer *uid )
{
  json_object *json, *ptr;
  gpointer id;
  gint i;

  if(!hypr_ipc_request(ipc_sockaddr, "j/clients", &json) || !json)
    return FALSE;
  if( json_object_is_type(json, json_type_array) )
    for(i=0; i<json_object_array_length(json); i++)
    {
      ptr = json_object_array_get_idx(json, i);
      id = hypr_ipc_window_id(ptr);
      if(id && (!uid || uid == id))
        hypr_ipc_handle_window(ptr);
    }
  json_object_put(json);

  return TRUE;
}

static void hypr_ipc_track_workspace ( gchar *event )
{
  window_t *win;
  gpointer id, wsid;
  gchar *ws;

  id = hypr_ipc_parse_id(event, &ws);
  if(!ws || *ws!=',' || !(win = wintree_from_id(id)))
    return;

  wsid = hypr_ipc_parse_ws(ws+1, NULL);
  if(GPOINTER_TO_SIZE(wsid)<0)
    win->state |= WS_MINIMIZED;
  else
  {
    win->state &= ~WS_MINIMIZED;
    wintree_set_workspace(id, GINT_TO_POINTER(wsid));
  }
  wintree_commit(win);
}

static GdkRectangle hypr_ipc_get_output_geom ( gpointer wsid )
{
  json_object *json, *iter;
  gint i, scale;
  gchar *monitor = NULL;
  GdkRectangle res;

  res.x = -1;
  res.y = -1;
  res.width = -1;
  res.height = -1;
  if(!hypr_ipc_request(ipc_sockaddr, "j/workspaces", &json) || !json)
    return res;
  if( json_object_is_type(json, json_type_array) )
    for(i=0; i<json_object_array_length(json); i++)
    {
      iter = json_object_array_get_idx(json, i);
      if(json_int_by_name(iter, "id", -1) == GPOINTER_TO_INT(wsid))
        monitor = g_strdup(json_string_by_name(iter, "monitor"));
    }
  json_object_put(json);
  if(!monitor)
    return res;
  if(hypr_ipc_request(ipc_sockaddr, "j/monitors", &json) && json)
    if( json_object_is_type(json, json_type_array) )
      for(i=0; i<json_object_array_length(json); i++)
      {
        iter = json_object_array_get_idx(json, i);
        if(!g_strcmp0(monitor, json_string_by_name(iter, "name")))
        {
          scale = json_int_by_name(iter, "scale", 1);
          res.width = json_int_by_name(iter, "width", 0) / scale;
          res.height = json_int_by_name(iter, "height", 0) / scale;
        }
      }
  json_object_put(json);
  g_free(monitor);
  return res;
}

static guint hypr_ipc_get_geom ( gpointer wid, GdkRectangle *place,
    gpointer wsid, GdkRectangle **wins, GdkRectangle *space, gint *focus )
{
  json_object *json, *iter;
  gint i, n=0;

  *space = hypr_ipc_get_output_geom(wsid);
  if(space->width<0)
    return 0;
  if(!hypr_ipc_request(ipc_sockaddr, "j/clients", &json) || !json)
    return 0;
  if( json_object_is_type(json, json_type_array) )
  {
    *wins = g_malloc(sizeof(GdkRectangle)*json_object_array_length(json));
    for(i=0; i<json_object_array_length(json); i++)
    {
      iter = json_object_array_get_idx(json, i);
      if(hypr_ipc_workspace_id(iter)==wsid)
      {
        if(!wid || hypr_ipc_window_id(iter)!=wid)
        {
          hypr_ipc_window_geom(iter, &((*wins)[n]));
          if(hypr_ipc_window_id(iter)==wintree_get_focus())
            *focus = n;
          n++;
        }
        else if(place)
          hypr_ipc_window_geom(iter, place);
      }
    }
  }
  json_object_put(json);
  return n;
}

static void hypr_ipc_window_place ( gpointer wid )
{
  GdkRectangle window;

  if(wintree_placer_calc(wid, &window))
    hypr_ipc_command("dispatch movewindowpixel exact %d %d,address:0x%lx",
      window.x, window.y, GPOINTER_TO_SIZE(wid));
}

static void hypr_ipc_pager_populate( void )
{
  json_object *json, *ptr, *iter;
  gint i, wid;
  workspace_t *ws;

  if(!hypr_ipc_request(ipc_sockaddr, "j/workspaces", &json) || !json)
    return;
  if(json_object_is_type(json, json_type_array))
    for(i=0; i<json_object_array_length(json); i++)
    {
      ptr = json_object_array_get_idx(json, i);
      wid = json_int_by_name(ptr, "id", -1);
      if(wid>=0 && !workspace_from_id(GINT_TO_POINTER(wid)))
      {
        ws = workspace_new(GINT_TO_POINTER(wid));
        workspace_set_name(ws, json_string_by_name(ptr, "name"));
      }
    }
  json_object_put(json);
  if(!hypr_ipc_request(ipc_sockaddr, "j/monitors", &json) || !json)
    return;
  if(json_object_is_type(json, json_type_array))
    for(i=0; i<json_object_array_length(json); i++)
    {
      iter = json_object_array_get_idx(json, i);
      if(json_object_object_get_ex(iter, "activeWorkspace", &ptr) && ptr)
      {
        wid = json_int_by_name(ptr, "id", -99);
        if(wid!=-99 && (ws = workspace_from_id(GINT_TO_POINTER(wid))) )
        {
          if(json_bool_by_name(iter, "focused", FALSE))
            ws->state |= WS_STATE_FOCUSED | WS_STATE_INVALID;
          ws->state |= WS_STATE_VISIBLE | WS_STATE_INVALID;
          workspace_set_active(ws, json_string_by_name(iter, "name"));
          workspace_set_focus(ws->id);
        }
      }
    }
  json_object_put(json);
}

static void hypr_ipc_title_handle ( gchar *str )
{
  gchar *eptr;
  gpointer id;

  id = hypr_ipc_parse_id(str, &eptr);
  if(eptr && *eptr)
    wintree_set_title(id, eptr+1);
}

static void hypr_ipc_track_focus ( void )
{
  json_object *json;

  if(!hypr_ipc_request(ipc_sockaddr, "j/activewindow", &json) || !json)
    return;
  wintree_set_title(hypr_ipc_window_id(json),
      json_string_by_name(json, "title"));
  wintree_set_focus(hypr_ipc_window_id(json));
  json_object_put(json);
}

static void hypr_ipc_floating_set ( gchar *data )
{
  window_t *win;
  gchar *ptr;

  ptr = strchr(data, ',');
  if(!ptr || !(win = wintree_from_id(hypr_ipc_parse_id(data, NULL))) )
    return;

  win->floating = *(ptr+1)=='1';
  wintree_commit(win);
}

static void hypr_ipc_workspace_new ( gchar *data )
{
  gchar *eptr;
  gpointer id;
  workspace_t *ws;

  id = hypr_ipc_parse_ws(data, &eptr);
  if(!eptr || *eptr!=',')
    return;

  ws = workspace_new(id);
  workspace_set_name(ws, eptr+1);
}

static void hypr_ipc_set_maximized ( gboolean state )
{
  window_t *win;

  win = wintree_from_id(wintree_get_focus());
  if(!win)
    return;

  if(state)
    win->state |= WS_MAXIMIZED;
  else
    win->state &= ~WS_MAXIMIZED;
}

static void hypr_ipc_maximize ( gpointer id )
{
  window_t *win;
  gpointer focus;

  win = wintree_from_id(id);
  if(!win || win->state & WS_MAXIMIZED)
    return;

  focus = wintree_get_focus();
  wintree_set_focus(id);
  hypr_ipc_command("dispatch fullscreen 0");
  wintree_set_focus(focus);
}

static void hypr_ipc_unmaximize ( gpointer id )
{
  window_t *win;
  gpointer focus;

  win = wintree_from_id(id);
  if(!win || !(win->state & WS_MAXIMIZED))
    return;

  focus = wintree_get_focus();
  wintree_set_focus(id);
  hypr_ipc_command("dispatch fullscreen 0");
  wintree_set_focus(focus);
}

static void hypr_ipc_minimize ( gpointer id )
{
  window_t *win;
  json_object *json;

  win = wintree_from_id(id);
  if(!win || win->state & WS_MINIMIZED)
    return;

  if(wintree_get_disown())
    wintree_set_workspace(win->uid, NULL);
  else if(hypr_ipc_request(ipc_sockaddr, "j/activewindow", &json) && json)
  {
    wintree_set_workspace(win->uid, hypr_ipc_workspace_id(json));
    json_object_put(json);
  }
  hypr_ipc_command("dispatch movetoworkspacesilent special,address:0x%lx",
      GPOINTER_TO_SIZE(id));
}

static void hypr_ipc_unminimize ( gpointer id )
{
  window_t *win;

  if( (win = wintree_from_id(id)) && (win->state & WS_MINIMIZED))
    hypr_ipc_command("dispatch movetoworkspacesilent %d,address:0x%lx",
        GPOINTER_TO_INT(win->workspace? win->workspace->id :
          workspace_get_focused()), GPOINTER_TO_SIZE(id));
}

static void hypr_ipc_close ( gpointer id )
{
  hypr_ipc_command("dispatch closewindow address:0x%lx",GPOINTER_TO_SIZE(id));
}

static void hypr_ipc_focus ( gpointer id )
{
  window_t *win;

  if( (win = wintree_from_id(id)) && win->state & WS_MINIMIZED )
    hypr_ipc_unminimize(id);
  hypr_ipc_command("dispatch focuswindow address:0x%lx",
      GPOINTER_TO_SIZE(id));
}

static void hypr_ipc_set_workspace ( workspace_t *ws )
{
  gchar *name;

  if( (name = hypr_ipc_workspace_data(ws, "name")) )
    hypr_ipc_command("dispatch workspace name:%s", name);
  else
    hypr_ipc_command("dispatch workspace name:%s", ws->name);
  g_free(name);
}

static void hypr_ipc_move_to ( gpointer id, gpointer wsid )
{
  hypr_ipc_command("dispatch movetoworkspace %d,address:0x%lx",
      wsid, GPOINTER_TO_SIZE(id));
}

static struct wintree_api hypr_wintree_api = {
  .minimize = hypr_ipc_minimize,
  .unminimize = hypr_ipc_unminimize,
  .maximize = hypr_ipc_maximize,
  .unmaximize = hypr_ipc_unmaximize,
  .close = hypr_ipc_close,
  .focus = hypr_ipc_focus,
  .move_to = hypr_ipc_move_to,
};

static struct workspace_api hypr_workspace_api = {
  .set_workspace = hypr_ipc_set_workspace,
    .get_geom = hypr_ipc_get_geom
};

static gboolean hypr_ipc_event ( GIOChannel *chan, GIOCondition cond,
    gpointer data)
{
  gchar *event, *ptr;

  (void)g_io_channel_read_line(chan, &event, NULL, NULL, NULL);
  while(event)
  {
    if((ptr=strchr(event, '\n')))
      *ptr=0;
    g_debug("hypr event: %s", event);
    if(!strncmp(event, "activewindowv2>>", 16))
      wintree_set_focus(hypr_ipc_parse_id(event+16, NULL));
    else if(!strncmp(event, "windowtitlev2>>", 15))
      hypr_ipc_title_handle(event+15);
    else if(!strncmp(event, "openwindow>>", 12))
    {
      hypr_ipc_get_clients(hypr_ipc_parse_id(event+12, NULL));
      hypr_ipc_window_place(hypr_ipc_parse_id(event+12, NULL));
    }
    else if(!strncmp(event, "closewindow>>", 13))
      wintree_window_delete(hypr_ipc_parse_id(event+13, NULL));
    else if(!strncmp(event, "fullscreen>>",12))
      hypr_ipc_set_maximized(g_ascii_digit_value(*(event+12)));
    else if(!strncmp(event, "movewindowv2>>", 14))
      hypr_ipc_track_workspace(event+14);
    else if(!strncmp(event, "workspacev2>>", 13))
      workspace_set_focus(hypr_ipc_parse_ws(event+13, NULL));
    else if(!strncmp(event, "focusedmonv2>>", 14))
    {
      if( (ptr = strchr(event+14, ',')) )
        workspace_set_focus(hypr_ipc_parse_ws(ptr+1, NULL));
    }
    else if(!strncmp(event, "createworkspacev2>>", 19))
      hypr_ipc_workspace_new(event+19);
    else if(!strncmp(event, "changefloatingmode>>", 20))
      hypr_ipc_floating_set(event+20);
    else if(!strncmp(event, "destroyworkspacev2>>", 20))
      workspace_unref(hypr_ipc_parse_ws(event+20, NULL));
    g_free(event);
    (void)g_io_channel_read_line(chan, &event, NULL, NULL, NULL);
  }

  return TRUE;
}

void hypr_ipc_init ( void )
{
  gchar *sockaddr;
  gint sock;

  if(wintree_api_check())
    return;


  ipc_sockaddr = g_build_filename(g_get_user_runtime_dir(), "hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"), ".socket.sock", NULL);
  if(!hypr_ipc_get_clients(NULL))
  {
    g_free(ipc_sockaddr);
    return;
  }

  workspace_api_register(&hypr_workspace_api);
  wintree_api_register(&hypr_wintree_api);
  hypr_ipc_track_focus();

  sockaddr = g_build_filename(g_get_user_runtime_dir(), "hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"), ".socket2.sock", NULL);
  sock = socket_connect(sockaddr, 10);
  if(sock!=-1)
    g_io_add_watch(g_io_channel_unix_new(sock), G_IO_IN, hypr_ipc_event, NULL);
  g_free(sockaddr);
  hypr_ipc_pager_populate();
}
