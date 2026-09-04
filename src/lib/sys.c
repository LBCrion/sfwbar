/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include <sys/statvfs.h>
#include <locale.h>
#include "exec.h"
#include "trigger.h"
#include "vm/vm.h"

/* Get current time string */
static value_t lib_sys_time ( vm_t *vm, value_t p[], gint np )
{
  GTimeZone *tz;
  GDateTime *time;
  gchar *str;

  vm_param_check_np_range(vm, np, 0, 2, "time");
  if(np>0)
    vm_param_check_string(vm, p, 0, "time");
  if(np==2)
    vm_param_check_string(vm, p, 1, "time");

  if(np<2)
    time = g_date_time_new_now_local();
  else
  {
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 68
    if( !(tz = g_time_zone_new_identifier(value_get_string(p[1]))) )
      tz = g_time_zone_new_utc();
#else
    tz = g_time_zone_new(value_get_string(p[1]));
#endif
    time = g_date_time_new_now(tz);
    g_time_zone_unref(tz);
  }

  str = g_date_time_format(time, (np>0)? value_get_string(p[0]) :
      "%a %b %d %H:%M:%S %Y");
  g_date_time_unref(time);

  return value_take_string(str);
}

/* query current locale */
static value_t lib_sys_getlocale ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(setlocale(LC_ALL, NULL));
}

/* generate disk space utilization for a device */
static value_t lib_sys_disk ( vm_t *vm, value_t p[], gint np )
{
  struct statvfs fs;

  vm_param_check_np(vm, np, 2, "disk");
  vm_param_check_string(vm, p, 0, "disk");
  vm_param_check_string(vm, p, 1, "disk");

  if(statvfs(value_get_string(p[0]), &fs))
    return value_na;

  if(!g_ascii_strcasecmp(value_get_string(p[1]), "total"))
    return value_new_numeric(fs.f_blocks * fs.f_frsize);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "avail"))
    return value_new_numeric(fs.f_bavail * fs.f_bsize);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "free"))
    return value_new_numeric(fs.f_bfree * fs.f_bsize);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "%avail"))
    return value_new_numeric(((gdouble)(fs.f_bfree*fs.f_bsize) /
        (gdouble)(fs.f_blocks*fs.f_frsize))*100);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "%used"))
    return value_new_numeric((1.0 - (gdouble)(fs.f_bfree*fs.f_bsize) /
        (gdouble)(fs.f_blocks*fs.f_frsize))*100);

  return value_na;
}

static value_t lib_sys_gettext ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 1, 2, "GT");
  vm_param_check_string(vm, p, 0, "GT");
  if(np==2)
    vm_param_check_string(vm, p, 1, "GT");

  expr_dep_add(g_quark_from_static_string(".locale1"), vm->expr);

  return value_new_string(g_dgettext(
        np==2? value_get_string(p[1]) : "sfwbar", value_get_string(p[0])));
}

static value_t lib_sys_exit ( vm_t *vm, value_t p[], gint np )
{
  exit(1);
}

static value_t lib_sys_print ( vm_t *vm, value_t p[], gint np )
{
  gchar *str;

  vm_param_check_np(vm, np, 1, "Print");

  str = value_to_string(p[0], -1);
  g_message("%s", str);
  g_free(str);

  return value_na;
}

static value_t lib_sys_usleep ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "uSleep");

  g_usleep(value_as_numeric(p[0]));

  return value_na;
}

typedef struct _schedule_t {
  gchar *trigger;
  gint64 time;
  gint64 interval;
} schedule_t;

static gboolean lib_sys_schedule_cb ( schedule_t *sched );

static void lib_sys_schedule_schedule ( schedule_t *sched )
{
  gint64 span;

  span = sched->time - g_get_real_time() / 1000;
  if(span<0 && sched->interval)
    span += sched->interval*(1-(gint64)(span/sched->interval));
  if(span>=0)
    g_timeout_add(span, (GSourceFunc)lib_sys_schedule_cb, sched);
  else
  {
    g_free(sched->trigger);
    g_free(sched);
  }
}

static gboolean lib_sys_schedule_cb ( schedule_t *sched )
{
  trigger_emit(sched->trigger);
  lib_sys_schedule_schedule(sched);

  return G_SOURCE_REMOVE;
}

static value_t lib_sys_schedule ( vm_t *vm, value_t p[], gint np )
{
  schedule_t *sched;
  GTimeZone *tz;
  GDateTime *dt;

  vm_param_check_np(vm, np, 3, "Schedule");
  vm_param_check_string(vm, p, 0, "Schedule");
  vm_param_check_numeric(vm, p, 1, "Schedule");
  vm_param_check_string(vm, p, 2, "Schedule");

  tz = g_time_zone_new_local();
  dt = g_date_time_new_from_iso8601(value_get_string(p[0]), tz);
  g_time_zone_unref(tz);

  if(!dt)
  {
    g_warning("Schedule: invalid time string '%s'", value_get_string(p[0]));
    return value_na;
  }

  sched = g_malloc0(sizeof(schedule_t));
  sched->trigger = g_strdup(value_get_string(p[2]));
  sched->interval = value_get_numeric(p[1]);
  sched->time = g_date_time_to_unix(dt) * 1000;
  g_date_time_unref(dt);
  lib_sys_schedule_schedule(sched);

  return value_na;
}

static value_t lib_sys_exec ( vm_t *vm, value_t p[], gint np )
{
  if(np==1 && value_is_string(p[0]) && p[0].value.string)
    exec_cmd(value_get_string(p[0]));

  return value_na;
}

static value_t lib_sys_exec_term ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 0, 1, "ExecTerm");
  exec_cmd_in_term(value_get_string(p[0]));

  return value_na;
}

static value_t lib_sys_get_term ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 0, "GetTerm");

  return value_new_string(exec_term_get());
}

void lib_sys_init ( void )
{
  vm_func_add("time", lib_sys_time, FALSE, TRUE);
  vm_func_add("getlocale", lib_sys_getlocale, FALSE, FALSE);
  vm_func_add("disk", lib_sys_disk, FALSE, TRUE);
  vm_func_add("gt", lib_sys_gettext, TRUE, TRUE);
  vm_func_add("exit", lib_sys_exit, TRUE, TRUE);
  vm_func_add("print", lib_sys_print, TRUE, TRUE);
  vm_func_add("usleep", lib_sys_usleep, TRUE, TRUE);
  vm_func_add("schedule", lib_sys_schedule, TRUE, TRUE);
  vm_func_add("exec", lib_sys_exec, TRUE, TRUE);
  vm_func_add("execterm", lib_sys_exec_term, TRUE, TRUE);
  vm_func_add("getterm", lib_sys_get_term, TRUE, TRUE);
}
