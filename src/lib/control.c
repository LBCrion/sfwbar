/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "module.h"
#include "scanner.h"
#include "trigger.h"
#include "vm/vm.h"

static value_t lib_control_call ( vm_t *vm, value_t p[], gint np )
{
  vm_function_t func;

  vm_param_check_np(vm, np, 1, "Call");
  vm_param_check_string(vm, p, 0, "Call");

  if(vm_func_copy(&func, vm_func_lookup(value_get_string(p[0]))) &&
      (func.flags & VM_FUNC_USERDEFINED))
    value_free(vm_function_user(vm, &func, 0));

  return value_na;
}

static value_t lib_control_eval ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "Eval");
  vm_param_check_string(vm, p, 0, "Eval");

  scanner_var_new_calc(value_get_string(p[0]), NULL,
      parser_string_build(value_to_string(p[1], -1)), VM_STORE(vm));

  return value_na;
}

static value_t lib_control_emit_trigger ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "EmitTrigger");
  vm_param_check_string(vm, p, 0, "EmitTrigger");

  trigger_emit(value_get_string(p[0]));

  return value_na;
}

static value_t lib_control_iface_provider ( vm_t *vm, value_t p[], int np )
{
  vm_param_check_np(vm, np, 1, "InterfaceProvider");
  vm_param_check_string(vm, p, 0, "InterfaceProvider");

  return value_take_string(
      module_interface_provider_get(value_get_string(p[0])));
}

void lib_control_init ( void )
{
  vm_func_add("call", lib_control_call, TRUE, TRUE);
  vm_func_add("eval", lib_control_eval, TRUE, FALSE);
  vm_func_add("emittrigger", lib_control_emit_trigger, FALSE, TRUE);
  vm_func_add("interfaceprovider", lib_control_iface_provider, FALSE, TRUE);
}
