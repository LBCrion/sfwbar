/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include <glib.h>
#include "util/string.h"
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

vm_store_t *vm_store_new ( vm_store_t *parent, gboolean transient )
{
  vm_store_t *store = g_malloc0(sizeof(vm_store_t));
  store->parent = parent;
  store->transient = transient;
  store->widget_map = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
  store->vars = datalist_new();
  store->refcount = 1;

  return store;
}

vm_store_t *vm_store_ref ( vm_store_t *store )
{
  if(!store)
    return NULL;

  store->refcount++;
  return store;
}

void vm_store_unref ( vm_store_t *store )
{
  if(!store)
    return;

  store->refcount--;
  if(store->refcount>0)
    return;

  if(store->vars)
    datalist_unref(store->vars);
  if(store->widget_map)
    g_hash_table_unref(store->widget_map);
  g_free(store);
}

vm_store_t *vm_store_dup ( vm_store_t *src )
{
  vm_store_t *dest;

  if(!src)
    return NULL;

  dest = g_malloc0(sizeof(vm_store_t));
  dest->refcount = 1;
  g_mutex_lock(&src->mutex);
  if(src->widget_map)
    dest->widget_map = g_hash_table_ref(src->widget_map);
  if(src->vars)
    dest->vars = datalist_ref(src->vars);
  dest->transient = src->transient;
  g_mutex_unlock(&src->mutex);

  return dest;
}

vm_var_t *vm_store_lookup ( vm_store_t *store, GQuark id )
{
  vm_store_t *iter;
  vm_var_t *var = NULL;

  if(!store)
    return NULL;

  g_mutex_lock(&store->mutex);
  for(iter=store; iter; iter=iter->parent)
    if( (var = g_datalist_id_get_data(&iter->vars->data, id)) )
      break;
  g_mutex_unlock(&store->mutex);

  return var;
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

  g_mutex_lock(&store->mutex);
  if( (found = !!g_datalist_id_get_data(&store->vars->data, var->quark)) )
    g_datalist_id_remove_data(&store->vars->data, var->quark);

  g_datalist_id_set_data_full(&(store->vars->data), var->quark, var,
      (GDestroyNotify)vm_var_free);
  g_mutex_unlock(&store->mutex);
  g_debug("var: new: '%s' in store %p", g_quark_to_string(var->quark), store);

  return found;
}

gboolean vm_store_insert_full ( vm_store_t *store, gchar *name, value_t v )
{
  vm_var_t *var;

  var = vm_var_new(name);
  var->value = v;

  return vm_store_insert(store, var);
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

void vm_closure_free ( vm_closure_t *closure )
{
  g_bytes_unref(closure->code);
  vm_store_unref(closure->store);
  g_free(closure);
}
