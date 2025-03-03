/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include <glib.h>
#include "vm/vm.h"

vm_var_t *vm_var_new ( gchar *name )
{
  vm_var_t *var;
  gchar *lower;
  
  g_return_val_if_fail(name, NULL);

  var = g_malloc0(sizeof(vm_var_t));
  var->value = value_na;
  lower = g_ascii_strdown(name, -1);
  var->quark = g_quark_from_string(lower);
  g_free(lower);

  return var;
}

void vm_var_free ( vm_var_t *var )
{
  value_free(var->value);
  g_free(var->name);
  g_free(var);
}

vm_store_t *vm_store_new ( vm_store_t *parent )
{
  vm_store_t *store = g_malloc0(sizeof(vm_store_t));
  store->parent = parent;
  g_datalist_init(&store->vars);

  return store;
}

void vm_store_free ( vm_store_t *store )
{
  g_datalist_clear(&store->vars);
  g_free(store);
}

vm_var_t *vm_store_lookup ( vm_store_t *store, GQuark id )
{
  vm_store_t *iter;
  vm_var_t *var;

  for(iter=store; iter; iter=iter->parent)
    if( (var = g_datalist_id_get_data(&iter->vars, id)) )
      return var;

  return NULL;
}

vm_var_t *vm_store_lookup_string ( vm_store_t *store, gchar *string )
{
  gchar *lower;
  GQuark quark;

  g_return_val_if_fail(store && string, NULL);
  lower = g_ascii_strdown(string, -1);
  quark = g_quark_from_string(lower);
  g_free(lower);

  return vm_store_lookup(store, quark);
}

gboolean vm_store_insert ( vm_store_t *store, vm_var_t *var )
{
  gboolean found;

  g_return_val_if_fail(store && var, FALSE);

 if( (found = !!g_datalist_id_get_data(&store->vars, var->quark)) )
   g_datalist_id_remove_data(&store->vars, var->quark);

  g_datalist_id_set_data_full(&(store->vars), var->quark, var,
      (GDestroyNotify)vm_var_free);

  return found;
}

vm_closure_t *vm_closure_new ( GBytes *code, vm_store_t *store )
{
  vm_closure_t *closure;

  g_return_val_if_fail(code && store, NULL);

  closure = g_malloc0(sizeof(vm_closure_t));
  closure->code = code;
  closure->store = store;

  return closure;
}
