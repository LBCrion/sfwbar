/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include "trigger.h"
#include "client.h"
#include "util/hash.h"

static hash_table_t *client_list;

GIOStatus client_source_update ( Client *client, gsize *size )
{
  return scanner_source_update(client->in, client->src, size);
}

static void client_reconnect ( Client *client )
{
  g_debug("client %s: disconnecting", client->src->fname);

  if(client->in == client->out)
    client->out = NULL;
  g_clear_pointer(&client->in, g_io_channel_unref);
  g_clear_pointer(&client->out, g_io_channel_unref);
  g_clear_pointer(&client->scon, g_object_unref);
  g_clear_pointer(&client->sclient, g_object_unref);
  g_clear_pointer(&client->addr, g_object_unref);
  if(client->connect)
    g_timeout_add(1000, (GSourceFunc )client->connect, client);
}

static gboolean client_event ( GIOChannel *chan, GIOCondition cond, Client *client )
{
  GIOStatus cstat;
  GList *iter;
  gsize size;

  g_debug("client: %s: event fd %d, flags %d, cond %d", client->src->fname,
      g_io_channel_unix_get_fd(chan), g_io_channel_get_flags(chan), cond);

  if(cond & G_IO_ERR || cond & G_IO_HUP)
  {
    g_debug("client: %s: error cond = %d", client->src->fname, cond);
    return FALSE;
  }

  if(client->consume && (cond & G_IO_IN || cond & G_IO_PRI) )
  {
    g_rec_mutex_lock(&client->src->mutex);
    for(iter=client->src->vars; iter; iter=g_list_next(iter))
      scanner_var_invalidate(0, iter->data, NULL);
    cstat = client->consume(client, &size);
    g_rec_mutex_unlock(&client->src->mutex);
    if(cstat == G_IO_STATUS_ERROR || !size )
    {
      g_debug("client: %s: read error, status = %d, size = %zu",
          client->src->fname, cstat, size);
      return FALSE;
    }
    else
      g_debug("client %s: status %d, read %zu bytes",
          client->src->fname, cstat, size);
  }
  if(client->respond)
  {
    cstat = client->respond(client);
    if(cstat != G_IO_STATUS_NORMAL)
    {
      g_debug("client %s: write error, status = %d", client->src->fname, cstat);
      client_reconnect(client);
      return FALSE;
    }
  }
  trigger_emit((gchar *)client->trigger);
  return TRUE;
}

void client_attach ( Client *client )
{
  if(!client_list)
    client_list = hash_table_new(g_direct_hash, g_direct_equal);
  hash_table_insert(client_list, (gchar *)client->trigger, client);
  client->connect(client);
}

static void client_subscribe ( Client *client )
{
  if(client->out && client->out != client->in)
  {
    g_io_channel_set_flags(client->out, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_close_on_unref(client->out, TRUE);
  }
  if(client->in)
  {
    g_io_channel_set_flags(client->in, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_close_on_unref(client->in, TRUE);
    g_io_add_watch_full(client->in, G_PRIORITY_DEFAULT,
        G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR,
        (GIOFunc)client_event, client, (GDestroyNotify)client_reconnect);
    g_debug("client %s: connected, channel: %p, fd: %d, flags: %d, conn: %p",
        client->src->fname, (void *)client->out,
        g_io_channel_unix_get_fd(client->out),
        g_io_channel_get_flags(client->out), (void *)client->scon);
  }
}

static void client_socket_connect_cb ( GSocketClient *src, GAsyncResult *res,
    Client *client )
{
  GSocket *sock;

  client->scon = g_socket_client_connect_finish(src, res, NULL);
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

  g_debug("client: %s: socket connection failed", client->src->fname);
  client_reconnect(client);
}

gboolean client_socket_connect ( Client *client )
{
  g_debug("client %s: connecting", client->src->fname);
  if(strchr(client->src->fname,':'))
    client->addr = g_network_address_parse(client->src->fname, 0, NULL);
  else
    client->addr = (GSocketConnectable*)
      g_unix_socket_address_new(client->src->fname);
  if(!client->addr)
  {
    g_debug("client %s: unable to parse address", client->src->fname);
    client_reconnect(client);
    return FALSE;
  }

  client->sclient = g_socket_client_new();
  g_socket_client_connect_async(client->sclient,client->addr, NULL,
      (GAsyncReadyCallback)client_socket_connect_cb,client);

  return FALSE;
}

source_t *client_socket ( gchar *fname, gchar *trigger )
{
  Client *client;

  if(!fname)
    return NULL;

  client = g_malloc0(sizeof(Client));
  client->src =  scanner_source_new(fname);
  client->connect = client_socket_connect;
  client->consume = client_source_update;
  client_attach(client);

  return client->src;
}

static gboolean client_exec_connect ( Client *client )
{
  gint in, out, err;
  gchar **argv;
  gint argc;

  if(!g_shell_parse_argv(client->src->fname, &argc, &argv, NULL))
    return FALSE;

  if(!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL,
        NULL,NULL, &in, &out, &err, NULL))
  {
    g_debug("client exec error on: %s", client->src->fname);
    g_strfreev(argv);
    return FALSE;
  }
  g_strfreev(argv);

  client->in = g_io_channel_unix_new(out);
  client->out = g_io_channel_unix_new(in);

  client_subscribe(client);
  return FALSE;
}

source_t *client_exec ( gchar *fname, gchar *trigger )
{
  Client *client;

  if(!fname)
    return NULL;

  client = g_malloc0(sizeof(Client));
  client->src = scanner_source_new(fname);
  client->connect = client_exec_connect;
  client->consume = client_source_update;
  client_attach(client);

  return client->src;
}

void client_send ( gchar *addr, gchar *command )
{
  Client *client;

  if(!addr || !command )
    return;

  if( !(client = hash_table_lookup(client_list, addr)) )
    return;

  if(!client || !client->out)
    return;
  (void)g_io_channel_write_chars(client->out, command, -1, NULL, NULL);
  g_io_channel_flush(client->out, NULL);
}
