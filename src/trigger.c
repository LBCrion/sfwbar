/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2024- sfwbar maintainers
 */

#include <glib.h>

static GHashTable *trigger_list;

typedef struct _trigger {
  GSourceFunc func;
  gpointer data;
} trigger_t;

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

const gchar *trigger_add ( gchar *name, GSourceFunc func, void *data )
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
    if(TRIGGER(iter->data)->func==func && TRIGGER(iter->data)->data==data)
      return NULL;
  trigger = g_malloc0(sizeof(trigger_t));
  trigger->func = func;
  trigger->data = data;
  list = g_list_append(list, trigger);
  g_hash_table_replace(trigger_list, (gchar *)trigger_name, list);

  return trigger_name;
}

void trigger_remove ( gchar *name, GSourceFunc func, void *data )
{
  GList *list, *iter;
  const gchar *iname;
  gpointer ptr;

  if(!name || !func)
    return;

  iname = trigger_name_intern(name);

  if(!trigger_list)
    trigger_list = g_hash_table_new(g_direct_hash, g_direct_equal);

  list = g_hash_table_lookup(trigger_list, iname);
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

void trigger_emit_in_main_context ( const gchar *name )
{
  GList *iter;

  if(trigger_list)
    for(iter=g_hash_table_lookup(trigger_list, name); iter; iter=iter->next)
      TRIGGER(iter->data)->func(TRIGGER(iter->data)->data);
}

void trigger_emit ( gchar *name )
{
  g_main_context_invoke(NULL, (GSourceFunc)trigger_emit_in_main_context,
      (gpointer)trigger_name_intern(name));
}
