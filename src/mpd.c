/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include "trigger.h"
#include "client.h"
#include "util/file.h"
#include "util/string.h"

typedef struct _MpdState {
  gchar *path;
  gboolean idle;
  GQueue *commands;
} MpdState;

ScanFile *mpd_file;

#define MPD_STATE(x) ((MpdState *)(x))

gboolean client_mpd_connect ( Client *client )
{
  const gchar *dir, *host, *port;
  gchar *fname;

  if(MPD_STATE(client->data)->path && *(MPD_STATE(client->data)->path) )
    fname = g_strdup(MPD_STATE(client->data)->path);
  else
  {
    dir = g_get_user_runtime_dir();
    fname = dir? g_build_filename(dir, "/mpd/socket", NULL) : NULL;
    if( !file_test_read(fname) )
    {
      str_assign(&fname, g_strdup("/run/mpd/socket"));
      if( !file_test_read(fname) )
      {
        host = g_getenv("MPD_HOST");
        port = g_getenv("MPD_PORT");
        str_assign(&fname, g_strconcat(host? host : "localhost", ":",
              port? port : "6600", NULL));
      }
    }
  }
  str_assign(&client->file->fname, fname);

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
    s = g_io_channel_write_chars(client->out, str, -1, NULL, NULL);
    g_free(str);
  }
  else
  {
    MPD_STATE(client->data)->idle = !MPD_STATE(client->data)->idle;
    s = g_io_channel_write_chars(client->out,
        MPD_STATE(client->data)->idle?
        "status\ncurrentsong\n" : "idle player options\n",
        -1, NULL, NULL);
  }
  g_io_channel_flush(client->out, NULL);

  return s;
}

void client_mpd ( ScanFile *file )
{
  Client *client;

  if( !file || !file->fname )
    return;

  if(mpd_file)
  {
    scanner_file_attach(mpd_file->trigger, mpd_file);
    scanner_file_merge(mpd_file, file);
    return;
  }

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
  mpd_file = file;

  client_attach(client);
}

void client_mpd_command ( gchar *command )
{
  Client *client;

  if(!command)
    return;

  if(!mpd_file)
    return;

  client = mpd_file->client;
  if(!client || !client->out || !client->data)
    return;

  g_queue_push_tail(MPD_STATE(client->data)->commands,
      g_strconcat(command, "\n", NULL));
  (void)g_io_channel_write_chars(client->out, "noidle\n", -1, NULL, NULL);
  g_io_channel_flush(client->out, NULL);
  MPD_STATE(client->data)->idle = FALSE;
}
