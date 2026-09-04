/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "lib/lib.h"
#include "vm/vm.h"

void lib_init ( void )
{
  vm_func_init();
  lib_io_init();
  lib_compositor_init();
  lib_control_init();
  lib_sys_init();
  lib_ui_init();
  lib_value_init();
  lib_builtin_init();
}
