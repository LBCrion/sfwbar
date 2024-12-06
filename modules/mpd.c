/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include <glib.h>
#include <mpd/client.h>
#include "module.h"
#include "trigger.h"
#include "stdio.h"
#include "vm/vm.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 2;
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
    trigger_emit("mpd-progress");

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

  trigger_emit("mpd");
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
    trigger_emit("mpd");
    return FALSE;
  }

  mpd_send_idle_mask(conn,MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS);
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
  g_debug("MPD client: connected to server (fd = %d)",
      mpd_connection_get_fd(conn));

  mpd_update();
  mpd_send_idle_mask(conn,MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS);

  chan = g_io_channel_unix_new(mpd_connection_get_fd(conn));
  g_io_add_watch(chan,G_IO_IN,(GIOFunc)mpd_event,conn);
  g_io_channel_unref(chan);
 
  return FALSE;
}

static void mpd_bool_set( bool (*get)(const struct mpd_status *),
    bool (*set)(struct mpd_connection *, bool), gchar *val )
{
  gboolean new;

  if(!conn || !status || !get || !set || !val)
    return;

  while(*val && g_ascii_isspace(*val))
    val++;

  if(!g_ascii_strcasecmp(val,"on"))
    new = TRUE;
  else if(!g_ascii_strcasecmp(val,"off"))
    new = FALSE;
  else if(!g_ascii_strcasecmp(val,"toggle"))
    new = !get(status);
  else
    return;

  set(conn,new);
}

static value_t mpd_expr_func ( vm_t *vm, value_t p[], gint np )
{
  if(!conn | !status | !song)
    return value_new_string(g_strdup("disconnected"));
  if(np!=1 || !value_is_string(p[0]))
    return value_na;

  if(!g_ascii_strcasecmp(p[0].value.string,"title"))
    return value_new_string(g_strdup(
          song? mpd_song_get_tag(song, MPD_TAG_TITLE, 0) : ""));
  if(!g_ascii_strcasecmp(p[0].value.string,"track"))
    return value_new_string(g_strdup(
          song? mpd_song_get_tag(song, MPD_TAG_TRACK, 0) : ""));
  if(!g_ascii_strcasecmp(p[0].value.string,"artist"))
    return value_new_string(g_strdup(
          song? mpd_song_get_tag(song, MPD_TAG_ARTIST, 0) : ""));
  if(!g_ascii_strcasecmp(p[0].value.string,"album"))
    return value_new_string(g_strdup(
          song? mpd_song_get_tag(song, MPD_TAG_ALBUM, 0): ""));
  if(!g_ascii_strcasecmp(p[0].value.string, "genre"))
    return value_new_string(g_strdup(
          song? mpd_song_get_tag(song, MPD_TAG_GENRE, 0) : ""));
  if(!g_ascii_strcasecmp(p[0].value.string, "volume"))
    return value_new_string(
        g_strdup_printf("%d", status? mpd_status_get_volume(status) : 0));
  if(!g_ascii_strcasecmp(p[0].value.string, "repeat"))
    return value_new_string(
        g_strdup_printf("%d", status? mpd_status_get_repeat(status) : 0));
  if(!g_ascii_strcasecmp(p[0].value.string, "random"))
    return value_new_string(
        g_strdup_printf("%d", status? mpd_status_get_random(status) : 0));
  if(!g_ascii_strcasecmp(p[0].value.string, "queue_len"))
    return value_new_string(
      g_strdup_printf("%d", status? mpd_status_get_queue_length(status) : 0));
  if(!g_ascii_strcasecmp(p[0].value.string, "queue_pos"))
    return value_new_string(
      g_strdup_printf("%d", status? mpd_status_get_song_pos(status) : 0));
  if(!g_ascii_strcasecmp(p[0].value.string, "elapsed"))
    return value_new_string(g_strdup_printf("%llu",
          (long long unsigned int)(status?  (mpd_status_get_elapsed_ms(status)+
              mpd_status_get_state(status)==MPD_STATE_PLAY?
         (g_get_monotonic_time()-last_update)/1000:0) : 0)));
  if(!g_ascii_strcasecmp(p[0].value.string, "length"))
    return value_new_string(
        g_strdup_printf("%u", status? mpd_status_get_total_time(status) : 0));
  if(!g_ascii_strcasecmp(p[0].value.string, "rate"))
    return value_new_string(
        g_strdup_printf("%u", status? mpd_status_get_kbit_rate(status) : 0));
  if(!g_ascii_strcasecmp(p[0].value.string, "state"))
    switch(mpd_status_get_state(status))
    {
      case MPD_STATE_PLAY:
        return value_new_string(g_strdup("play"));
      case MPD_STATE_PAUSE:
        return value_new_string(g_strdup("pause"));
      case MPD_STATE_STOP:
        return value_new_string(g_strdup("stop"));
      default:
        return value_new_string(g_strdup("unknown"));
    }

  return value_new_string(g_strdup("Invalid request"));
}

static value_t mpd_command ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MpdCommand");
  vm_param_check_string(vm, p, 0, "MpdCommand");

  if(!value_is_string(p[0]) || !p[0].value.string)
    return value_na;

  mpd_run_noidle(conn);
  if(!g_ascii_strcasecmp(p[0].value.string, "play"))
    mpd_run_play(conn);
  else if(!g_ascii_strcasecmp(p[0].value.string, "prev"))
    mpd_run_previous(conn);
  else if(!g_ascii_strcasecmp(p[0].value.string, "next"))
    mpd_run_next(conn);
  else if(!g_ascii_strcasecmp(p[0].value.string, "pause"))
    mpd_run_toggle_pause(conn);
  else if(!g_ascii_strcasecmp(p[0].value.string, "stop"))
    mpd_run_stop(conn);
  else if(!g_ascii_strncasecmp(p[0].value.string, "random", 6))
    mpd_bool_set(mpd_status_get_random, mpd_run_random, p[0].value.string+6);
  else if(!g_ascii_strncasecmp(p[0].value.string, "repeat", 6))
    mpd_bool_set(mpd_status_get_repeat, mpd_run_repeat, p[0].value.string+6);

  mpd_response_finish(conn);
  mpd_send_idle_mask(conn,MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS);

  return value_na;
}

static value_t mpd_set_passwd ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MpdSetPassword");
  vm_param_check_string(vm, p, 0, "MpdSetPassword");

  if(value_is_string(p[0]) && p[0].value.string && *(p[0].value.string))
  {
    g_free(password);
    password = g_strdup(p[0].value.string);
  }
  else
    password = NULL;

  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  vm_func_add("mpd", mpd_expr_func, FALSE);
  vm_func_add("mpdcommand", mpd_command, TRUE);
  vm_func_add("mpdsetpassword", mpd_set_passwd, TRUE);
  if(mpd_connect(NULL))
    g_timeout_add (1000,(GSourceFunc )mpd_connect,NULL);
  return TRUE;
}
