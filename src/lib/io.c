/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "client.h"
#include "trigger.h"
#include "config/config.h"
#include "util/file.h"

static void lib_io_read_uri_cb ( GFile *file, GAsyncResult *res, gchar *tr )
{
  GError *err = NULL;
  gchar *str;

  if(g_file_load_contents_finish(file, res, &str, NULL, NULL, &err))
    trigger_emit_with_string(tr, "result", str);
  else
  {
    g_warning("ReadURI failed. Error: %s", err? err->message : "none");
    g_error_free(err);
  }

  g_free(tr);
  g_object_unref(file);
}

static value_t lib_io_read_uri ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ReadURI");
  vm_param_check_string(vm, p, 0, "ReadURI");
  vm_param_check_string(vm, p, 1, "ReadURI");

  g_file_load_contents_async(g_file_new_for_uri(value_get_string(p[0])),
      NULL, (GAsyncReadyCallback)lib_io_read_uri_cb,
      g_strdup(value_get_string(p[1])));

  return value_na;
}

static value_t lib_io_read ( vm_t *vm, value_t p[], gint np )
{
  gchar *fname, *result;
  GIOChannel *in;

  vm_param_check_np(vm, np, 1, "read");
  vm_param_check_string(vm, p, 0, "read");

  if( !(fname = get_xdg_config_file(value_get_string(p[0]), NULL)) )
    return value_take_string(g_strdup_printf("Read: file not found '%s'",
          value_get_string(p[0])));

  if( (in = g_io_channel_new_file(value_get_string(p[0]), "r", NULL)) )
  {
    (void)g_io_channel_read_to_end(in, &result, NULL, NULL);
    g_io_channel_unref(in);
  }
  else
    result = NULL;

  if(!result)
    result = g_strdup_printf("Read: can't open file '%s'", fname);

  g_free(fname);
  return value_take_string(result);
}

static value_t lib_io_exec_read ( vm_t *vm, value_t p[], gint np )
{
  gchar **argv, *str;
  value_t result;

  vm_param_check_np(vm, np, 1, "ExecRead");
  vm_param_check_string(vm, p, 0, "ExecRead");

  if(!g_shell_parse_argv(value_get_string(p[0]), NULL, &argv, NULL))
    return value_na;

  if(!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH |
        G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &str, NULL, NULL, NULL))
    result = value_na;
  else
    result = value_take_string(str);

  g_strfreev(argv);
  return result;
}

static value_t lib_io_test_file ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "TestFile");
  vm_param_check_string(vm, p, 0, "TestFile");

  return value_new_numeric(file_test_read(value_get_string(p[0])));
}

static value_t lib_io_ls ( vm_t *vm, value_t p[], gint np )
{
  GDir *dir;
  value_t array;
  const gchar *file;

  vm_param_check_np(vm, np, 1, "ls");
  vm_param_check_string(vm, p, 0, "ls");

  if( !(dir = g_dir_open(value_get_string(p[0]), 0, NULL)) )
    return value_na;

  array = value_array_create(1);
  while( (file = g_dir_read_name(dir)) )
    value_array_append(array, value_new_string(file));
  g_dir_close(dir);

  return array;
}

static void lib_io_file_trigger_cb ( GFileMonitor *m, GFile *f1, GFile *f2,
    GFileMonitorEvent ev, gchar *trigger )
{
  g_source_remove_by_user_data(trigger);
  g_debug("FileTrigger: emit '%s'", trigger);
  trigger_emit(trigger);
}

static gboolean lib_io_file_timeout_cb ( gchar *trigger )
{
  g_debug("FileTrigger: timeout emit '%s'", trigger);
  trigger_emit(trigger);

  return TRUE;
}

static value_t lib_io_file_trigger ( vm_t *vm, value_t p[], gint np )
{
  GFile *f;
  GFileMonitor *m;

  vm_param_check_np_range(vm, np, 2, 3, "FileTrigger");
  vm_param_check_string(vm, p, 0, "FileTrigger");
  vm_param_check_string(vm, p, 1, "FileTrigger");
  if(np==3)
    vm_param_check_numeric(vm, p, 2, "FileTrigger");

  f = g_file_new_for_path(value_get_string(p[0]));
  if( !(m = g_file_monitor_file(f, 0, NULL, NULL)) )
  {
    g_debug("FileTrigger: unable to setup a file monitor for %s",
        value_get_string(p[0]));
    g_object_unref(f);
    return value_na;
  }
  g_file_monitor_set_rate_limit(m, 0);
  if(np==3)
    g_timeout_add(value_get_numeric(p[2]), (GSourceFunc)lib_io_file_timeout_cb,
        (gpointer)trigger_name_intern(value_get_string(p[1])));
  g_object_unref(f);
  g_signal_connect(G_OBJECT(m), "changed", G_CALLBACK(lib_io_file_trigger_cb),
      (gpointer)trigger_name_intern(value_get_string(p[1])));
  g_debug("FileTrigger: subscribe to '%s', trigger '%s'",
      value_get_string(p[0]), value_get_string(p[1]));
  return value_na;
}

static value_t lib_io_client_send ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ClientSend");
  vm_param_check_string(vm, p, 0, "ClientSend");
  vm_param_check_string(vm, p, 1, "ClientSend");

  client_send(value_get_string(p[0]), value_get_string(p[1]));

  return value_na;
}

void lib_io_init ( void )
{
  vm_func_add("read", lib_io_read, FALSE, TRUE);
  vm_func_add("readuri", lib_io_read_uri, FALSE, TRUE);
  vm_func_add("execread", lib_io_exec_read, TRUE, TRUE);
  vm_func_add("testfile", lib_io_test_file, FALSE, TRUE);
  vm_func_add("ls", lib_io_ls, FALSE, TRUE);
  vm_func_add("filetrigger", lib_io_file_trigger, FALSE, FALSE);
  vm_func_add("clientsend", lib_io_client_send, TRUE, FALSE);
}
