#include "sfwbar.h"
#include "wintree.h"
#include <sys/socket.h>

static gchar *ipc_sockaddr;

gpointer hypr_ipc_id_from_json ( json_object *json )
{
  const gchar *str;

  str = json_string_by_name(json,"address");
  if(!str)
    return NULL;

  return GINT_TO_POINTER(g_ascii_strtoull(str,NULL,16));
}

gboolean hypr_ipc_send ( gchar *addr, gchar *command, gchar **response )
{
  gint sock;
  static gchar buf[1024];
  gchar *result = NULL,*tmp;
  ssize_t rlen,total=0;

  sock = socket_connect(addr,10);
  if(sock==-1 || !command)
    return FALSE;

  if(write(sock,command,strlen(command))==-1)
  {
    close(sock);
    return FALSE;
  }

  if(!response)
  {
    close(sock);
    return TRUE;
  }

  do
  {
    rlen = recv(sock,buf,1024,0);
    if(rlen<=0)
      break;
    tmp = result;
    result = g_malloc(total+rlen+1);
    memcpy(result,tmp,total);
    memcpy(result+total,buf,rlen);
    *(result+total+rlen)='\0';
    g_free(tmp);
    total += rlen;
  } while(rlen>0);

  close(sock);
  *response = result;
  return TRUE;
}

void hypr_ipc_command ( gchar *cmd, ... )
{
  va_list args;
  gchar *buf;
  
  if(!cmd)
    return;

  va_start(args,cmd);
  buf = g_strdup_vprintf(cmd,args);
  hypr_ipc_send ( ipc_sockaddr, buf, NULL);
  g_free(buf);
  va_end(args);
}

void hypr_ipc_handle_window ( json_object *obj )
{
  window_t *win;
  gpointer id;
  const gchar *title;
  json_object *ptr;

  id = hypr_ipc_id_from_json(obj);
  if(!id)
    return;

  win = wintree_from_id(id);
  if(!win)
  {
    win = wintree_window_init();
    win->uid = id;
    win->pid = json_int_by_name(obj,"pid",0);
    wintree_window_append(win);
    wintree_set_app_id(id,json_string_by_name(obj,"class"));
  }
  title = json_string_by_name(obj,"title");
  if(g_strcmp0(title,win->title))
    wintree_set_title(id,title);
  json_object_object_get_ex(obj,"workspace",&ptr);
  if(ptr)
  {
    if(json_int_by_name(ptr,"id",0)==-99)
      win->state |= WS_MINIMIZED;
    else
      win->state &= ~WS_MINIMIZED;
  }
}

gboolean hypr_ipc_get_clients ( gpointer *uid )
{
  gchar *response;
  json_object *json, *ptr;
  gpointer id;
  gint i;

  hypr_ipc_send(ipc_sockaddr,"j/clients",&response);
  if(response)
  {
    json = json_tokener_parse(response);
    if( json && json_object_is_type(json, json_type_array) )
      for(i=0;i<json_object_array_length(json);i++)
      {
        ptr = json_object_array_get_idx(json,i);
        id = hypr_ipc_id_from_json(ptr);
        if(id && (!uid || uid == id))
          hypr_ipc_handle_window(ptr);
      }
    if(json)
      json_object_put(json);
    g_free(response);
  }

  return TRUE;
}

void hypr_ipc_track_minimized ( gchar *event )
{
  gpointer *id;
  window_t *win;
  gchar *ws;

  id = GINT_TO_POINTER(g_ascii_strtoull(event,&ws,16));
  win = wintree_from_id(id);
  if(!win || !ws || *ws!=',')
    return;

  if(ws && !strncmp(ws,",special",8))
    win->state |= WS_MINIMIZED;
  else
    win->state &= ~WS_MINIMIZED;
}

void hypr_ipc_maximize ( gpointer id )
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

void hypr_ipc_unmaximize ( gpointer id )
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

void hypr_ipc_minimize ( gpointer id )
{
  window_t *win;
  gpointer focus;
  gchar *response;
  json_object *json, *ptr;

  win = wintree_from_id(id);
  if(!win || win->state & WS_MINIMIZED)
    return;

  focus = wintree_get_focus();
  if(focus!=id)
    wintree_set_focus(id);
  hypr_ipc_send(ipc_sockaddr,"j/activewindow",&response);
  if(response)
  {
    json = json_tokener_parse(response);
    if(json)
    {
      json_object_object_get_ex(json,"workspace",&ptr);
      if(ptr)
        win->workspace = GINT_TO_POINTER(json_int_by_name(ptr,"id",0));
      json_object_put(json);
    }
    g_free(response);
    hypr_ipc_command("dispatch movetoworkspace special");
    hypr_ipc_command("workspace %ld",GPOINTER_TO_INT(win->workspace));
  }
  if(focus!=id)
    wintree_set_focus(focus);
}

void hypr_ipc_unminimize ( gpointer id )
{
  window_t *win;

  win = wintree_from_id(id);
  if(!win || !(win->state & WS_MINIMIZED))
    return;

  if(!win->workspace)
    return; // we should set win->workspace to current ws

  hypr_ipc_command("dispatch movetoworkspace %ld,address:0x%lx",
      GPOINTER_TO_INT(win->workspace),GPOINTER_TO_INT(id));
}

void hypr_ipc_close ( gpointer id )
{
  hypr_ipc_command("dispatch closewindow address:0x%lx",GPOINTER_TO_INT(id));
}

void hypr_ipc_focus ( gpointer id )
{
  window_t *win;

  win = wintree_from_id(id);
  if(win && win->state & WS_MINIMIZED)
    hypr_ipc_unminimize(id);
  hypr_ipc_command("dispatch focuswindow address:0x%lx",GPOINTER_TO_INT(id));
}

static struct wintree_api hypr_wintree_api = {
  .minimize = hypr_ipc_minimize,
  .unminimize = hypr_ipc_unminimize,
  .maximize = hypr_ipc_maximize,
  .unmaximize = hypr_ipc_unmaximize,
  .close = hypr_ipc_close,
  .focus = hypr_ipc_focus
};

void hypr_ipc_track_focus ( void )
{
  gchar *response;
  json_object *json;

  hypr_ipc_send(ipc_sockaddr,"j/activewindow",&response);
  if(response)
  {
    json = json_tokener_parse(response);
    if(json)
    {
      wintree_set_focus(hypr_ipc_id_from_json(json));
      json_object_put(json);
    }
  }
  g_free(response);
}

void hypr_ipc_set_maximized ( gboolean state )
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

gboolean hypr_ipc_event ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  gchar *event;

  g_io_channel_read_line(chan,&event,NULL,NULL,NULL);
  if(!event)
    return TRUE;
  if(!strncmp(event,"activewindow>>",14))
    hypr_ipc_track_focus();
  else if(!strncmp(event,"openwindow>>",12))
    hypr_ipc_get_clients(GINT_TO_POINTER(g_ascii_strtoull(event+12,NULL,16)));
  else if(!strncmp(event,"closewindow>>",13))
    wintree_window_delete(GINT_TO_POINTER(g_ascii_strtoull(
            event+13,NULL,16)));
  else if(!strncmp(event,"fullscreen>>",12))
    hypr_ipc_set_maximized(g_ascii_digit_value(*(event+12)));
  else if(!strncmp(event,"movewindow>>",12))
    hypr_ipc_track_minimized(event+12);

  g_free(event);
  return TRUE;
}

void hypr_ipc_init ( void )
{
  gchar *sockaddr;
  gint sock;

  if(ipc_get())
    return;

  ipc_sockaddr = g_build_filename("/tmp/hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"),".socket.sock",NULL);
  if(hypr_ipc_get_clients(NULL))
    ipc_set(IPC_HYPR);
  hypr_ipc_track_focus();

  sockaddr = g_build_filename("/tmp","hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"),".socket2.sock",NULL);
  sock = socket_connect(sockaddr,10);
  if(sock!=-1)
    g_io_add_watch(g_io_channel_unix_new(sock),G_IO_IN,hypr_ipc_event,NULL);
  g_free(sockaddr);
  wintree_api_register(&hypr_wintree_api);
}
