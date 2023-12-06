/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include "sfwbar.h"
#include "basewidget.h"
#include "client.h"

void client_reconnect ( Client *client )
{
  g_debug("client %s: disconnecting",client->file->fname);
  if(client->in == client->out)
    client->out = NULL;
  g_clear_pointer(&client->in,g_io_channel_unref);
  g_clear_pointer(&client->out,g_io_channel_unref);
  g_clear_pointer(&client->scon,g_object_unref);
  g_clear_pointer(&client->sclient,g_object_unref);
  g_clear_pointer(&client->addr,g_object_unref);
  if(client->connect)
    g_timeout_add(1000, (GSourceFunc )client->connect, client);
}

gboolean client_event ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  Client *client = data;
  gsize size;
  GIOStatus cstat;

  g_debug("client %s: event fd %d, flags %d, cond %d",client->file->fname,
      g_io_channel_unix_get_fd(chan),g_io_channel_get_flags(chan),cond);

  if( cond & G_IO_ERR || cond & G_IO_HUP )
  {
    g_debug("client %s: error cond = %d",client->file->fname,cond);
    return FALSE;
  }

  if( cond & G_IO_IN || cond & G_IO_PRI )
  {
    if(client->consume)
      cstat = client->consume(client, &size);
    else
    {
      g_list_foreach(client->file->vars,(GFunc)scanner_var_reset,NULL);
      cstat = scanner_file_update( chan, client->file, &size );
    }
    if(cstat == G_IO_STATUS_ERROR || !size )
    {
      g_debug("client %s: read error, status = %d, size = %ld",
          client->file->fname,cstat,size);
      return FALSE;
    }
    else
      g_debug("client %s: status %d, read %ld bytes",
          client->file->fname,cstat,size);
  }
  if(client->respond)
  {
    cstat = client->respond(client);
    if(cstat != G_IO_STATUS_NORMAL)
    {
      g_debug("client %s: write error, status = %d",client->file->fname,cstat);
      client_reconnect(client);
      return FALSE;
    }
  }
  base_widget_emit_trigger(client->file->trigger);
  return TRUE;
}

void client_attach ( Client *client )
{
  scanner_file_attach(client->file->trigger, client->file);
  client->connect(client);
}

void client_subscribe ( Client *client )
{
  if(client->out && client->out != client->in)
  {
    g_io_channel_set_flags(client->out,G_IO_FLAG_NONBLOCK,NULL);
    g_io_channel_set_close_on_unref(client->out,TRUE);
  }
  if(client->in)
  {
    g_io_channel_set_flags(client->in,G_IO_FLAG_NONBLOCK,NULL);
    g_io_channel_set_close_on_unref(client->in,TRUE);
    g_io_add_watch_full(client->in,G_PRIORITY_DEFAULT,
        G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR,
        client_event, client, (GDestroyNotify)client_reconnect);
    g_debug("client %s: connected, channel: %p, fd: %d, flags: %d, conn: %p",
        client->file->fname,client->out,g_io_channel_unix_get_fd(client->out),
        g_io_channel_get_flags(client->out),client->scon);
  }
}

void client_socket_connect_cb ( GSocketClient *src, GAsyncResult *res,
    Client *client )
{
  GSocket *sock;

  client->scon = g_socket_client_connect_finish(src,res,NULL);
  if(client->scon && g_socket_connection_is_connected(client->scon))
  {
    sock = g_socket_connection_get_socket(client->scon);
    if(sock)
    {
      g_socket_set_keepalive(sock,TRUE);
      client->out = g_io_channel_unix_new(g_socket_get_fd(sock));
      if(client->out)
      {
        client->in = client->out;
        client_subscribe(client);
        return;
      }
    }
  }

  g_debug("client: %s: socket connection failed",client->file->fname);
  client_reconnect(client);
}

gboolean client_socket_connect ( Client *client )
{
  g_debug("client %s: connecting",client->file->fname);
  if(strchr(client->file->fname,':'))
    client->addr = g_network_address_parse(client->file->fname,0,NULL);
  else
    client->addr = (GSocketConnectable*)
      g_unix_socket_address_new(client->file->fname);
  if(!client->addr)
  {
    g_debug("client %s: unable to parse address",client->file->fname);
    client_reconnect(client);
    return FALSE;
  }

  client->sclient = g_socket_client_new();
  g_socket_client_connect_async(client->sclient,client->addr,NULL,
      (GAsyncReadyCallback)client_socket_connect_cb,client);

  return FALSE;
}

void client_socket ( ScanFile *file )
{
  Client *client;

  if( !file || !file->fname )
    return;

  client = g_malloc0(sizeof(Client));
  client->file = file;
  client->connect = client_socket_connect;
  file->client = client;
  client_attach(client);
}

gboolean client_exec_connect ( Client *client )
{
  gint in,out,err;
  gchar **argv;
  gint argc;

  if(!g_shell_parse_argv(client->file->fname,&argc,&argv,NULL))
    return FALSE;

  if(!g_spawn_async_with_pipes(NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,NULL,
      &in, &out, &err, NULL))
  {
    g_debug("client exec error on: %s",client->file->fname);
    g_strfreev(argv);
    return FALSE;
  }
  g_strfreev(argv);

  client->in = g_io_channel_unix_new(out);
  client->out = g_io_channel_unix_new(in);

  client_subscribe(client);
  return FALSE;
}

void client_exec ( ScanFile *file )
{
  Client *client;

  if( !file || !file->fname )
    return;

  client = g_malloc0(sizeof(Client));
  client->file = file;
  client->connect = client_exec_connect;
  client_attach(client);
  file->client = client;
}

void client_send ( gchar *addr, gchar *command )
{
  ScanFile *file;
  Client *client;

  if(!addr || !command )
    return;

  file = scanner_file_get ( addr );
  if(!file)
    return;

  client = file->client;
  if(!client || !client->out)
    return;
  (void)g_io_channel_write_chars(client->out, command, -1, NULL, NULL);
  g_io_channel_flush(client->out, NULL);
}
