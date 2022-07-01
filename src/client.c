/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include "sfwbar.h"

gboolean client_event ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  scan_file_t *file = data;

  if(file->scon)
    g_io_channel_write_chars(chan,"\n",1,NULL,NULL);

  if( cond & G_IO_ERR || cond & G_IO_HUP )
  {
    if(file->scon)
      g_object_unref(file->scon);
    g_io_channel_shutdown(chan,FALSE,NULL);
    g_io_channel_unref(chan);
    return FALSE;
  }

  scanner_reset_vars(file->vars);
  scanner_update_file( chan, file );
  base_widget_emit_trigger(file->trigger);
  return TRUE;
}

void client_socket ( scan_file_t *file )
{
  GSocketClient *client;
  GSocketAddress *addr;
  GSocketConnection *scon;
  GSocket *sock;
  GIOChannel *chan;

  if( !file || !file->fname )
    return;

  client = g_socket_client_new();
  scon = g_socket_client_connect_to_host(client,file->fname,0,NULL,NULL);

  if(!scon)
  {
    addr = g_unix_socket_address_new(file->fname);
    scon = g_socket_client_connect(client,(GSocketConnectable *)addr,NULL,NULL);
    g_object_unref(addr);
  }

  if(!scon)
  {
    g_object_unref(client);
    return;
  }

  g_object_weak_ref(G_OBJECT(scon),(GWeakNotify)g_object_unref,client);
  sock = g_socket_connection_get_socket(scon);
  g_socket_set_keepalive(sock,TRUE);

  if(!sock || !g_socket_connection_is_connected(scon))
  {
    g_object_unref(scon);
    return;
  }

  file->scon = scon;

  chan = g_io_channel_unix_new(g_socket_get_fd(sock));

  if(chan)
  {
    g_io_channel_set_flags(chan,G_IO_FLAG_NONBLOCK,NULL);
    g_io_add_watch(chan, G_IO_IN | G_IO_HUP | G_IO_ERR, client_event, file);
    file->out = chan;
  }
  else
    g_object_unref(scon);
}

void client_exec ( scan_file_t *file )
{
  gint in, out, err;
  gchar **argv;
  gint argc;
  GIOChannel *chan;

  if(!g_shell_parse_argv(file->fname,&argc,&argv,NULL))
    return;

  if(!g_spawn_async_with_pipes(NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,NULL,
      &in, &out, &err, NULL))
  {
    g_strfreev(argv);
    return;
  }
  g_strfreev(argv);

  chan = g_io_channel_unix_new(out);
  if(chan)
  {
    g_io_channel_set_flags(chan,G_IO_FLAG_NONBLOCK,NULL);
    g_io_add_watch(chan, G_IO_IN | G_IO_HUP | G_IO_ERR, client_event, file);
    g_io_add_watch(g_io_channel_unix_new(err), G_IO_IN, client_event, file);
    file->out = g_io_channel_unix_new(in);
    g_io_channel_set_flags(file->out,G_IO_FLAG_NONBLOCK,NULL);
  }
}
