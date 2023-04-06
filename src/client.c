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

void client_disconnect ( Client *client )
{
    if(client->scon)
      g_object_unref(client->scon);
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
    if(client->err)
    {
      g_io_channel_shutdown(client->err,FALSE,NULL);
      g_io_channel_unref(client->err);
      client->err = NULL;
    }
}

void client_reconnect ( Client *client )
{
  client_disconnect(client);
  g_usleep(1000000);
  if(client->connect)
    client->connect(client);
}

gboolean client_event ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  Client *client = data;
  gsize size;
  GIOStatus cstat;

  g_debug("channel event: %s, fd %d, flags %d, cond %d",client->file->fname,
      g_io_channel_unix_get_fd(chan),g_io_channel_get_flags(chan),cond);
  if(client->scon)
    cstat = g_io_channel_write_chars(chan,"\n",1,NULL,NULL);
  else
    cstat = G_IO_STATUS_NORMAL;

  if( cond & G_IO_ERR || cond & G_IO_HUP || cstat != G_IO_STATUS_NORMAL )
  {
    g_debug("channel write error: %s, cond = %d, status = %d",client->file->fname,
        cond,cstat);
    client_reconnect(client);
    return FALSE;
  }

  g_list_foreach(client->file->vars,(GFunc)scanner_var_reset,NULL);
  cstat = scanner_file_update( chan, client->file, &size );
  g_debug("channel %s, read %ld bytes, status %d",
      client->file->fname?client->file->fname:"(null)",size,cstat);
  if(cstat == G_IO_STATUS_ERROR || !size )
  {
    g_debug("channel read error: %s, status = %d, size = %ld",client->file->fname,
        cstat,size);
    client_reconnect(client);
    return FALSE;
  }
  base_widget_emit_trigger(client->file->trigger);
  return TRUE;
}

void client_socket_connect ( Client *client )
{
  GSocketClient *sclient;
  GSocketAddress *addr;
  GSocket *sock;

  sclient = g_socket_client_new();
  client->scon = g_socket_client_connect_to_host(sclient,client->file->fname,
      0,NULL,NULL);

  if(!client->scon)
  {
    addr = g_unix_socket_address_new(client->file->fname);
    client->scon = g_socket_client_connect(sclient,(GSocketConnectable *)addr,
        NULL,NULL);
    g_object_unref(addr);
  }

  if(!client->scon)
  {
    g_object_unref(sclient);
    return;
  }

  g_object_weak_ref(G_OBJECT(client->scon),(GWeakNotify)g_object_unref,sclient);
  sock = g_socket_connection_get_socket(client->scon);
  g_debug("client: %s connected to %d",client->file->fname,
      g_socket_get_fd(sock));
  g_socket_set_keepalive(sock,TRUE);

  if(!sock || !g_socket_connection_is_connected(client->scon))
  {
    g_object_unref(client->scon);
    return;
  }

  client->out = g_io_channel_unix_new(g_socket_get_fd(sock));
    g_debug("channel new: %p, fd: %d, flags: %d, conn: %p, source: %s",
        client->out,g_io_channel_unix_get_fd(client->out),
        g_io_channel_get_flags(client->out),client->scon,client->file->fname);

  if(client->out)
  {
    g_io_channel_set_flags(client->out,G_IO_FLAG_NONBLOCK,NULL);
    g_io_add_watch(client->out, G_IO_IN | G_IO_HUP | G_IO_ERR, client_event,
        client);
    client->in = client->out;
  }
  else
    g_object_unref(client->scon);
}

void client_socket ( ScanFile *file )
{
  Client *client;

  if( !file || !file->fname )
    return;

  client = g_malloc0(sizeof(Client));
  client->file = file;
  client->connect = client_socket_connect;
  client_socket_connect(client);
}

void client_exec_connect ( Client *client )
{
  gint in,out,err;
  gchar **argv;
  gint argc;

  if(!g_shell_parse_argv(client->file->fname,&argc,&argv,NULL))
    return;

  if(!g_spawn_async_with_pipes(NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,NULL,
      &in, &out, &err, NULL))
  {
    g_strfreev(argv);
    g_debug("channel exec error on: %s",client->file->fname);
    return;
  }
  g_strfreev(argv);

  client->in = g_io_channel_unix_new(in);
  client->out = g_io_channel_unix_new(out);
  client->err = g_io_channel_unix_new(err);
  if(client->out)
  {
    g_io_channel_set_flags(client->in,G_IO_FLAG_NONBLOCK,NULL);
    g_io_channel_set_flags(client->out,G_IO_FLAG_NONBLOCK,NULL);
    g_io_channel_set_flags(client->err,G_IO_FLAG_NONBLOCK,NULL);
    g_io_add_watch(client->out, G_IO_IN | G_IO_HUP | G_IO_ERR, client_event,
        client);
    g_io_add_watch(client->err, G_IO_IN | G_IO_HUP | G_IO_ERR, client_event,
        client);
    g_debug("channel new: %p, fd: %d, flags: %d, source: %s", client->out,out,
        g_io_channel_get_flags(client->out),client->file->fname);
  }
}

void client_exec ( ScanFile *file )
{
  Client *client;

  if( !file || !file->fname )
    return;

  client = g_malloc0(sizeof(Client));
  client->file = file;
  client->connect = client_exec_connect;
  client_exec_connect(client);
}
