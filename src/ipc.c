/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json.h>
#include "sfwbar.h"

const  gint8 magic[6] = {0x69, 0x33, 0x2d, 0x69, 0x70, 0x63};

json_object *ipc_poll ( int sock, gint32 *etype )
{
  gint8 ipc_header[14];
  gchar *response = NULL;
  gint32 plen;
  size_t pos=0;
  ssize_t rlen;

  while(pos<sizeof(ipc_header))
  {
    rlen = recv(sock,(gchar *)ipc_header,sizeof(ipc_header)-pos,0);
    if (rlen<=0)
      break;
    pos+=rlen;
  }

  if(pos==sizeof(ipc_header))
  {
    pos=0;
    memcpy(etype,ipc_header+sizeof(magic)+sizeof(plen),sizeof(plen));
    memcpy(&plen,ipc_header+sizeof(magic),sizeof(plen));
    response = g_malloc(plen+1);
    if ( response != NULL)
    {
      while(pos<plen)
      {
        rlen = recv(sock,(gchar *)response,plen-pos,0);
        if (rlen<=0)
          break;
        pos+=rlen;
      }
      if(pos<plen)
      {
        g_free(response);
        response = NULL;
      }
      response[plen]='\0';
    }
  }
  if(response==NULL)
    return NULL;
  return json_tokener_parse(response);
}

struct ipc_event ipc_parse_event ( json_object *obj )
{
  struct ipc_event ev;
  const char *changes[] = {"new","close","focus","title","fullscreen_mode","move","floating","urgent","mark"};
  json_object *container;
  char *change;
  int i;

  ev.event = -1;
  ev.appid = NULL;
  ev.title = NULL;

  change = json_string_by_name(obj,"change");
  if(change==NULL)
    return ev;
  json_object_object_get_ex(obj,"container",&container);
  for(i=0;i<9;i++)
    if(!g_strcmp0(change,changes[i]))
    {
      ev.event=i;
      ev.pid = json_int_by_name(container,"pid",G_MININT64); 
      ev.appid = json_string_by_name(container,"app_id");
      ev.title = json_string_by_name(container,"name");
   }
  g_free(change);
  return ev;
}

int ipc_open ()
{
  const gchar *socket_path;
  int sock;
  struct sockaddr_un addr;
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 10};

  socket_path = g_getenv("SWAYSOCK");
  if (socket_path == NULL)
    return -1;
  sock = socket(AF_UNIX,SOCK_STREAM,0);
  if (sock == -1)
    return -1;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
  if(connect(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr_un)) == -1 )
    return -1;
  if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    return -1;
  return sock;
}

int ipc_send ( int sock, gint32 type, gchar *command )
{
  gint8 ipc_header[14];
  gint32 plen = strlen(command);
  memcpy(ipc_header,magic,sizeof(magic));
  memcpy(ipc_header+sizeof(magic),&plen,sizeof(plen));
  memcpy(ipc_header+sizeof(magic)+sizeof(plen),&type,sizeof(type));
  if( write(sock,ipc_header,sizeof(ipc_header))==-1 )
    return -1;
  if(plen>0)
    if( write(sock,command,plen)==-1 )
      return -1;
  return 0;
}

int ipc_subscribe ( int sock )
{
  if ( ipc_send(sock, 2, "['window']") == -1)
    return -1;

  return sock;
}

