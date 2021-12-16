/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "sfwbar.h"

static gint main_ipc;
static const  gint8 magic[6] = {0x69, 0x33, 0x2d, 0x69, 0x70, 0x63};

gboolean sway_ipc_active ( void )
{
  return ( main_ipc > 0 );
}

gchar *sway_ipc_poll ( gint sock, gint32 *etype )
{
  gint8 sway_ipc_header[14];
  gchar *response = NULL;
  guint32 plen;
  size_t pos=0;
  ssize_t rlen;

  while(pos<sizeof(sway_ipc_header))
  {
    rlen = recv(sock,(gchar *)sway_ipc_header+pos,sizeof(sway_ipc_header)-pos,0);
    if (rlen<=0)
      break;
    pos+=rlen;
  }

  if(pos==sizeof(sway_ipc_header))
  {
    pos=0;
    memcpy(etype,sway_ipc_header+sizeof(magic)+sizeof(plen),sizeof(plen));
    memcpy(&plen,sway_ipc_header+sizeof(magic),sizeof(plen));
    if(plen>65536)
      response=NULL;
    else
      response = g_malloc(plen+1);
    if ( response != NULL)
    {
      while(pos<plen)
      {
        rlen = recv(sock,(gchar *)response+pos,plen-pos,0);
        if (rlen<=0)
          break;
        pos+=rlen;
      }
      if(pos<plen)
      {
        g_free(response);
        response = NULL;
      }
      else
        response[plen]='\0';
    }
  }
  return response;
}

extern gchar *sockname;

int sway_ipc_open (int to)
{
  const gchar *socket_path;
  gint sock;
  struct sockaddr_un addr;
  struct timeval timeout = {.tv_sec = to/1000, .tv_usec = to%1000};

  if(sockname!=NULL)
    socket_path=sockname;
  else
    socket_path = g_getenv("SWAYSOCK");
  if (socket_path == NULL)
    return -1;
  sock = socket(AF_UNIX,SOCK_STREAM,0);
  if (sock == -1)
    return -1;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
  if(connect(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr_un)) != -1 )
    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != -1)
      return sock;
  close(sock);
  return -1;
}

int sway_ipc_send ( gint sock, gint32 type, gchar *command )
{
  gint8 sway_ipc_header[14];
  gint32 plen = strlen(command);
  memcpy(sway_ipc_header,magic,sizeof(magic));
  memcpy(sway_ipc_header+sizeof(magic),&plen,sizeof(plen));
  memcpy(sway_ipc_header+sizeof(magic)+sizeof(plen),&type,sizeof(type));
  if( write(sock,sway_ipc_header,sizeof(sway_ipc_header))==-1 )
    return -1;
  if(plen>0)
    if( write(sock,command,plen)==-1 )
      return -1;
  return 0;
}

void sway_ipc_command ( gchar *cmd, ... )
{
  va_list args;
  gchar *buf;
  va_start(args,cmd);
  buf = g_strdup_vprintf(cmd,args);
  sway_ipc_send ( main_ipc, 0, buf);
  g_free(buf);
  va_end(args);
}
int sway_ipc_subscribe ( gint sock )
{
  if ( sway_ipc_send(sock, 2, "['workspace','window','barconfig_update']") == -1)
    return -1;

  return sock;
}

void sway_window_new ( struct json_object *container )
{
  struct wt_window *win;
  gint64 wid;

  wid = json_int_by_name(container,"id",G_MININT64); 
  win = wintree_from_id(GINT_TO_POINTER(wid));
  if(win)
    return;
  
  win = wintree_window_init();
  if(win==NULL)
    return;

  win->wid = wid;
  win->uid = GINT_TO_POINTER(wid);
  win->pid = json_int_by_name(container,"pid",G_MININT64); 

  place_window(win->wid, win->pid );

  win->appid = json_string_by_name(container,"app_id");
  if(win->appid==NULL)
  {
    g_free(win);
    return;
  }

  win->title = json_string_by_name(container,"name");
  if (win->title == NULL )
    win->title = g_strdup(win->appid);
  wintree_set_active(win->title);

  if(json_bool_by_name(container,"focused",FALSE) == TRUE)
    wintree_set_focus(win->uid);

  wintree_window_append(win);
}

void sway_window_title ( struct json_object *container )
{
  struct wt_window *win;
  gchar *title;
  gint64 wid;

  wid = json_int_by_name(container,"id",G_MININT64);
  title = json_string_by_name(container,"name");

  if(title==NULL)
    return;

  win = wintree_from_id(GINT_TO_POINTER(wid));

  if(win)
  {
    str_assign(&(win->title),title);
    taskbar_set_label(win,title);
    switcher_set_label(win,title);
    wintree_set_active(title);
  }

  g_free(title);
}

void sway_window_close (struct json_object *container)
{
  wintree_window_delete(GINT_TO_POINTER(
        json_int_by_name(container,"id",G_MININT64)));
}

void sway_set_focus ( struct json_object *container)
{
  struct wt_window *win;
  gint64 wid;

  wid = json_int_by_name(container,"id",G_MININT64);
  wintree_set_focus(GINT_TO_POINTER(wid));
  win = wintree_from_id(GINT_TO_POINTER(wid));
  if(win)
    wintree_set_active(win->title);
}

gboolean sway_ipc_event ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  struct json_object *obj,*container;
  gchar *response,*change;
  gint32 etype;

  if(main_ipc==-1)
    return FALSE;

  response = sway_ipc_poll(main_ipc,&etype);
  while (response != NULL)
  { 
    obj = json_tokener_parse(response);

    if(etype==0x80000000)
      pager_update();

    if(etype==0x80000004)
    {
      window_hide_event(obj);
      switcher_event(obj);
    }

    if(etype==0x80000003)
    {
      if(obj!=NULL)
      {
        change = json_string_by_name(obj,"change");
        if(change!=NULL)
        {
          json_object_object_get_ex(obj,"container",&container);
          if(g_strcmp0(change,"new")==0)
            sway_window_new (container);
          if(g_strcmp0(change,"close")==0)
            sway_window_close(container);
          if(g_strcmp0(change,"title")==0)
            sway_window_title(container);
          if(g_strcmp0(change,"focus")==0)
            sway_set_focus(container);
          taskbar_invalidate();
          switcher_invalidate();
        }
      }
    }

    json_object_put(obj);
    g_free(response);
    response = sway_ipc_poll(main_ipc,&etype);
  }
  return TRUE;
}

void sway_traverse_tree ( struct json_object *obj)
{
  struct json_object *iter,*arr;
  gint i;

  json_object_object_get_ex(obj,"floating_nodes",&arr);
  if( arr && json_object_is_type(arr, json_type_array) )
    for(i=0;i<json_object_array_length(arr);i++)
    {
      iter = json_object_array_get_idx(arr,i);
      sway_window_new (iter);
    }

  json_object_object_get_ex(obj,"nodes",&arr);
  if( arr && json_object_is_type(arr, json_type_array) )
    for(i=0;i<json_object_array_length(arr);i++)
    {
      iter = json_object_array_get_idx(arr,i);
      if( json_int_by_name(iter,"id",G_MININT64) == G_MININT64 )
        sway_window_new (iter);
      else
        sway_traverse_tree(iter);
    }
}

void sway_ipc_init ( void )
{
  struct json_object *obj;
  gchar *response;
  gint sock;
  gint32 etype;

  sock=sway_ipc_open(3000);
  if(sock==-1)
    return;
  sway_ipc_send(sock,0,"bar hidden_state show");
  response = sway_ipc_poll(sock,&etype);
  g_free(response);
  sway_ipc_send(sock,4,"");
  response = sway_ipc_poll(sock,&etype);
  close(sock);
  if(response==NULL)
    return;
  obj = json_tokener_parse(response);
  if(obj!=NULL)
  {
    sway_traverse_tree(obj);
    json_object_put(obj);
  }
  g_free(response);

  taskbar_invalidate();

  main_ipc = sway_ipc_open(10);
  if(main_ipc<0)
    return;

  sway_ipc_subscribe(main_ipc);
  GIOChannel *chan = g_io_channel_unix_new(main_ipc);
  g_io_add_watch(chan,G_IO_IN,sway_ipc_event,NULL);
}

