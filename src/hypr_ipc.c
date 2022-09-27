#include "sfwbar.h"
#include "wintree.h"
#include <sys/socket.h>

static gchar *ipc_sockaddr;

guint64 hypr_ipc_id_from_json ( json_object *json )
{
  return strtoull(json_string_by_name(json,"address"),NULL,16);
}

gboolean hypr_ipc_send ( gint sock, gchar *command )
{
  if(sock<0 || !command)
    return FALSE;

  return (write(sock,command,strlen(command))!=-1);
}

gchar *hypr_ipc_poll ( gint sock )
{
  static gchar buf[1024];
  gchar *result = NULL,*tmp;
  ssize_t rlen,total=0;

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

  return result;
}

void hypr_ipc_command ( gchar *cmd, ... )
{
  va_list args;
  gchar *buf;
  gint sock;
  
  if(!cmd)
    return;
  sock = socket_connect(ipc_sockaddr,10);
  if(sock<1)
    return;

  va_start(args,cmd);
  buf = g_strdup_vprintf(cmd,args);
  g_message("command: '%s'",buf);
  hypr_ipc_send ( sock, buf);
  g_free(buf);
  va_end(args);
  close(sock);
}

void hypr_ipc_handle_window ( json_object *obj )
{
  window_t *win;
  gpointer id;

  id = GINT_TO_POINTER(hypr_ipc_id_from_json(obj));
  if(!id)
    return;

  win = wintree_from_id(id);
  if(!win)
  {
    win = wintree_window_init();
    win->uid = id;
    g_message("id %p",id);
    win->pid = json_int_by_name(obj,"pid",0);
    wintree_window_append(win);
  }
  wintree_set_app_id(id,json_string_by_name(obj,"class"));
  wintree_set_title(id,json_string_by_name(obj,"title"));
}

gboolean hypr_ipc_get_clients ( gpointer *uid )
{
  gchar *response;
  json_object *json, *ptr;
  gpointer id;
  gint sock,i;

  sock = socket_connect(ipc_sockaddr,10);
  if(sock==-1)
    return FALSE;
  hypr_ipc_send(sock,"j/clients");
  response = hypr_ipc_poll(sock);
  if(response)
  {
    json = json_tokener_parse(response);
    if( json && json_object_is_type(json, json_type_array) )
      for(i=0;i<json_object_array_length(json);i++)
      {
        ptr = json_object_array_get_idx(json,i);
        id = GINT_TO_POINTER(hypr_ipc_id_from_json(ptr));
        if(id && (!uid || uid == id))
          hypr_ipc_handle_window(ptr);
      }
    if(json)
      json_object_put(json);
    g_free(response);
  }
  close(sock);

  return TRUE;
}

void hypr_ipc_focus ( gpointer id )
{
  hypr_ipc_command("dispatch focuswindow address:0x%lx",GPOINTER_TO_INT(id));
}

void hypr_ipc_maximize ( gpointer id )
{
  gpointer focus;

  focus = wintree_get_focus();
  wintree_set_focus(id);
  hypr_ipc_command("dispatch fullscreen 0");
  wintree_set_focus(focus);
}

void hypr_ipc_minimize ( gpointer id )
{
}

static struct wintree_api hypr_wintree_api = {
  .minimize = NULL,
  .unminimize = NULL,
  .maximize = hypr_ipc_maximize,
  .unmaximize = hypr_ipc_maximize,
  .close = NULL,
  .focus = hypr_ipc_focus
};

void hypr_ipc_track_focus ( void )
{
  gint sock;
  gchar *response;
  json_object *json;

  sock = socket_connect(ipc_sockaddr,10);
  if(sock==-1)
    return;
  hypr_ipc_send(sock,"j/activewindow");
  response = hypr_ipc_poll(sock);
  if(response)
  {
    json = json_tokener_parse(response);
    if(json)
    {
      wintree_set_focus(GINT_TO_POINTER(hypr_ipc_id_from_json(json)));
      json_object_put(json);
    }
  }
  g_free(response);
  close(sock);
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
  g_message("event: %s",event);
  if(!strncmp(event,"activewindow>>",14))
    hypr_ipc_track_focus();
  else if(!strncmp(event,"openwindow>>",12))
    hypr_ipc_get_clients(GINT_TO_POINTER(g_ascii_strtoull(event+12,NULL,16)));
  else if(!strncmp(event,"closewindow>>",13))
    wintree_window_delete(GINT_TO_POINTER(g_ascii_strtoull(
            event+13,NULL,16)));
  else if(!strncmp(event,"fullscreen>>",12))
    hypr_ipc_set_maximized(g_ascii_digit_value(*(event+12)));

  g_free(event);
  return TRUE;
}

void hypr_ipc_init ( void )
{
  gchar *sockaddr;
  gint sock2;

  if(ipc_get())
    return;

  ipc_sockaddr = g_build_filename("/tmp/hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"),".socket.sock",NULL);
  if(hypr_ipc_get_clients(NULL))
    ipc_set(IPC_HYPR);
  hypr_ipc_track_focus();

  sockaddr = g_build_filename("/tmp","hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"),".socket2.sock",NULL);
  sock2 = socket_connect(sockaddr,10);
  if(sock2!=-1)
    g_io_add_watch(g_io_channel_unix_new(sock2),G_IO_IN,hypr_ipc_event,NULL);
  g_free(sockaddr);
  wintree_api_register(&hypr_wintree_api);
}
