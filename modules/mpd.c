/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include <glib.h>
#include <mpd/client.h>
#include "../src/module.h"
#include "stdio.h"

static ModuleApiV1 *sfwbar_module_api;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 1;
static struct mpd_status *status;
static struct mpd_song *song;
static struct mpd_connection *conn;
static gchar *password;
static guint64 last_update;
static gboolean timer;

static gboolean mpd_connect ( gpointer data );

static gboolean mpd_timer ( gpointer data )
{
  static guint64 last, current;

  if( !status || mpd_status_get_state(status)!=MPD_STATE_PLAY )
  {
    timer = FALSE;
    return FALSE;
  }

  current = g_get_monotonic_time();
  if((current-last)/mpd_status_get_total_time(status)/10 > 1)
    MODULE_TRIGGER_EMIT("mpd-progress");

  return TRUE;
}

static gboolean mpd_update ( void )
{
  if(status)
    mpd_status_free(status);
  if(song)
    mpd_song_free(song);
  song = NULL;

  status =  mpd_run_status (conn);
  if(!mpd_response_finish(conn))
    return FALSE;

  song = mpd_run_current_song(conn);
  if(!mpd_response_finish(conn))
    return FALSE;
  last_update = g_get_monotonic_time();

  if(!timer && (!status || mpd_status_get_state(status) == MPD_STATE_PLAY))
  {
    g_timeout_add(1000,mpd_timer,NULL);
    timer = 1;
  }

  MODULE_TRIGGER_EMIT("mpd");
  return TRUE;
}

static gboolean mpd_event ( GIOChannel *chan, GIOCondition cond, void *d)
{
  g_debug("MPD client: processing an event");
  mpd_recv_idle(conn,FALSE);
  mpd_response_finish(conn);
  if( !mpd_update() )
  {
    mpd_connection_free(conn);
    conn = NULL;
    g_timeout_add (1000,(GSourceFunc )mpd_connect,NULL);
    MODULE_TRIGGER_EMIT("mpd");
    return FALSE;
  }

  mpd_send_idle_mask(conn,MPD_IDLE_PLAYER);
  return TRUE;
}

static gboolean mpd_connect ( gpointer data )
{
  GIOChannel *chan;

  conn = mpd_connection_new(NULL,0,0);
  if(conn && mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)
  {
    mpd_connection_free(conn);
    conn = NULL;
  }
  if(!conn)
    return TRUE;
  if(password)
    mpd_send_password(conn, password);
  g_debug("MPD client: connected to server");

  mpd_update();
  mpd_send_idle_mask(conn,MPD_IDLE_PLAYER);

  chan = g_io_channel_unix_new(mpd_connection_get_fd(conn));
  g_io_add_watch(chan,G_IO_IN,(GIOFunc)mpd_event,conn);
  g_io_channel_unref(chan);
 
  return FALSE;
}

void sfwbar_module_init ( ModuleApiV1 *api )
{
  sfwbar_module_api = api;

  if(mpd_connect(NULL))
    g_timeout_add (1000,(GSourceFunc )mpd_connect,NULL);
}

void *mpd_expr_func ( void **params )
{
  if(!conn | !status | !song)
    return g_strdup("disconnected");
  if(!params || !params[0])
    return g_strdup("");

  if(!g_ascii_strcasecmp(params[0],"title"))
    return g_strdup(song?mpd_song_get_tag(song,MPD_TAG_TITLE,0):"");
  else if(!g_ascii_strcasecmp(params[0],"track"))
    return g_strdup(song?mpd_song_get_tag(song,MPD_TAG_TRACK,0):"");
  else if(!g_ascii_strcasecmp(params[0],"artist"))
    return g_strdup(song?mpd_song_get_tag(song,MPD_TAG_ARTIST,0):"");
  else if(!g_ascii_strcasecmp(params[0],"album"))
    return g_strdup(song?mpd_song_get_tag(song,MPD_TAG_ALBUM,0):"");
  else if(!g_ascii_strcasecmp(params[0],"genre"))
    return g_strdup(song?mpd_song_get_tag(song,MPD_TAG_GENRE,0):"");
  else if(!g_ascii_strcasecmp(params[0],"volume"))
    return g_strdup_printf("%d",status?mpd_status_get_volume(status):0);
  else if(!g_ascii_strcasecmp(params[0],"repeat"))
    return g_strdup_printf("%d",status?mpd_status_get_repeat(status):0);
  else if(!g_ascii_strcasecmp(params[0],"random"))
    return g_strdup_printf("%d",status?mpd_status_get_random(status):0);
  else if(!g_ascii_strcasecmp(params[0],"queue_len"))
    return g_strdup_printf("%d",status?mpd_status_get_queue_length(status):0);
  else if(!g_ascii_strcasecmp(params[0],"queue_pos"))
    return g_strdup_printf("%d",status?mpd_status_get_song_pos(status):0);
  else if(!g_ascii_strcasecmp(params[0],"elapsed"))
    return g_strdup_printf("%lu",status?mpd_status_get_elapsed_ms(status) +
        (mpd_status_get_state(status)==MPD_STATE_PLAY?
         (g_get_monotonic_time()-last_update)/1000:0):0);
  else if(!g_ascii_strcasecmp(params[0],"length"))
    return g_strdup_printf("%u",status?mpd_status_get_total_time(status):0);
  else if(!g_ascii_strcasecmp(params[0],"rate"))
    return g_strdup_printf("%u",status?mpd_status_get_kbit_rate(status):0);
  else if(!g_ascii_strcasecmp(params[0],"state"))
    switch(mpd_status_get_state(status))
    {
      case MPD_STATE_PLAY:
        return g_strdup("play");
      case MPD_STATE_PAUSE:
        return g_strdup("pause");
      case MPD_STATE_STOP:
        return g_strdup("stop");
      default:
        return g_strdup("unknown");
    }

  return g_strdup("Invalid request");
}

static void mpd_command ( gchar *cmd, gchar *dummy, void *d1,
    void *d2, void *d3, void *d4 )
{
  if(!conn)
    return;
  mpd_run_noidle(conn);
  if(!g_ascii_strcasecmp(cmd,"play"))
    mpd_run_play(conn);
  else if(!g_ascii_strcasecmp(cmd,"prev"))
    mpd_run_previous(conn);
  else if(!g_ascii_strcasecmp(cmd,"next"))
    mpd_run_next(conn);
  else if(!g_ascii_strcasecmp(cmd,"pause"))
    mpd_run_toggle_pause(conn);
  else if(!g_ascii_strcasecmp(cmd,"stop"))
    mpd_run_stop(conn);
  mpd_response_finish(conn);
  mpd_send_idle_mask(conn,MPD_IDLE_PLAYER);
}
static void mpd_set_passwd ( gchar *pwd, gchar *dummy, void *d1,
    void *d2, void *d3, void *d4 )
{
  g_free(password);
  if(pwd && *pwd)
    password = g_strdup(pwd);
  else
    password = NULL;
}

ModuleExpressionHandlerV1 handler1 = {
  .numeric = FALSE,
  .name = "Mpd",
  .parameters = "S",
  .function = mpd_expr_func
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &handler1,
  NULL
};

ModuleActionHandlerV1 act_handler1 = {
  .name = "MpdSetPassword",
  .function = mpd_set_passwd
};

ModuleActionHandlerV1 act_handler2 = {
  .name = "MpdCommand",
  .function = mpd_command
};

ModuleActionHandlerV1 *sfwbar_action_handlers[] = {
  &act_handler1,
  &act_handler2,
  NULL
};
