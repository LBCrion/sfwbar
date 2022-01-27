/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include "sfwbar.h"

static gchar *mpd_socket;

gboolean mpd_ipc_event ( GIOChannel *chan, GIOCondition cond, gpointer file )
{
  static gboolean r;

  if ( cond == G_IO_ERR || cond == G_IO_HUP )
  {
    g_io_channel_shutdown(chan,FALSE,NULL);
    g_io_channel_unref(chan);
    return FALSE;
  }

  scanner_update_file( chan, file );

  if(!r)
    g_io_channel_write_chars(chan,"status\ncurrentsong\n",-1,NULL,NULL);
  else
    g_io_channel_write_chars(chan,"idle player\n",-1,NULL,NULL);
  g_io_channel_flush(chan,NULL);
  r= !r;
  return TRUE;
}

GSocketConnection *mpd_ipc_connect_unix ( GSocketClient *client, gchar *path )
{
  GSocketAddress *addr;
  GSocketConnection *scon;

  addr = g_unix_socket_address_new(path);
  scon = g_socket_client_connect(client,(GSocketConnectable *)addr,NULL,NULL);

  g_object_unref(addr);

  return scon;
}

GSocketConnection *mpd_ipc_connect ( GSocketClient *client, gchar *path )
{
  GSocketConnection *scon = NULL;
  gchar *host, *port, *addr;
  const gchar *dir;

  if(path)
    scon = g_socket_client_connect_to_host(client,path,0,NULL,NULL);
  if( scon )
    return scon;

  if(path)
    scon = mpd_ipc_connect_unix(client, path);
  if( scon )
    return scon;

  host = g_strdup(g_getenv("MPD_HOST"));
  if(!host)
    host = g_strdup("localhost");
  port = g_strdup(g_getenv("MPD_PORT"));
  if(!port)
    port = g_strdup("6600");
  addr = g_strconcat( host, ":", port, NULL );
  g_free(host);
  g_free(port);
  scon = g_socket_client_connect_to_host(client,addr,0,NULL,NULL);
  g_free(addr);
  if(scon)
    return scon;

  dir = g_get_user_runtime_dir();
  if(!dir)
    dir = "/run";
  addr = g_build_filename(dir,"/mpd/socket",NULL);
  scon = mpd_ipc_connect_unix(client, addr);
  g_free(addr);

  return scon;
}

GIOChannel *mpd_ipc_open ( gchar *user )
{
  GSocketConnection*scon;
  GSocket *sock;
  GSocketClient *client;

  client = g_socket_client_new();
  scon = mpd_ipc_connect ( client, user );

  if(!scon)
    return NULL;

  if(g_socket_connection_is_connected(scon))
    sock = g_socket_connection_get_socket(scon);
  else
    sock = NULL;

  if(!sock)
    return NULL;

  return g_io_channel_unix_new(g_socket_get_fd(sock));
}

void mpd_ipc_init ( struct scan_file *file )
{
  static GIOChannel *chan;

  if(!mpd_socket)
    mpd_socket = g_strdup(file->fname);

  chan = mpd_ipc_open ( file->fname );
  g_io_add_watch(chan,G_IO_IN | G_IO_ERR | G_IO_HUP,mpd_ipc_event,file);
}


gboolean mpd_ipc_reset ( GIOChannel *chan, GIOCondition cond, gpointer *ptr  )
{
  if ( cond != G_IO_IN )
  {
    g_io_channel_shutdown(chan,FALSE,NULL);
    g_io_channel_unref(chan);
    *ptr = NULL;
    return FALSE;
  }
  return TRUE;
}

void mpd_ipc_command ( gchar *command )
{
  static GIOChannel *chan;
  gchar *str;

  if(!chan)
  {
    chan = mpd_ipc_open(mpd_socket);
    g_io_add_watch(chan,0,(GIOFunc)mpd_ipc_reset,&chan);
  }

  if(!chan)
    return;

  str = g_strconcat( "noidle\n", command, "\nidle\n", NULL );

  g_io_channel_write_chars ( chan, str, -1, NULL, NULL );
  g_io_channel_flush( chan, NULL );

  g_free(str);
}
