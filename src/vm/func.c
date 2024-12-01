
#include "vm/vm.h"
#include "util/string.h"

GHashTable *vm_func_table;

void vm_func_init ( void )
{
  if(vm_func_table)
    return;
  vm_func_table = g_hash_table_new_full((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal, NULL, g_free);
}

void vm_func_add ( gchar *name, vm_func_t func, gboolean deterministic )
{
  vm_function_t *function;

  function = g_malloc0(sizeof(vm_function_t));
  function->function = func;
  function->deterministic = deterministic;
  g_hash_table_insert(vm_func_table, name, function);
  expr_dep_trigger(name);
  g_debug("function: registered '%s'", name);
}

vm_function_t *vm_func_lookup ( gchar *name )
{
  return (vm_function_t *)g_hash_table_lookup(vm_func_table, name);
}

gboolean vm_func_remove ( gchar *name )
{
  return g_hash_table_remove(vm_func_table, name);
}
