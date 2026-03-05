/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include "vm/vm.h"
#include "util/string.h"

GHashTable *vm_func_table;
GMutex func_mutex;

void vm_func_init ( void )
{
  if(vm_func_table)
    return;
  vm_func_table = g_hash_table_new((GHashFunc)str_nhash,
      (GEqualFunc)str_nequal);
}

vm_function_t *vm_func_lookup ( gchar *name )
{
  vm_function_t *func;

  g_mutex_lock(&func_mutex);
  func = (vm_function_t *)g_hash_table_lookup(vm_func_table, name);
  g_mutex_unlock(&func_mutex);

  if(func)
    return func;

  func = g_malloc0(sizeof(vm_function_t));
  func->name = g_strdup(name);
  g_mutex_lock(&func_mutex);
  g_hash_table_insert(vm_func_table, func->name, func);
  g_mutex_unlock(&func_mutex);

  return func;
}

void vm_func_add_full ( gchar *name, vm_func_t func, gboolean det,
    gboolean safe, GMainContext *ctx )
{
  vm_function_t *func_o;

  func_o = vm_func_lookup(name);

  g_mutex_lock(&func_mutex);
  g_atomic_int_inc(&func_o->seq);
  func_o->ptr.function = func;
  g_clear_pointer(&func_o->context, g_main_context_unref);
  func_o->context = ctx? g_main_context_ref(ctx) : NULL;
  func_o->flags = 0;
  func_o->flags |= (det? VM_FUNC_DETERMINISTIC : 0);
  func_o->flags |= (safe? VM_FUNC_THREADSAFE : 0);
  g_atomic_int_inc(&func_o->seq);
  g_mutex_unlock(&func_mutex);
  expr_dep_trigger(g_quark_from_string(name));
  g_debug("function: registered '%s'", name);
}

void vm_func_add ( gchar *name, vm_func_t func, gboolean det, gboolean safe )
{
  vm_func_add_full(name, func, det, safe, g_main_context_get_thread_default());
}

void vm_func_add_user ( gchar *name, GBytes *code, guint8 arity )
{
  vm_function_t *func;

  func = vm_func_lookup(name);
  g_mutex_lock(&func_mutex);
  g_atomic_int_inc(&func->seq);
  func->ptr.code = code;
  func->flags = VM_FUNC_USERDEFINED;
  func->arity = arity;
  g_atomic_int_inc(&func->seq);
  g_mutex_unlock(&func_mutex);
  expr_dep_trigger(g_quark_from_string(name));
  g_debug("function: registered '%s'", name);
}

void vm_func_remove ( gchar *name, vm_func_t ofunc )
{
  vm_function_t *func;

  g_mutex_lock(&func_mutex);
  if( (func = g_hash_table_lookup(vm_func_table, name)) &&
      (!ofunc || func->ptr.function == ofunc) )
  {
    g_atomic_int_inc(&func->seq);
    g_clear_pointer(&func->context, g_main_context_unref);
    func->ptr.function = NULL;
    g_atomic_int_inc(&func->seq);
    g_debug("function: removed '%s'", name);
  }
  g_mutex_unlock(&func_mutex);
}

vm_function_t *vm_func_copy ( vm_function_t *dest, vm_function_t *src )
{
  gint seq;

  if(!src)
    return NULL;

  do {
    while( ((seq = g_atomic_int_get(&src->seq))&0x01) );
    memcpy(dest, src, sizeof(vm_function_t));
  } while(g_atomic_int_get(&src->seq) != seq);

  return dest;
}
