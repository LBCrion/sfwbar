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
#include "action.h"

void client_disconnect ( Client *client )
{
    if(client->in == client->out)
      client->out = NULL;
    if(client->in)
    {
      g_io_channel_shutdown(client->in,FALSE,NULL);
      g_io_channel_unref(client->in);
      client->in = NULL;
    }
    if(client->out)
    {
      g_io_channel_shutdown(client->out,FALSE,NULL);
      g_io_channel_unref(client->out);
      client->out = NULL;
    }
    if(client->scon)
    {
      g_object_unref(client->scon);
      client->scon = NULL;
    }
}

void client_reconnect ( Client *client )
{
  client_disconnect(client);
  if(client->connect)
    g_timeout_add (1000,(GSourceFunc )client->connect,client);
}

gboolean client_event ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  Client *client = data;
  gsize size;
  GIOStatus cstat;

  g_debug("client event: %s, fd %d, flags %d, cond %d",client->file->fname,
      g_io_channel_unix_get_fd(chan),g_io_channel_get_flags(chan),cond);

  if( cond & G_IO_ERR || cond & G_IO_HUP )
  {
    g_debug("client error: %s, cond = %d",client->file->fname,
        cond);
    client_reconnect(client);
    return FALSE;
  }

  if( cond & G_IO_IN || cond & G_IO_PRI )
  {
    g_list_foreach(client->file->vars,(GFunc)scanner_var_reset,NULL);
    cstat = scanner_file_update( chan, client->file, &size );
    g_debug("client: %s, read %ld bytes, status %d",
      client->file->fname?client->file->fname:"(null)",size,cstat);
    if(cstat == G_IO_STATUS_ERROR || !size )
    {
      g_debug("client read error: %s, status = %d, size = %ld",
          client->file->fname,cstat,size);
      client_reconnect(client);
      return FALSE;
    }
  }
  if(client->respond)
  {
    cstat = client->respond(client);
    if(cstat != G_IO_STATUS_NORMAL)
    {
      g_debug("client write error: %s, status = %d",
          client->file->fname,cstat);
      client_reconnect(client);
      return FALSE;
    }
  }
  base_widget_emit_trigger(client->file->trigger);
  return TRUE;
}

void client_attach ( Client *client )
{
  scanner_file_attach(client->file->trigger,client->file);
  client->connect(client);
}

void client_subscribe ( Client *client )
{
  if(client->in)
    g_io_channel_set_flags(client->in,G_IO_FLAG_NONBLOCK,NULL);
  if(client->out)
  {
    g_io_channel_set_flags(client->out,G_IO_FLAG_NONBLOCK,NULL);
    g_io_add_watch(client->out, G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR,
        client_event, client);
    g_debug("client new: %p, flags: %d, source: %s", client->out,
        g_io_channel_get_flags(client->out),client->file->fname);
  }
}

GSocketConnection *client_socket_connect_path ( gchar *path )
{
  GSocketClient *sclient;
  GSocketConnection *scon;
  GSocketAddress *addr;

  sclient = g_socket_client_new();
  if(strchr(path,':'))
    scon = g_socket_client_connect_to_host(sclient,path,0,NULL,NULL);
  else
  {
    addr = g_unix_socket_address_new(path);
    scon = g_socket_client_connect(sclient,(GSocketConnectable *)addr,
        NULL,NULL);
    g_object_unref(addr);
  }

  if(!scon)
  {
    g_object_unref(sclient);
    return NULL;
  }

  g_object_weak_ref(G_OBJECT(scon),(GWeakNotify)g_object_unref,sclient);
  if(!g_socket_connection_is_connected(scon))
  {
    g_object_unref(scon);
    return NULL;
  }

  return scon;
}

gboolean client_socket_connect ( Client *client )
{
  GSocket *sock;

  client->scon = client_socket_connect_path(client->file->fname);
  if(!client->scon)
    return TRUE;

  sock = g_socket_connection_get_socket(client->scon);
  g_socket_set_keepalive(sock,TRUE);

  if(!sock)
  {
    g_debug("client: %s: socket connection failed",client->file->fname);
    g_object_unref(client->scon);
    client->scon = NULL;
    return TRUE;
  }

  client->out = g_io_channel_unix_new(g_socket_get_fd(sock));
  if(!client->out)
  {
    g_debug("client %s: unable to create channel",client->file->fname);
    g_object_unref(client->scon);
    client->scon = NULL;
    return TRUE;
  }
  g_debug("client connected: %s, channel: %p, fd: %d, flags: %d, conn: %p",
      client->file->fname,client->out,g_io_channel_unix_get_fd(client->out),
      g_io_channel_get_flags(client->out),client->scon);

  client->in = client->out;
  client_subscribe(client);

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

  client->in = g_io_channel_unix_new(in);
  client->out = g_io_channel_unix_new(out);

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
}

void client_send ( action_t *action )
{
  ScanFile *file;

  if(!action->addr->cache || !action->command->cache )
    return;

  file = scanner_file_get ( action->addr->cache );
  if(!file)
    return;

  if(file->client && ((Client *)(file->client))->out)
  {
    (void)g_io_channel_write_chars(((Client *)(file->client))->out,
        action->command->cache,-1,NULL,NULL);
    g_io_channel_flush(((Client *)(file->client))->out,NULL);
  }
}
