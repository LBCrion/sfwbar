/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "appinfo.h"
#include "exec.h"
#include "input.h"
#include "module.h"
#include "config/config.h"
#include "gui/basewidget.h"
#include "gui/taskbaritem.h"
#include "util/string.h"
#include "vm/vm.h"

static value_t action_config ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 1, 2, "Config");
  vm_param_check_string(vm, p, 0, "Config");
  if(np==2)
    vm_param_check_string(vm, p, 1, "Config");

  g_debug("parsing config string: %s", value_get_string(p[0]));
  config_parse_data(np==2? value_get_string(p[1]) : "config string",
      value_get_string(p[0]), NULL, VM_STORE(vm),
      ((guint8 *)g_bytes_get_data(vm->bytes, NULL))[0]);

  return value_na;
}

static value_t action_map_appid ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "MapAppId");
  vm_param_check_string(vm, p, 0, "MapAppId");
  vm_param_check_string(vm, p, 1, "MapAppId");

  wintree_appid_map_add(value_get_string(p[0]), value_get_string(p[1]));

  return value_na;
}

static value_t action_map_icon ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "MapIcon");
  vm_param_check_string(vm, p, 0, "MapIcon");
  vm_param_check_string(vm, p, 1, "MapIcon");

  app_icon_map_add(value_get_string(p[0]), value_get_string(p[1]));

  return value_na;
}

static value_t action_filter_appid ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "FilterAppid");
  vm_param_check_string(vm, p, 0, "FilterAppid");

  wintree_filter_appid(value_get_string(p[0]));

  return value_na;
}

static value_t action_filter_title ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "FilterTitle");
  vm_param_check_string(vm, p, 0, "FilterTitle");

  wintree_filter_title(value_get_string(p[0]));

  return value_na;
}

void action_lib_init ( void )
{
  vm_func_add("config", action_config, TRUE, FALSE);
  vm_func_add("mapappid", action_map_appid, TRUE, TRUE);
  vm_func_add("mapicon", action_map_icon, TRUE, TRUE);
  vm_func_add("filterappid", action_filter_appid, TRUE, TRUE);
  vm_func_add("filtertitle", action_filter_title, TRUE, TRUE);
}
