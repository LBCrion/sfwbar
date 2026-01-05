/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- Sfwbar maintainers
 */


#include "module.h"
#include "trigger.h"
#include "util/string.h"
#include "gui/scaleimage.h"

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;

static gboolean mpd_connect( gpointer );

static GList *address_list, *address_current, *mpd_cmd_queue, *mpd_playlist;
static GList *mpd_db_list, *mpd_search_list;
static GSocketConnection *mpd_connection;
static GIOChannel *chan;
static GHashTable *mpd_state, *mpd_song_current, *mpd_tags_numeric;
static gchar *mpd_error, *mpd_cover;
static const gchar *mpd_cmd_current, *mpd_cmd_currentsong, *mpd_cmd_albumart,
             *mpd_cmd_playlistinfo, *mpd_cmd_status, *mpd_cmd_idle,
             *mpd_cmd_init, *mpd_cmd_list, *mpd_cmd_search, *mpd_cmd_find;
static gboolean mpd_idle;
static gint64 mpd_time, mpd_pb_counter;
static gsize mpd_cover_total, mpd_cover_received, mpd_cover_bufsize;
static GdkPixbufLoader *mpd_cover_loader;
static gpointer mpd_cover_buf;

gchar *mpd_tags_numeric_src[] = { "repeat", "random", "single", "consume",
  "volume", "playlist", "playlistlength", "song", "songid", "nextsong",
  "nextsongid", "bitrate", "xfade", "mixrampdb", "elapsed", "duration", NULL };

static void mpd_cmd_append ( gchar *cmd, ... )
{
  va_list args;
  
  if(!cmd)
    return;

  va_start(args, cmd);
  mpd_cmd_queue = g_list_append(mpd_cmd_queue, g_strdup_vprintf(cmd, args));
  if(mpd_idle)
  {
    g_io_channel_write_chars(chan, "noidle\n", 7, NULL, NULL);
    g_io_channel_flush(chan, NULL);
  }
}

static void mpd_cmd_cover ( gsize offset )
{
  gchar *file;

  if(!offset)
  {
    mpd_cover_total = 0;
    mpd_cover_received = 0;
    if(mpd_cover_loader)
      g_clear_pointer(&mpd_cover_loader, g_object_unref);
    mpd_cover_loader = gdk_pixbuf_loader_new();
  }
  if( (file = g_hash_table_lookup(mpd_song_current, "file")) && *file)
    mpd_cmd_append("albumart \"%s\" %d", file, offset);
}

static gboolean mpd_idle_handle ( gchar *str )
{
  gchar *change;

  if(g_ascii_strncasecmp(str, "changed:", 8))
    return FALSE;

  change = g_strstrip(str+8);

  if(!g_ascii_strcasecmp(change, "player"))
  {
    mpd_cmd_append("status");
    mpd_cmd_append("currentsong");
  }
  mpd_cmd_current = NULL;
  mpd_idle = FALSE;

  return TRUE;
}

static gboolean mpd_error_handle ( gchar *str )
{
  if(g_ascii_strncasecmp(str, "ACK", 3))
    return FALSE;

  str_assign(&mpd_error, g_strdup(g_strstrip(str+3)));
  g_message("mpd: error: %s", mpd_error);
  g_io_channel_write_chars(chan, "clearerror\n", 11, NULL, NULL);
  g_io_channel_flush(chan, NULL);

  return TRUE;
}

static gboolean mpd_ok_handle ( gchar *str )
{
  gchar *ptr;

  if(g_ascii_strncasecmp(str, "OK", 2))
    return FALSE;

  if(mpd_cmd_current == mpd_cmd_playlistinfo)
    trigger_emit("mpd-playlist");
  if(mpd_cmd_current == mpd_cmd_search || mpd_cmd_current == mpd_cmd_find)
    trigger_emit("mpd-search");
  else if(mpd_cmd_current == mpd_cmd_list)
    trigger_emit("mpd-list");
  mpd_cmd_current = NULL;
  trigger_emit("mpd");
  if(!mpd_cmd_queue)
  {
    mpd_cmd_current = mpd_cmd_idle;
    g_io_channel_write_chars(chan, "idle player playlist\n", 21, NULL, NULL);
    g_io_channel_flush(chan, NULL);
    mpd_idle = TRUE;
  }
  else
  {
    g_debug("mpd: command: %s", (gchar *)mpd_cmd_queue->data);
    g_io_channel_write_chars(chan, mpd_cmd_queue->data, -1, NULL, NULL);
    g_io_channel_write_chars(chan, "\n", 1, NULL, NULL);
    g_io_channel_flush(chan, NULL);
    if( (ptr = strchr(mpd_cmd_queue->data, ' ')) )
      *ptr = 0;
    mpd_cmd_current = g_intern_string(mpd_cmd_queue->data);
    g_free(mpd_cmd_queue->data);
    mpd_cmd_queue = g_list_delete_link(mpd_cmd_queue, mpd_cmd_queue);
    if(mpd_cmd_current == mpd_cmd_list)
      g_list_free_full(g_steal_pointer(&mpd_db_list), g_free);
    if(mpd_cmd_current == mpd_cmd_playlistinfo)
      g_list_free_full(g_steal_pointer(&mpd_playlist),
          (GDestroyNotify)g_hash_table_destroy);
    if(mpd_cmd_current == mpd_cmd_search || mpd_cmd_current == mpd_cmd_find)
      g_list_free_full(g_steal_pointer(&mpd_search_list),
          (GDestroyNotify)g_hash_table_destroy);
  }

  return TRUE;
}

static gboolean mpd_tag_handle ( gchar *str, GHashTable *hash )
{
  gchar *ptr, *val, *oldval;

  if( !(ptr = strchr(str, ':')) )
    return FALSE;
  *ptr = 0;

  if( !(val = g_strstrip(ptr+1)) )
    return FALSE;

  if(hash == mpd_state && !g_ascii_strcasecmp(str, "elapsed"))
    mpd_time = g_get_monotonic_time();

  oldval = g_hash_table_lookup(hash, str);
  if(oldval && !g_strcmp0(val, oldval))
    return TRUE;

  g_hash_table_insert(hash, g_strdup(str), g_strdup(g_strstrip(ptr+1)));
  if(hash == mpd_song_current && !g_ascii_strcasecmp(str, "file"))
    mpd_cmd_cover(0);

  return TRUE;
}

static gboolean mpd_cover_handle ( gchar *str )
{
  GdkPixbuf *pixbuf;
  GIOStatus status;
  gsize newtotal, chunk;

  if(!g_ascii_strncasecmp(str, "size:", 5))
  {
    newtotal = g_ascii_strtoull(g_strstrip(str+5), NULL, 10);
    if(mpd_cover_total && mpd_cover_total!=newtotal)
      mpd_cmd_cover(0);
    else
      mpd_cover_total = newtotal;
  }
  else if(!g_ascii_strncasecmp(str, "binary:", 7))
  {
    chunk = g_ascii_strtoull(g_strstrip(str+7), NULL, 10);
    if(mpd_cover_bufsize < chunk)
      mpd_cover_buf = g_realloc(mpd_cover_buf, (mpd_cover_bufsize = chunk) );
    status = g_io_channel_read_chars(chan, mpd_cover_buf, chunk, NULL, NULL);
    if(status==G_IO_STATUS_NORMAL)
    {
      mpd_cover_received += chunk;
      gdk_pixbuf_loader_write(mpd_cover_loader, mpd_cover_buf, chunk, NULL);
      if(mpd_cover_received>=mpd_cover_total)
      {
        if(gdk_pixbuf_loader_close(mpd_cover_loader, NULL) &&
            (pixbuf = gdk_pixbuf_loader_get_pixbuf(mpd_cover_loader)) )
        {
          scale_image_cache_remove(mpd_cover);
          str_assign(&mpd_cover, g_strdup_printf("<pixbufcache/>mpdart-%ld",
                mpd_pb_counter++));
          scale_image_cache_insert(mpd_cover, gdk_pixbuf_copy(pixbuf));
          trigger_emit("mpd-cover");
        }
        if(mpd_cover_loader)
          g_clear_pointer(&mpd_cover_loader, g_object_unref);
      }
      else
        mpd_cmd_cover(mpd_cover_received);
    }
  }

  return TRUE;
}

static gboolean mpd_playlist_handle ( gchar *str, GList **list )
{
  if(!g_ascii_strncasecmp(str, "file:", 5))
    *list = g_list_prepend(*list, g_hash_table_new_full(
          (GHashFunc)str_nhash, (GEqualFunc)str_nequal, g_free, g_free));
  return mpd_tag_handle(str, (*list)->data);
}

static gboolean mpd_event ( GIOChannel *chan, GIOCondition cond, void *d )
{
  gchar *str;

  if(cond & G_IO_ERR || cond & G_IO_HUP)
  {
    g_debug("mpd: error cond %d", cond);
    return G_SOURCE_REMOVE;
  }
  if( !(cond & G_IO_IN) && !(cond & G_IO_PRI) )
    return G_SOURCE_CONTINUE;
  while(g_io_channel_read_line(chan, &str, NULL, NULL, NULL)==
      G_IO_STATUS_NORMAL && str)
  {
    if(mpd_cmd_current == mpd_cmd_idle)
      mpd_idle_handle(str);
    else if(mpd_cmd_current == mpd_cmd_status)
      mpd_tag_handle(str, mpd_state);
    else if(mpd_cmd_current == mpd_cmd_currentsong)
      mpd_tag_handle(str, mpd_song_current);
    else if(mpd_cmd_current == mpd_cmd_albumart)
      mpd_cover_handle(str);
    else if(mpd_cmd_current == mpd_cmd_playlistinfo)
      mpd_playlist_handle(str, &mpd_playlist);
    else if(mpd_cmd_current == mpd_cmd_search || mpd_cmd_current == mpd_cmd_find)
      mpd_playlist_handle(str, &mpd_search_list);
    else if(mpd_cmd_current == mpd_cmd_list && strchr(str,':'))
      mpd_db_list = g_list_prepend(mpd_db_list, g_strdup(g_strstrip(strchr(str, ':')+1)));

    if(!mpd_ok_handle(str))
      mpd_error_handle(str);

    g_free(str);
  }

  return G_SOURCE_CONTINUE;
}

static void mpd_reconnect ( void )
{
  if( !(address_current = g_list_next(address_current)) )
  {
    address_current = address_list;
    g_timeout_add(1000, mpd_connect, NULL);
  }
  else
    g_idle_add(mpd_connect, NULL);
}

static void mpd_connect_cb ( GSocketClient *client, GAsyncResult *res,
    GSocketConnectable *conn )
{
  GSocket *sock;

  mpd_connection = g_socket_client_connect_finish(client, res, NULL);
  g_object_unref(client);
  if(mpd_connection && !g_socket_connection_is_connected(mpd_connection))
    g_clear_pointer(&mpd_connection, g_object_unref);
  if(mpd_connection &&
      (sock = g_socket_connection_get_socket(mpd_connection)) &&
      (chan = g_io_channel_unix_new(g_socket_get_fd(sock))) )
  {
    g_io_channel_set_flags(chan, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_close_on_unref(chan, TRUE);
    g_io_channel_set_encoding(chan, NULL, NULL);
    mpd_cmd_append("status");
    mpd_cmd_append("currentsong");
    mpd_cmd_current = mpd_cmd_init;
    g_io_add_watch_full(chan, G_PRIORITY_DEFAULT,
        G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR,
        (GIOFunc)mpd_event, client, (GDestroyNotify)mpd_reconnect);
  }
  else
    mpd_reconnect();
}

static gboolean mpd_connect ( gpointer d )
{
  GSocketClient *client;

  if(!address_current)
    address_current = address_list;

  g_return_val_if_fail(address_current, G_SOURCE_REMOVE);

  g_debug("mpd: connecting: %s",
      g_socket_connectable_to_string(address_current->data));
  client = g_socket_client_new();
  g_socket_client_connect_async(client, address_current->data, NULL,
      (GAsyncReadyCallback)mpd_connect_cb, address_current->data);

  return G_SOURCE_REMOVE;
}

static void mpd_address_new ( const gchar *host, guint16 port )
{
  GSocketConnectable *conn;
  const gchar *host_act;

  if(!host)
    return;

  if(*host != '@' && (host_act = strchr(host, '@')) )
    host_act++;
  else
    host_act = host;

  if(!host_act || !*host_act)
    return;

  if(*host_act == '@')
    conn = (GSocketConnectable *)g_unix_socket_address_new_with_type(
        host_act+1, -1, G_UNIX_SOCKET_ADDRESS_ABSTRACT);
  if(*host_act == '/')
    conn = (GSocketConnectable *)g_unix_socket_address_new(host_act);
  else
    conn = g_network_address_parse(host_act, port? port : 6600, NULL);

  if(host_act != host)
    g_object_set_data_full(G_OBJECT(conn), "password",
        g_strndup(host, host_act - host), g_free);

  address_list = g_list_prepend(address_list, conn);
}

static value_t mpd_func_list ( vm_t *vm, value_t p[], gint np )
{
  value_t result, row;
  GList *list, *iter;
  gint i;

  if(np<2)
    return value_na;

  vm_param_check_string(vm, p, 0, "MpdList");
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "playlist"))
    list = mpd_playlist;
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "search"))
    list = mpd_search_list;
  else
    return value_na;

  result = value_array_create(g_list_length(list));
  for(iter=list; iter; iter=g_list_next(iter))
  {
    row = value_array_create(np-1);
    for(i=1; i<np; i++)
      value_array_append(row, value_new_string(g_strdup(
              g_hash_table_lookup(iter->data, value_get_string(p[i])))));
    value_array_append(result, row);
  }

  return result;
}

static value_t mpd_func_info ( vm_t *vm, value_t p[], gint np )
{
  gchar *val;

  vm_param_check_np(vm, np, 1, "MpdInfo");
  vm_param_check_string(vm, p, 0, "MpdInfo");

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "age"))
    return value_new_numeric(g_get_monotonic_time() - mpd_time);
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "cover"))
    return value_new_string(g_strdup(mpd_cover));
  if(!g_ascii_strcasecmp(value_get_string(p[0]), "db"))
  {
    value_t array = value_array_create(g_list_length(mpd_db_list));
    for(GList *iter=mpd_db_list; iter; iter=g_list_next(iter))
      value_array_append(array, value_new_string(g_strdup(iter->data)));
    return array;
  }
  if((val = g_hash_table_lookup(mpd_state, value_get_string(p[0]))) ||
      (val = g_hash_table_lookup(mpd_song_current, value_get_string(p[0]))))
  {
    if(g_hash_table_lookup(mpd_tags_numeric, value_get_string(p[0])))
      return value_new_numeric(g_ascii_strtod(val, 0));
    else
      return value_new_string(g_strdup(val));
  }

  return value_na;
}

static value_t mpd_func_cmd ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MpdCmd");
  vm_param_check_string(vm, p, 0, "MpdCmd");

  mpd_cmd_append("%s", value_get_string(p[0]));

  return value_na;
}

gboolean sfwbar_module_init ( void )
{
  const gchar *dir, *port;
  gchar *tmp;
  gint i;

  mpd_cmd_idle = g_intern_static_string("idle");
  mpd_cmd_init = g_intern_static_string("init");
  mpd_cmd_currentsong = g_intern_static_string("currentsong");
  mpd_cmd_status = g_intern_static_string("status");
  mpd_cmd_playlistinfo = g_intern_static_string("playlistinfo");
  mpd_cmd_albumart = g_intern_static_string("albumart");
  mpd_cmd_list = g_intern_static_string("list");
  mpd_cmd_search = g_intern_static_string("search");
  mpd_cmd_find = g_intern_static_string("find");

  mpd_state = g_hash_table_new_full((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal, g_free, g_free);
  mpd_song_current = g_hash_table_new_full((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal, g_free, g_free);
  mpd_tags_numeric = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  for(i=0; mpd_tags_numeric_src[i]; i++)
    g_hash_table_insert(mpd_tags_numeric, mpd_tags_numeric_src[i], (void *)1);

  vm_func_add("mpdlist", mpd_func_list, FALSE, FALSE);
  vm_func_add("mpdinfo", mpd_func_info, FALSE, FALSE);
  vm_func_add("mpdcmd", mpd_func_cmd, TRUE, FALSE);

  mpd_address_new("localhost", 6600);
  mpd_address_new("/run/mpd/socket", 0);

  if( (dir = g_get_user_runtime_dir()) )
  {
    tmp = g_build_filename(dir, "/mpd/socket", NULL);
    mpd_address_new(tmp, 0);
    g_free(tmp);
  }

  port = g_getenv("MPD_PORT");
  mpd_address_new(g_getenv("MPD_HOST"), port? g_ascii_strtoll(port, NULL, 10) : 0);

  g_idle_add(mpd_connect, NULL);

  return TRUE;
}
