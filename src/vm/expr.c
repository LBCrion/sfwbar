/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "expr.h"
#include "scanner.h"
#include "vm/vm.h"
#include "util/string.h"

static datalist_t *expr_deps;

expr_cache_t *expr_cache_new ( void )
{
  expr_cache_t *expr = g_malloc0(sizeof(expr_cache_t));
  expr->refcount = 1;
  return expr;
}

expr_cache_t *expr_cache_new_with_code ( GBytes *code )
{
  expr_cache_t *expr;

  expr = expr_cache_new();
  expr->code = g_bytes_ref(code);
  expr->invalid = TRUE;

  return expr;
}

void expr_update ( expr_cache_t **expr, GBytes *code )
{
  if(!*expr)
    *expr = expr_cache_new();
  g_clear_pointer(&((*expr)->code), g_bytes_unref);
  (*expr)->code = g_bytes_ref(code);
  (*expr)->invalid = TRUE;
}

expr_cache_t *expr_cache_ref ( expr_cache_t *expr )
{
  g_atomic_int_inc(&expr->refcount);
  return expr;
}

void expr_cache_unref ( expr_cache_t *expr )
{
  if(!expr)
    return;
  if(!g_atomic_int_dec_and_test(&expr->refcount))
    return;
  expr_dep_remove(expr);
  g_free(expr->definition);
  g_free(expr->cache);
  g_bytes_unref(expr->code);
  g_free(expr);
}

void expr_dep_add ( GQuark quark, expr_cache_t *expr )
{
  expr_dep_t *dep;

  if(!expr)
    return;

  if( !(dep = datalist_get(expr_deps, quark)) )
  {
    dep = g_malloc0(sizeof(expr_dep_t));
    datalist_set(expr_deps, quark, dep);
  }

  g_mutex_lock(&dep->mutex);
  if(!g_list_find(dep->list, expr))
    dep->list = g_list_prepend(dep->list, expr);
  g_mutex_unlock(&dep->mutex);
}

static void expr_dep_remove_func ( GQuark quark, expr_dep_t *dep,
    expr_cache_t *expr )
{
  g_mutex_lock(&dep->mutex);
  dep->list = g_list_remove(dep->list, expr);
  g_mutex_unlock(&dep->mutex);
}

void expr_dep_remove ( expr_cache_t *expr )
{
  datalist_foreach(expr_deps, (GDataForeachFunc)expr_dep_remove_func, expr);
}

void expr_dep_trigger ( GQuark quark )
{
  expr_dep_t *dep;
  GList *iter;

  if( !(dep = datalist_get(expr_deps, quark)) )
    return;

  g_mutex_lock(&dep->mutex);
  for(iter=dep->list; iter; iter=g_list_next(iter))
  {
    if(!EXPR_CACHE(iter->data)->invalid && EXPR_CACHE(iter->data)->quark)
      expr_dep_trigger(EXPR_CACHE(iter->data)->quark);
    EXPR_CACHE(iter->data)->invalid = TRUE;
  }
  g_mutex_unlock(&dep->mutex);
}

static void expr_dep_dump_each ( GQuark key, void *value, void *d )
{
  expr_dep_t *dep;
  GList *iter;

  if( !(dep=value) )
    return;
  g_mutex_lock(&dep->mutex);
  for(iter=dep->list; iter; iter=g_list_next(iter))
    g_message("%s: %s", g_quark_to_string(key),
        EXPR_CACHE(iter->data)->definition);
  g_mutex_unlock(&dep->mutex);
}

void expr_dep_dump ( void )
{
  datalist_foreach(expr_deps, expr_dep_dump_each, NULL);
}

void expr_init ( void )
{
  expr_deps = datalist_new();
}
