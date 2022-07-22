/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include "sfwbar.h"
#include "basewidget.h"

static GSocketConnection *mpd_ipc_sock;
static GSocketConnection *mpd_cmd_sock;

gboolean mpd_ipc_event ( GIOChannel *chan, GIOCondition cond, gpointer file )
{
  static gboolean r;
  GIOStatus s;

  if(file)
    scanner_reset_vars(((ScanFile *)file)->vars);

  if ( cond & G_IO_ERR || cond & G_IO_HUP )
  {
    g_io_channel_shutdown(chan,FALSE,NULL);
    g_io_channel_unref(chan);
    if(mpd_ipc_sock)
    {
      g_object_unref(mpd_ipc_sock);
      mpd_ipc_sock = NULL;
      r = 0;
    }
    return FALSE;
  }

  if( cond & G_IO_IN )
  {
    if( file )
    {
      scanner_update_file( chan, file );
      base_widget_emit_trigger("mpd");
    }

    if(!r)
      s = g_io_channel_write_chars(chan,"status\ncurrentsong\n",-1,NULL,NULL);
    else
      s = g_io_channel_write_chars(chan,"idle player\n",-1,NULL,NULL);
    g_io_channel_flush(chan,NULL);
    if(s != G_IO_STATUS_NORMAL)
      g_debug("mpd: failed to write to mpd socket");
    r= !r;
  }
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
  g_debug("mpd: attempting to connect to: %s",addr);
  scon = g_socket_client_connect_to_host(client,addr,0,NULL,NULL);
  g_free(addr);
  if(scon)
    return scon;

  dir = g_get_user_runtime_dir();
  if(!dir)
    dir = "/run";
  addr = g_build_filename(dir,"/mpd/socket",NULL);
  g_debug("mpd: attempting to connect to: %s",addr);
  scon = mpd_ipc_connect_unix(client, addr);
  g_free(addr);

  if(!scon)
    g_debug("mpd: failed to connect to server");

  return scon;
}

GIOChannel *mpd_ipc_open ( gchar *user, gpointer *addr )
{
  GSocketConnection *scon;
  GSocket *sock;
  GSocketClient *client;

  client = g_socket_client_new();

  scon = mpd_ipc_connect ( client, user );

  if(!scon)
  {
    g_object_unref(client);
    return NULL;
  }

  g_object_weak_ref(G_OBJECT(scon),(GWeakNotify)g_object_unref,client);
  sock = g_socket_connection_get_socket(scon);

  if(!sock || !g_socket_connection_is_connected(scon))
  {
    g_object_unref(scon);
    return NULL;
  }

  *addr = scon;
  return g_io_channel_unix_new(g_socket_get_fd(sock));
}

gboolean mpd_ipc_reconnect ( gpointer file )
{
  if(mpd_ipc_sock)
    return TRUE;

  mpd_ipc_init(file);
  return FALSE;
}

void mpd_ipc_init ( ScanFile *file )
{
  static GIOChannel *chan;

  chan = mpd_ipc_open ( file->fname, (gpointer *) &mpd_ipc_sock );
  if(chan)
    g_io_add_watch(chan,G_IO_IN | G_IO_ERR | G_IO_HUP,mpd_ipc_event,file);

  g_timeout_add (1000,(GSourceFunc )mpd_ipc_reconnect,file);
}

gboolean mpd_ipc_command_cb ( GIOChannel *chan, GIOCondition cond, gpointer command )
{
  gchar *str;

  if(g_io_channel_read_to_end(chan, &str, NULL, NULL) != G_IO_STATUS_NORMAL )
    g_debug("mpd: Failed to read from socket");
  g_free(str);
  str = g_strconcat( command, "\n", NULL );
  if( g_io_channel_write_chars ( chan, str, -1, NULL, NULL ) != 
      G_IO_STATUS_NORMAL )
    g_debug("mpd: Command \"%s\". Failed to write to socket",
        (gchar *)command);
  g_io_channel_flush( chan, NULL );
  g_free(str);

  g_io_channel_shutdown(chan, FALSE, NULL);
  g_io_channel_unref(chan);

  g_object_unref(mpd_cmd_sock);

  return FALSE;
}

void mpd_ipc_command ( gchar *command )
{
  GIOChannel *chan;

  if(!command)
    return;

  chan = mpd_ipc_open ( NULL, (gpointer *) &mpd_cmd_sock );

  if(!chan)
    return;

  g_io_add_watch(chan,G_IO_IN | G_IO_ERR | G_IO_HUP,
      mpd_ipc_command_cb,command);
}
