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

typedef struct _MpdState {
  gchar *path;
  gboolean idle;
  GQueue *commands;
} MpdState;

#define MPD_STATE(x) ((MpdState *)(x))

gboolean client_mpd_connect ( Client *client )
{
  gchar *host, *port;
  const gchar *dir;

  g_free(client->file->fname);
  if(MPD_STATE(client->data)->path && *(MPD_STATE(client->data)->path) )
    client->file->fname = g_strdup(MPD_STATE(client->data)->path);
  else
  {
    dir = g_get_user_runtime_dir();
    client->file->fname = g_build_filename(dir?dir:"/run","/mpd/socket",NULL);
    if( !g_file_test(client->file->fname, G_FILE_TEST_EXISTS) &&
        g_strcmp0(client->file->fname,"/run/mp/socket") )
    {
      g_free(client->file->fname);
      client->file->fname = g_strdup("/run/mpd/socket");
    }
    if( !g_file_test(client->file->fname, G_FILE_TEST_EXISTS) )
    {
      host = g_strdup(g_getenv("MPD_HOST"));
      port = g_strdup(g_getenv("MPD_PORT"));
      client->file->fname = g_strconcat(host?host:"localhost",":",
          port?port:"6600",NULL);
    }
  }

  return client_socket_connect(client);
}

GIOStatus client_mpd_respond ( Client *client )
{
  GIOStatus s;
  gchar *str;

  if(!client || !client->out || !client->data)
    return G_IO_ERROR;

  if(!g_queue_is_empty(MPD_STATE(client->data)->commands))
  {
    str = g_queue_pop_head(MPD_STATE(client->data)->commands);
    s = g_io_channel_write_chars(client->out,str,-1,NULL,NULL);
    g_free(str);
  }
  else
  {
    MPD_STATE(client->data)->idle = !MPD_STATE(client->data)->idle;
    s = g_io_channel_write_chars(client->out,
        MPD_STATE(client->data)->idle?
        "status\ncurrentsong\n":"idle player options\n",
        -1,NULL,NULL);
  }
  g_io_channel_flush(client->out,NULL);

  return s;
}


void client_mpd ( ScanFile *file )
{
  Client *client;

  if( !file || !file->fname )
    return;

  client = g_malloc0(sizeof(Client));
  client->file = file;
  client->data = g_malloc0(sizeof(MpdState));
  client->connect = client_mpd_connect;
  client->respond = client_mpd_respond;
  MPD_STATE(client->data)->commands = g_queue_new();
  MPD_STATE(client->data)->path = g_strdup(file->fname);

  file->trigger = g_intern_static_string("mpd");
  file->source = SO_CLIENT;
  file->client = client;

  client_attach(client);
}

void client_mpd_command ( gchar *command )
{
  ScanFile *file;
  Client *client;

  if(!command)
    return;

  file = scanner_file_get("mpd");
  if(!file)
    return;

  client = file->client;
  if(!client || !client->out || !client->data)
    return;

  g_queue_push_tail(MPD_STATE(client->data)->commands,
      g_strconcat(command,"\n",NULL));
  (void)g_io_channel_write_chars(client->out,"noidle\n",-1,NULL,NULL);
  g_io_channel_flush(client->out,NULL);
  MPD_STATE(client->data)->idle = FALSE;
}
