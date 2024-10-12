/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "scanner.h"
#include "module.h"
#include "bar.h"
#include "switcher.h"
#include "wintree.h"
#include "util/json.h"

static gint main_ipc;
static ScanFile *sway_file;

extern gchar *sockname;

static json_object *sway_ipc_poll ( gint sock, gint32 *etype )
{
  static gint8 sway_ipc_header[14] = {0x69, 0x33, 0x2d, 0x69, 0x70, 0x63};
  static guint32 *sway_ipc_len = (void *)&sway_ipc_header + 6;
  static guint32 *sway_ipc_type = (void *)&sway_ipc_header + 10;

  if(recv_retry(sock, sway_ipc_header, 14)!=14)
    return NULL;

  if(etype)
    *etype = *sway_ipc_type;
  return recv_json(sock, *sway_ipc_len);
}

static int sway_ipc_open (int to)
{
  const gchar *socket_path;

  if( !(socket_path = sockname?sockname:g_getenv("SWAYSOCK")) )
    return -1;
  return socket_connect(socket_path, to);
}

static int sway_ipc_send ( gint sock, gint32 type, gchar *command )
{
  static gint8 sway_ipc_header[14] = {0x69, 0x33, 0x2d, 0x69, 0x70, 0x63};
  static guint32 *sway_ipc_len = (void *)&sway_ipc_header + 6;
  static guint32 *sway_ipc_type = (void *)&sway_ipc_header + 10;

  *sway_ipc_len = strlen(command);
  *sway_ipc_type = type;
  if(write(sock, sway_ipc_header, sizeof(sway_ipc_header))==-1)
    return -1;
  if(*sway_ipc_len>0)
    if(write(sock, command, *sway_ipc_len)==-1)
      return -1;
  return 0;
}

static void sway_ipc_command ( gchar *cmd, ... )
{
  va_list args;
  gchar *buf;
  
  if(!cmd || main_ipc<1)
    return;

  va_start(args, cmd);
  buf = g_strdup_vprintf(cmd, args);
  sway_ipc_send (main_ipc, 0, buf);
  g_free(buf);
  va_end(args);
}

static json_object *sway_ipc_request ( gchar *command, gint32 type )
{
  gint sock;
  json_object *json;

  sock = sway_ipc_open(3000);
  if(sock==-1)
    return NULL;
  sway_ipc_send(sock, type, command);
  json = sway_ipc_poll(sock, NULL);
  close(sock);

  return json;
}

static GdkRectangle sway_ipc_parse_rect ( struct json_object *obj )
{
  struct json_object *rect;
  GdkRectangle eret = {-1, -1, -1, -1};

  if(json_object_object_get_ex(obj, "rect", &rect))
    return json_rect_get(rect);
  
  return eret;
}

static struct json_object *placement_find_wid ( struct json_object *obj,
    gint64 wid )
{
  json_object *arr, *ret;
  gint i;

  if( (arr = json_array_by_name(obj, "floating_nodes")) )
    for(i=0; i<json_object_array_length(arr); i++)
      if(json_int_by_name(json_object_array_get_idx(arr, i), "id", wid+1)==wid)
        return obj;
  if( (arr = json_array_by_name(obj, "nodes")) )
    for(i=0; i<json_object_array_length(arr); i++)
      if( (ret = placement_find_wid(json_object_array_get_idx(arr, i), wid)) )
        return ret;
  return NULL;
}

static void sway_ipc_window_place ( gint wid, gint64 pid )
{
  struct json_object *json, *obj, *item, *arr;
  GdkRectangle output, win, *obs;
  gint c, i, nobs;

  if(!wintree_placer_check(pid))
    return;

  if( !(json = sway_ipc_request("", 4)) )
    return;

  if( !(obj = placement_find_wid (json, wid)) ||
      !(arr = json_array_by_name(obj, "floating_nodes")))
  {
    json_object_put(json);
    return;
  }
  output = sway_ipc_parse_rect(obj);
  win = output;
  nobs = json_object_array_length(arr)-1;
  obs = g_malloc(nobs*sizeof(GdkRectangle));
  c=0;
  for(i=0; i<=nobs; i++)
  {
    item = json_object_array_get_idx(arr, i);
    if(json_int_by_name(item, "id", wid+1) == wid)
      win = sway_ipc_parse_rect(item);
    else if(c<nobs)
      obs[c++] = sway_ipc_parse_rect(item);
  }
  if(c==nobs)
  {
    wintree_placer_calc(nobs, obs, output, &win);
    sway_ipc_command("[con_id=%d] move absolute position %d %d",
        wid, win.x, win.y);
  }
  g_free(obs);
  json_object_put(json);
}

static void sway_window_handle ( struct json_object *container,
    const gchar *parent, const gchar *monitor )
{
  struct json_object *ptr;
  gpointer wid;
  window_t *win;
  const gchar *app_id;

  wid = GINT_TO_POINTER(json_int_by_name(container, "id", G_MININT64));
  if( !(win = wintree_from_id(GINT_TO_POINTER(wid))) )
  {
    if( !(app_id = json_string_by_name(container, "app_id")) )
    {
      if(json_object_object_get_ex(container, "window_properties", &ptr))
        app_id = json_string_by_name(ptr, "instance");
      else
        app_id = "";
    }

    win = wintree_window_init();
    win->uid = wid;
    win->pid = json_int_by_name(container, "pid", G_MININT64);
    wintree_window_append(win);
    wintree_set_app_id(wid, app_id);
    wintree_set_title(wid, json_string_by_name(container, "name"));
    wintree_set_float(wid,
        !g_strcmp0(json_string_by_name(container, "type"), "floating_con"));
    wintree_log(wid);
    sway_ipc_window_place(GPOINTER_TO_INT(wid), win->pid);
  }

  if(json_bool_by_name(container, "focused", FALSE))
    wintree_set_focus(wid);
  set_bit(win->state, WS_FULLSCREEN | WS_MAXIMIZED,
      json_int_by_name(container, "fullscreen_mode", 0));
  set_bit(win->state, WS_URGENT, json_int_by_name(container, "urgent", 0));

  if(!g_strcmp0(parent, "__i3_scratch"))
    win->state |= WS_MINIMIZED;
  else
  {
    win->state &= ~WS_MINIMIZED;
    wintree_set_workspace(win->uid, workspace_id_from_name(parent));
  }

  if(!g_list_find_custom(win->outputs, monitor, (GCompareFunc)g_strcmp0) &&
      g_strcmp0(monitor, "__i3"))
  {
    g_list_free_full(win->outputs, g_free);
    win->outputs = g_list_prepend(NULL, g_strdup(monitor));
    wintree_commit(win);
  }
}

static void sway_traverse_tree ( struct json_object *obj, const gchar *parent,
    const gchar *output)
{
  struct json_object *arr, *iter;
  const gchar *type, *name;
  gint i;

  if( (arr = json_array_by_name(obj, "floating_nodes")) )
    for(i=0; i<json_object_array_length(arr); i++)
      sway_window_handle(json_object_array_get_idx(arr, i), parent, output);

  if( (arr = json_array_by_name(obj, "nodes")) )
    for(i=0; i<json_object_array_length(arr); i++)
    {
      iter = json_object_array_get_idx(arr, i);
      if(json_string_by_name(iter, "app_id"))
        sway_window_handle(iter, parent, output);
      else
      {
        type = json_string_by_name(iter, "type");
        name = json_string_by_name(iter, "name");
        if(!g_strcmp0(type, "output"))
          sway_traverse_tree(iter, NULL, name);
        else if(!g_strcmp0(type, "workspace"))
          sway_traverse_tree(iter, name, output);
        else
          sway_traverse_tree(iter, parent, output);
      }
    }
}

void sway_ipc_client_init ( ScanFile *file )
{
  if(sway_file)
  {
    scanner_file_attach(sway_file->trigger, sway_file);
    scanner_file_merge(sway_file, file);
  }
  else
    sway_file = file;
}

static workspace_t *sway_ipc_workspace_new ( struct json_object *obj )
{
  workspace_t *ws;
  gpointer id;
  guint32 state = 0;

  if( !(id = GINT_TO_POINTER(json_int_by_name(obj, "id", 0))) )
    return NULL;

  ws = workspace_new(id);
  workspace_set_name(ws, json_string_by_name(obj, "name"));

  if(json_bool_by_name(obj, "focused", FALSE))
    state |= WS_STATE_FOCUSED;
  if(json_bool_by_name(obj, "urgent", FALSE))
    state |= WS_STATE_URGENT;
  if(json_bool_by_name(obj, "visible", FALSE))
    state |= WS_STATE_VISIBLE;
  workspace_set_state(ws, state);

  return ws;
}

static void sway_ipc_workspace_event ( struct json_object *obj )
{
  const gchar *change;
  gpointer id;
  struct json_object *current;

  json_object_object_get_ex(obj, "current", &current);
  if(!current)
    return;

  id = GINT_TO_POINTER(json_int_by_name(current, "id", 0));
  change = json_string_by_name(obj, "change");

  if(!g_strcmp0(change, "empty"))
    workspace_unref(id);
  else if(!g_strcmp0(change, "init"))
    sway_ipc_workspace_new(current);

  if(!g_strcmp0(change, "focus") || !g_strcmp0(change, "move"))
    workspace_set_active(workspace_from_id(id),
        json_string_by_name(current, "output"));
  /* we don't need a workspace commit after workspace_set_active as it will
     be called by workspace_set_focus */
  if(!g_strcmp0(change, "focus"))
    workspace_set_focus(id);
}

static void sway_ipc_workspace_populate ( void )
{
  struct json_object *robj;
  workspace_t *ws;
  gint i;

  robj = sway_ipc_request("", 1);

  if(!robj || !json_object_is_type(robj, json_type_array))
    return;
  for(i=0; i<json_object_array_length(robj); i++)
  {
    ws = sway_ipc_workspace_new(json_object_array_get_idx(robj, i));
    if(ws->state & WS_STATE_FOCUSED)
      workspace_set_active(ws,
          json_string_by_name(json_object_array_get_idx(robj, i), "output"));
    workspace_commit(ws);
  }
  json_object_put(robj);
}

static void sway_ipc_window_event ( struct json_object *obj )
{
  gpointer *wid;
  window_t *win;
  const gchar *change;
  struct json_object *container;

  if(!obj)
    return;

  if( !(change = json_string_by_name(obj, "change")) )
    return;

  json_object_object_get_ex(obj, "container", &container);
  wid = GINT_TO_POINTER(json_int_by_name(container, "id", G_MININT64));

  if(!g_strcmp0(change, "new"))
    sway_ipc_send(main_ipc, 4, ""); // get tree to map workspace
  else if(!g_strcmp0(change, "close"))
    wintree_window_delete(wid);
  else if(!g_strcmp0(change, "title"))
    wintree_set_title(wid, json_string_by_name(container, "name"));
  else if(!g_strcmp0(change, "focus"))
  {
    wintree_set_focus(wid);
    sway_ipc_send(main_ipc, 4, "");
  }
  else if(!g_strcmp0(change, "fullscreen_mode"))
  {
    if( (win = wintree_from_id(wid)) )
      set_bit(win->state, WS_FULLSCREEN | WS_MAXIMIZED,
            json_int_by_name(container, "fullscreen_mode", 0));
  }
  else if(!g_strcmp0(change, "urgent"))
  {
    if( (win = wintree_from_id(wid)) )
      set_bit(win->state, WS_URGENT,
            json_int_by_name(container, "urgent", 0));
  }
  else if(!g_strcmp0(change, "move"))
    sway_ipc_send(main_ipc, 4, "");
  else if(!g_strcmp0(change,"floating"))
    wintree_set_float(wid,!g_strcmp0(
          json_string_by_name(container, "type"), "floating_con"));
}

static void sway_ipc_scan_input ( struct json_object *obj, gint32 etype )
{
  struct json_object *scan;
  static gchar *ename[] = {
    "workspace",
    "",
    "mode",
    "window",
    "barconfig_update",
    "binding",
    "shutdown",
    "tick",
    "","","","","","","","","","","","",
    "bar_state_update",
    "input" };

  if(!sway_file || etype<0x80000000 || etype>0x80000015)
    return;

  scan = json_object_new_object();
  json_object_object_add_ex(scan, ename[etype-0x80000000], obj, 0);
  g_list_foreach(sway_file->vars, (GFunc)scanner_var_reset, NULL);
  scanner_update_json(scan, sway_file);
  json_object_get(obj);
  json_object_put(scan);
  base_widget_emit_trigger(g_intern_static_string("sway"));
}

static gboolean sway_ipc_event ( GIOChannel *chan, GIOCondition cond,
    gpointer data )
{
  struct json_object *obj;
  gint32 etype;

  if(main_ipc==-1)
    return FALSE;

  while ( (obj=sway_ipc_poll(main_ipc, &etype)) )
  { 
    if(etype==0x80000000)
      sway_ipc_workspace_event(obj);
    else if(etype==0x80000004)
    {
      bar_set_visibility(NULL, json_string_by_name(obj, "id"),
          *(json_string_by_name(obj, "mode")));
      if(g_strcmp0(json_string_by_name(obj, "hidden_state"), "hide"))
      {
        sway_ipc_command("bar %s hidden_state hide",
            json_string_by_name(obj, "id"));
        switcher_event(NULL);
      }
    }
    else if(etype==0x00000004)
      sway_traverse_tree(obj, NULL, NULL);
    else if(etype==0x80000003)
      sway_ipc_window_event(obj);
    else if(etype==0x80000014)
      bar_set_visibility(NULL, json_string_by_name(obj, "id"),
          json_bool_by_name(obj, "visible_by_modifier", FALSE)?'v':'x');

    sway_ipc_scan_input(obj, etype);

    json_object_put(obj);
  }
  return TRUE;
}

/* Window API */

static void sway_ipc_minimize ( gpointer id )
{
  if(wintree_get_disown())
    wintree_set_workspace(id, NULL);
  sway_ipc_command("[con_id=%d] move window to scratchpad",
      GPOINTER_TO_INT(id));
}

static void sway_ipc_unminimize ( gpointer id )
{
  window_t *win;
  workspace_t *ws;

  win = wintree_from_id(id);
  if(!win)
    return;
  ws = workspace_from_id(win->workspace);
  if(ws)
    sway_ipc_command("[con_id=%d] move window to workspace %s",
        GPOINTER_TO_INT(id), ws->name);
  else
    sway_ipc_command("[con_id=%d] focus", GPOINTER_TO_INT(id));
}

static void sway_ipc_maximize ( gpointer id )
{
  sway_ipc_command("[con_id=%d] fullscreen enable", GPOINTER_TO_INT(id));
}

static void sway_ipc_unmaximize ( gpointer id )
{
  sway_ipc_command("[con_id=%d] fullscreen disable", GPOINTER_TO_INT(id));
}

static void sway_ipc_focus ( gpointer id )
{
  window_t *win;
  workspace_t *ws;

  win = wintree_from_id(id);
  if(!win)
    return;
  ws = workspace_from_id(win->workspace);
  if(ws && win->floating)
    sway_ipc_command("[con_id=%d] move window to workspace %s",
        GPOINTER_TO_INT(id), ws->name);
  sway_ipc_command("[con_id=%d] focus", GPOINTER_TO_INT(id));
}

static void sway_ipc_close ( gpointer id )
{
  sway_ipc_command("[con_id=%d] kill", GPOINTER_TO_INT(id));
}

static void sway_ipc_move_to ( gpointer id, gpointer wsid )
{
  workspace_t *ws;

  ws = workspace_from_id(wsid);
  if(!ws || !ws->name)
    return;

  sway_ipc_command("[con_id=%d] move window to workspace %s",
      GPOINTER_TO_INT(id), ws->name);
}

static struct wintree_api sway_wintree_api = {
  .minimize = sway_ipc_minimize,
  .unminimize = sway_ipc_unminimize,
  .maximize = sway_ipc_maximize,
  .unmaximize = sway_ipc_unmaximize,
  .close = sway_ipc_close,
  .focus = sway_ipc_focus,
  .move_to = sway_ipc_move_to,
};

/* workspace API */

static void sway_ipc_set_workspace ( workspace_t *ws )
{
  sway_ipc_command("workspace '%s'", ws->name);
}

static guint sway_ipc_get_geom ( workspace_t *ws, GdkRectangle **wins,
    GdkRectangle *space, gint *focus )
{
  struct json_object *obj = NULL;
  struct json_object *iter, *fiter, *arr;
  gint i, j, n = 0;

  obj = sway_ipc_request("", 1);

  *wins = NULL;
  *focus = -1;
  if(obj && json_object_is_type(obj, json_type_array))
    for(i=0; i<json_object_array_length(obj); i++)
    {
      iter = json_object_array_get_idx(obj, i);
      if(!g_strcmp0(ws->name, json_string_by_name(iter, "name")) &&
          (arr = json_array_by_name(iter, "floating_nodes")) )
      {
        *space = sway_ipc_parse_rect(iter);
        n = json_object_array_length(arr);
        *wins = g_malloc0(n * sizeof(GdkRectangle));
        for(j=0; j<n; j++)
        {
          fiter = json_object_array_get_idx(arr, j);
          (*wins)[j] = sway_ipc_parse_rect(fiter);
          if(json_bool_by_name(fiter, "focused", FALSE))
            *focus = j;
        }
      }
    }

  json_object_put(obj);
  return n;
}

static struct workspace_api sway_workspace_api = {
  .set_workspace = sway_ipc_set_workspace,
  .get_geom = sway_ipc_get_geom
};

static void sway_ipc_cmd_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  sway_ipc_command("%s",cmd);
}

static ModuleActionHandlerV1 sway_ipc_cmd_handler = {
  .name = "SwayCmd",
  .function = (ModuleActionFunc)sway_ipc_cmd_action
};

static void sway_ipc_wincmd_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  sway_ipc_command("[con_id=%ld] %s", GPOINTER_TO_INT(win->uid), cmd);
}

static ModuleActionHandlerV1 sway_ipc_wincmd_handler = {
  .name = "SwayWinCmd",
  .function = (ModuleActionFunc)sway_ipc_wincmd_action
};

static ModuleActionHandlerV1 *sway_ipc_action_handlers[] = {
  &sway_ipc_cmd_handler,
  &sway_ipc_wincmd_handler,
  NULL
};

void sway_ipc_init ( void )
{
  struct json_object *obj;
  gint sock;

  if(wintree_api_check() || ((sock=sway_ipc_open(10))==-1) )
    return;
  workspace_api_register(&sway_workspace_api);
  wintree_api_register(&sway_wintree_api);

  sway_ipc_send(sock, 0, "bar hidden_state hide");
  if( (obj = sway_ipc_poll(sock, NULL)) )
    json_object_put(obj);
  sway_ipc_workspace_populate();
  sway_ipc_send(sock, 4, "");
  if( (obj = sway_ipc_poll(sock, NULL)) )
  {
    sway_traverse_tree(obj, NULL, NULL);
    json_object_put(obj);
  }
  close(sock);

  if((main_ipc = sway_ipc_open(10))<0)
    return;
  module_actions_add(sway_ipc_action_handlers, "sway ipc library");
  sway_ipc_send(main_ipc, 2, "['workspace','mode','window','barconfig_update',\
      'binding','shutdown','tick','bar_state_update','input']");
  g_io_add_watch(g_io_channel_unix_new(main_ipc), G_IO_IN, sway_ipc_event,
      NULL);
}
