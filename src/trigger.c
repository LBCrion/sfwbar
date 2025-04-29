/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include <glib.h>
#include "trigger.h"
#include "vm/vm.h"

static GHashTable *trigger_list;

typedef struct _trigger {
  trigger_func_t func;
  gpointer data;
} trigger_t;

typedef struct _trigger_invocation {
  const gchar *trigger;
  vm_store_t *store;
} trigger_invocation_t;

#define TRIGGER(x) ((trigger_t *)(x))

const gchar *trigger_name_intern  ( gchar *name )
{
  gchar *lower;
  const gchar *trigger_name;

  if(!name)
    return NULL;

  lower = g_ascii_strdown(name, -1);
  trigger_name = g_intern_string(lower);
  g_free(lower);

  return trigger_name;
}

const gchar *trigger_add ( gchar *name, trigger_func_t func, void *data )
{
  trigger_t *trigger;
  GList *list, *iter;
  const gchar *trigger_name;

  if(!name || !func)
    return NULL;

  trigger_name = trigger_name_intern(name);

  if(!trigger_list)
    trigger_list = g_hash_table_new(g_direct_hash, g_direct_equal);

  list = g_hash_table_lookup(trigger_list, trigger_name);
  for(iter=list; iter; iter=g_list_next(iter))
    if(TRIGGER(iter->data)->func == func && TRIGGER(iter->data)->data == data)
      return NULL;
  trigger = g_malloc0(sizeof(trigger_t));
  trigger->func = func;
  trigger->data = data;
  list = g_list_append(list, trigger);
  g_hash_table_replace(trigger_list, (gchar *)trigger_name, list);

  return trigger_name;
}

void trigger_remove ( gchar *name, trigger_func_t func, void *data )
{
  GList *list, *iter;
  gpointer ptr;

  if(!name || !func || !trigger_list)
    return;

  list = g_hash_table_lookup(trigger_list, trigger_name_intern(name));
  for(iter=list; iter; iter=g_list_next(iter))
    if(TRIGGER(iter->data)->func==func && TRIGGER(iter->data)->data==data)
    {
      ptr = iter->data;
      list = g_list_remove(list, ptr);
      g_free(ptr);
      g_hash_table_replace(trigger_list, name, list);
      return;
    }
}

void trigger_action_cb ( vm_closure_t *closure, vm_store_t *store )
{
  vm_store_t *new_store;

  if(!store)
  {
    vm_run_action(closure->code, NULL, NULL, NULL, NULL, closure->store);
    return;
  }
 
  new_store = vm_store_dup(store);

  new_store->parent = closure->store;
  vm_run_action(closure->code, NULL, NULL, NULL, NULL, new_store);
  vm_store_free(new_store);
}

static gboolean trigger_emit_in_main_context ( trigger_invocation_t *inv )
{
  GList *iter;

  g_debug("trigger: '%s' %p", inv->trigger, inv->store);
  if(trigger_list)
    for(iter=g_hash_table_lookup(trigger_list, inv->trigger); iter;
        iter=iter->next)
      TRIGGER(iter->data)->func(TRIGGER(iter->data)->data, inv->store);

  vm_store_free(inv->store);
  g_free(inv);

  return FALSE;
}

void trigger_emit_with_data ( gchar *name, vm_store_t *store )
{
  trigger_invocation_t *inv;

  inv = g_malloc0(sizeof(trigger_invocation_t));
  inv->trigger = trigger_name_intern(name);
  inv->store = vm_store_dup(store);

  g_main_context_invoke(NULL, (GSourceFunc)trigger_emit_in_main_context, inv);
}

gboolean trigger_emit ( gchar *name )
{
  trigger_emit_with_data(name, NULL);
  return G_SOURCE_REMOVE;
}
