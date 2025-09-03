/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "scanner.h"
#include "module.h"
#include "trigger.h"
#include "wintree.h"
#include "gui/bar.h"
#include "util/json.h"
#include "vm/vm.h"

static gint main_ipc;
static ScanFile *sway_file;

extern gchar *sockname;

static json_object *sway_ipc_poll ( gint sock, guint32 *etype )
{
  gint8 sway_ipc_header[14] = {0x69, 0x33, 0x2d, 0x69, 0x70, 0x63};
  guint32 len;

  if(recv_retry(sock, sway_ipc_header, 14)!=14)
    return NULL;

  if(etype)
    memcpy(etype, sway_ipc_header + 10, sizeof(guint32));

  memcpy(&len, sway_ipc_header + 6, sizeof(guint32));
  return recv_json(sock, len);
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

static void sway_ipc_window_place ( gint wid, gint64 pid )
{
  GdkRectangle place;

  if(wintree_placer_calc(GINT_TO_POINTER(wid), &place))
    sway_ipc_command("[con_id=%d] move absolute position %d %d",
        wid, place.x, place.y);
}

static void sway_window_handle ( struct json_object *container,
    const gchar *parent, const gchar *monitor )
{
  struct json_object *ptr;
  gpointer wid;
  window_t *win;
  const gchar *app_id;
  guint16 state;

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
    if(g_strcmp0(parent, "__i3_scratch"))
    {
      wintree_set_workspace(win->uid, workspace_id_from_name(parent));
      sway_ipc_window_place(GPOINTER_TO_INT(wid), win->pid);
    }
  }

  state = win->state;

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

  if(state!=win->state)
    wintree_commit(win);

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

  if( !(id = GINT_TO_POINTER(json_int_by_name(obj, "id", 0))) )
    return NULL;

  if( !(ws = workspace_from_id(id)) && 
    !(ws = workspace_from_name(json_string_by_name(obj, "name"))) )
  {
    ws = workspace_new(id);
    workspace_set_name(ws, json_string_by_name(obj, "name"));
  }
  else
  {
    workspace_ref(ws->id);
    ws->id = id;
  }

  if(json_bool_by_name(obj, "focused", FALSE))
  {
    workspace_change_focus(id);
    workspace_set_active(ws, json_string_by_name(obj, "output"));
  }
  workspace_mod_state(id, WS_STATE_URGENT,
      json_bool_by_name(obj, "urgent", FALSE));
  workspace_mod_state(id, WS_STATE_VISIBLE,
      json_bool_by_name(obj, "visible", FALSE));

  workspace_commit(ws);

  return ws;
}

static void sway_ipc_workspace_event ( struct json_object *obj )
{
  struct json_object *ws_obj;
  const gchar *change;
  gpointer id;

  if( !(change = json_string_by_name(obj, "change")) )
    return;
  if( !json_object_object_get_ex(obj, "current", &ws_obj) || !ws_obj)
    return;

  if( !(id = GINT_TO_POINTER(json_int_by_name(ws_obj, "id", 0))) )
    return;

  if(!g_strcmp0(change, "empty"))
    workspace_unref(id);
  else if(!g_strcmp0(change, "init"))
    sway_ipc_workspace_new(ws_obj);
  else if(!g_strcmp0(change, "focus"))
  {
    workspace_change_focus(id);
    workspace_set_active(workspace_from_id(id),
        json_string_by_name(ws_obj, "output"));
  }
  else if(!g_strcmp0(change, "urgent"))
    workspace_mod_state(id, WS_STATE_URGENT,
        json_bool_by_name(ws_obj, "urgent", FALSE));
  else if(!g_strcmp0(change, "visible"))
    workspace_mod_state(id, WS_STATE_VISIBLE,
        json_bool_by_name(ws_obj, "visible", FALSE));
  else if(!g_strcmp0(change, "move"))
    workspace_set_active(workspace_from_id(id),
        json_string_by_name(ws_obj, "output"));
  else
    return;

  workspace_commit(workspace_from_id(id));
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

static void sway_ipc_scan_input ( struct json_object *obj, guint32 etype )
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
  trigger_emit("sway");
}

static gboolean sway_ipc_event ( GIOChannel *chan, GIOCondition cond,
    gpointer data )
{
  struct json_object *obj;
  guint32 etype;

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
        trigger_emit("switcher_forward");
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

  win = wintree_from_id(id);
  if(!win)
    return;
  if(win->workspace)
    sway_ipc_command("[con_id=%d] move window to workspace %s",
        GPOINTER_TO_INT(id), win->workspace->name);
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

  win = wintree_from_id(id);
  if(!win)
    return;
  if(win->workspace && win->floating)
    sway_ipc_command("[con_id=%d] move window to workspace %s",
        GPOINTER_TO_INT(id), win->workspace->name);
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
  .custom_ipc = "sway",
  .minimize = sway_ipc_minimize,
  .unminimize = sway_ipc_unminimize,
  .maximize = sway_ipc_maximize,
  .unmaximize = sway_ipc_unmaximize,
  .close = sway_ipc_close,
  .focus = sway_ipc_focus,
  .move_to = sway_ipc_move_to,
};

/* workspace API */

static guint sway_ipc_get_geom ( gpointer wid, GdkRectangle *place,
    gpointer wsid, GdkRectangle **wins, GdkRectangle *space, gint *focus )
{
  struct json_object *obj = NULL;
  struct json_object *iter, *fiter, *arr;
  gint i, j, c, n = 0;

  obj = sway_ipc_request("", 1);

  *wins = NULL;
  *focus = -1;
  if(obj && json_object_is_type(obj, json_type_array))
    for(i=0; i<json_object_array_length(obj); i++)
    {
      iter = json_object_array_get_idx(obj, i);
      if(GPOINTER_TO_INT(wsid) == json_int_by_name(iter, "id", 0) &&
          (arr = json_array_by_name(iter, "floating_nodes")) )
      {
        *space = sway_ipc_parse_rect(iter);
        n = json_object_array_length(arr);
        c = 0;
        *wins = g_malloc0(n * sizeof(GdkRectangle));
        for(j=0; j<n; j++)
        {
          fiter = json_object_array_get_idx(arr, j);
          if(!wid || json_int_by_name(fiter, "id", 0)!=GPOINTER_TO_INT(wid))
            (*wins)[c++] = sway_ipc_parse_rect(fiter);
          else if(place)
            *place = sway_ipc_parse_rect(fiter);
          if(json_bool_by_name(fiter, "focused", FALSE))
            *focus = j;
        }
        json_object_put(obj);
        return c;
      }
    }

  json_object_put(obj);
  return 0;
}


static void sway_ipc_set_workspace ( workspace_t *ws )
{
  sway_ipc_command("workspace '%s'", ws->name);
}

static struct workspace_api sway_workspace_api = {
  .set_workspace = sway_ipc_set_workspace,
  .get_geom = sway_ipc_get_geom
};

static value_t sway_ipc_cmd_action ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "SwayCmd");
  vm_param_check_string(vm, p, 0, "SwayCmd");
  sway_ipc_command("%s", value_get_string(p[0]));

  return value_na;
}

static value_t sway_ipc_wincmd_action ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "SwayWinCmd");
  vm_param_check_string(vm, p, 0, "SwayWinCmd");

  if(VM_WINDOW(vm))
    sway_ipc_command("[con_id=%ld] %s", GPOINTER_TO_INT(VM_WINDOW(vm)),
        value_get_string(p[0]));

  return value_na;
}

void sway_ipc_init ( void )
{
  struct json_object *obj;
  gint sock;

  if(wintree_api_check() || ((sock=sway_ipc_open(1000))==-1) )
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
  vm_func_add("swaycmd", sway_ipc_cmd_action, TRUE, FALSE);
  vm_func_add("swaywincmd", sway_ipc_wincmd_action, TRUE, FALSE);
  sway_ipc_send(main_ipc, 2, "['workspace','mode','window','barconfig_update',\
      'binding','shutdown','tick','bar_state_update','input']");
  g_io_add_watch(g_io_channel_unix_new(main_ipc), G_IO_IN, sway_ipc_event,
      NULL);
}
