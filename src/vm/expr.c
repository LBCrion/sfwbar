/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <glib.h>
#include "expr.h"
#include "scanner.h"
#include "wintree.h"
#include "module.h"
#include "vm/vm.h"
#include "util/string.h"

static GHashTable *expr_deps;

gboolean expr_cache_eval ( expr_cache_t *expr )
{
  value_t v1;
  gchar *eval;

  if(!expr || !expr->eval)
    return FALSE;

  expr->vstate = FALSE;
  v1 = vm_expr_eval(expr);
  if(v1.type==EXPR_TYPE_STRING)
    eval = v1.value.string;
  else if(v1.type==EXPR_TYPE_NUMERIC)
    eval = numeric_to_string(v1.value.numeric, -1);
  else
    eval = g_strdup("");

  if(!expr->vstate)
    expr->eval = FALSE;

  g_debug("expr: '%s' = '%s', vstate: %d", expr->definition, eval,
      expr->vstate);

  if(g_strcmp0(eval, expr->cache))
  {
    g_free(expr->cache);
    expr->cache = eval;
    return TRUE;
  }
  else
  {
    g_free(eval);
    return FALSE;
  }
}

expr_cache_t *expr_cache_new ( void )
{
  return g_malloc0(sizeof(expr_cache_t));
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

void expr_dep_add ( gchar *ident, expr_cache_t *expr )
{
  GList *list;
  expr_cache_t *iter;
  gchar *vname;

  if(!expr)
    return;

  if(!expr_deps)
    expr_deps = g_hash_table_new_full((GHashFunc)str_nhash,
          (GEqualFunc)str_nequal, g_free, NULL);

  vname = scanner_parse_identifier(ident, NULL);
  list = g_hash_table_lookup(expr_deps, vname);
  for(iter=expr; iter; iter=iter->parent)
    if(!g_list_find(list, iter))
      list = g_list_prepend(list, iter);
  g_hash_table_replace(expr_deps, vname, list);
}

void expr_dep_remove ( expr_cache_t *expr )
{
  GHashTableIter hiter;
  void *list, *key;

  if(!expr_deps)
    return;

  g_hash_table_iter_init(&hiter, expr_deps);
  while(g_hash_table_iter_next(&hiter, &key, &list))
    g_hash_table_iter_replace(&hiter, g_list_remove(list, expr));
}

void expr_dep_trigger ( gchar *ident )
{
  GList *iter,*list;

  if(!expr_deps)
    return;

  list = g_hash_table_lookup(expr_deps, ident);

  for(iter=list; iter; iter=g_list_next(iter))
    ((expr_cache_t *)(iter->data))->eval = TRUE;
}

void expr_dep_dump_each ( void *key, void *value, void *d )
{
  GList *iter;

  for(iter=value;iter;iter=g_list_next(iter))
    g_message("%s: %s", (gchar *)key, ((expr_cache_t *)iter->data)->definition);
}

void expr_dep_dump ( void )
{
  g_hash_table_foreach(expr_deps,expr_dep_dump_each,NULL);
}
