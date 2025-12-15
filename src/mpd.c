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

Client *mpd_client;

#define MPD_STATE(x) ((MpdState *)(x))

static gboolean client_mpd_connect ( Client *client )
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
        if(host && *host=='/')
          str_assign(&fname, g_strdup(host));
        else
          str_assign(&fname, g_strconcat(host? host : "localhost", ":",
                port? port : "6600", NULL));
      }
    }
  }
  str_assign(&client->src->fname, fname);

  return client_socket_connect(client);
}

static GIOStatus client_mpd_respond ( Client *client )
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

static value_t mpd_action_cmd ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MpdCmd");
  vm_param_check_string(vm, p, 0, "MpdCmd");

  if(!value_get_string(p[0]) || !mpd_client || !mpd_client->out)
    return value_na;

  g_queue_push_tail(MPD_STATE(mpd_client->data)->commands,
      g_strconcat(value_get_string(p[0]), "\n", NULL));
  (void)g_io_channel_write_chars(mpd_client->out, "noidle\n", -1, NULL, NULL);
  g_io_channel_flush(mpd_client->out, NULL);
  MPD_STATE(mpd_client->data)->idle = FALSE;

  return value_na;
}

source_t *client_mpd ( gchar *fname )
{
  if(!mpd_client)
  {
    mpd_client = g_malloc0(sizeof(Client));
    mpd_client->data = g_malloc0(sizeof(MpdState));
    mpd_client->connect = client_mpd_connect;
    mpd_client->consume = client_source_update;
    mpd_client->respond = client_mpd_respond;
    mpd_client->trigger = g_strdup("mpd");
    mpd_client->src = scanner_source_new(fname);

    MPD_STATE(mpd_client->data)->commands = g_queue_new();
    MPD_STATE(mpd_client->data)->path = g_strdup(mpd_client->src->fname);

    client_attach(mpd_client);
    vm_func_add("mpdcmd", mpd_action_cmd, TRUE, TRUE);
  }

  return mpd_client->src;
}
