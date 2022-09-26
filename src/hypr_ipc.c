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

void hypr_ipc_track_focus ( void )
{
  gint sock;
  gchar *response;
  json_object *json;

  sock = socket_connect(ipc_sockaddr,10);
  if(sock<1)
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

gboolean hypr_ipc_event ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  gint sock;
  gchar *event;

  sock = g_io_channel_unix_get_fd(chan);
  event = hypr_ipc_poll(sock);

  if(!strncmp(event,"activewindow>>",14))
    hypr_ipc_track_focus();
  g_free(event);
  return TRUE;
}

void hypr_ipc_init ( void )
{
  gchar *sockaddr;
  gchar *response;
  gint sock, sock2, i;
  json_object *json, *ptr;
  window_t *win;
  gpointer id;

  if(ipc_get())
    return;

  ipc_sockaddr = g_build_filename("/tmp/hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"),".socket.sock",NULL);
  sock = socket_connect(ipc_sockaddr,10);
  if(sock<1)
    return;
  ipc_set(IPC_HYPR);
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
        if(id)
          {
            win = wintree_from_id(id);
            if(!win)
            {
              win = wintree_window_init();
              win->uid = id;
              win->pid = json_int_by_name(ptr,"pid",0);
              wintree_window_append(win);
            }
            wintree_set_app_id(id,json_string_by_name(ptr,"class"));
            wintree_set_title(id,json_string_by_name(ptr,"title"));
          }
      }
    if(json)
      json_object_put(json);
    g_free(response);
  }
  close(sock);
  hypr_ipc_track_focus();

  sockaddr = g_build_filename("/tmp","hypr",
      g_getenv("HYPRLAND_INSTANCE_SIGNATURE"),".socket2.sock",NULL);
  sock2 = socket_connect(sockaddr,10);
  g_io_add_watch(g_io_channel_unix_new(sock2),G_IO_IN,hypr_ipc_event,NULL);
  g_free(sockaddr);
}
