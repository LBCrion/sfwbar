/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "expr.h"
#include "scanner.h"
#include "vm/vm.h"
#include "util/string.h"

static GData *expr_deps;

expr_cache_t *expr_cache_new ( void )
{
  return g_malloc0(sizeof(expr_cache_t));
}

expr_cache_t *expr_cache_new_with_code ( GBytes *code )
{
  expr_cache_t *expr;

  expr = expr_cache_new();
  expr->code = g_bytes_ref(code);
  expr->eval = TRUE;

  return expr;
}

void expr_cache_free ( expr_cache_t *expr )
{
  if(!expr)
    return;
  expr_dep_remove(expr);
  g_free(expr->definition);
  g_free(expr->cache);
  g_bytes_unref(expr->code);
  g_free(expr);
}

void expr_dep_add ( GQuark quark, expr_cache_t *expr )
{
  expr_cache_t *iter;
  GList *list;

  if(!expr)
    return;

  list = g_datalist_id_get_data(&expr_deps, quark);
  for(iter=expr; iter; iter=iter->parent)
    if(!g_list_find(list, iter))
      list = g_list_prepend(list, iter);
  g_datalist_id_set_data(&expr_deps, quark, list);
}

static void expr_dep_remove_func ( GQuark quark, GList *l, expr_cache_t *expr )
{
  g_datalist_id_replace_data(&expr_deps, quark, l, g_list_remove(l, expr),
      NULL, NULL);
}

void expr_dep_remove ( expr_cache_t *expr )
{
  g_datalist_foreach(&expr_deps, (GDataForeachFunc)expr_dep_remove_func, expr);
}

void expr_dep_trigger ( GQuark quark )
{
  GList *iter, *list;

  list = g_datalist_id_get_data(&expr_deps, quark);

  for(iter=list; iter; iter=g_list_next(iter))
    ((expr_cache_t *)(iter->data))->eval = TRUE;
}

static void expr_dep_dump_each ( GQuark key, void *value, void *d )
{
  GList *iter;

  for(iter=value; iter; iter=g_list_next(iter))
    g_message("%s: %s", g_quark_to_string(key),
        ((expr_cache_t *)iter->data)->definition);
}

void expr_dep_dump ( void )
{
  g_datalist_foreach(&expr_deps, expr_dep_dump_each, NULL);
}
