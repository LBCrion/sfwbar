/* This entire file is licensed under GNU General Public License v3.0
*
* Copyright 2020- sfwbar maintainers
*/

#include <glib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <glob.h>
#include "client.h"
#include "config/config.h"
#include "util/datalist.h"
#include "util/hash.h"
#include "util/json.h"
#include "util/string.h"
#include "vm/expr.h"

static datalist_t *scan_list;

static gboolean scanner_handler_cmp ( src_handler_t *h1, src_handler_t *h2 )
{
  return h1->init != h2->init || h1->handle != h2->handle || 
    h1->finish != h2->finish;
}

static void scanner_source_handler_add ( source_t *src, src_handler_t *h )
{
  if(g_list_find_custom(src->handlers, h, (GCompareFunc)scanner_handler_cmp))
    return;
  src->handlers = g_list_prepend(src->handlers,
      g_memdup2(h, sizeof(src_handler_t)));
}

static void scanner_var_values_update ( scan_var_t *var, gchar *value )
{
  gint64 t = g_get_monotonic_time();

  if(!value)
    return;

  if(var->invalid)
  {
    var->pval = var->val;
    var->count = 0;
    var->val = 0;
    var->time = t-var->ptime;
    var->ptime = t;
  }
  if(var->multi!=VT_FIRST || !var->count)
  {
    str_assign(&var->str, value);
    switch(var->multi)
    {
      case VT_SUM:
        var->val += g_ascii_strtod(var->str, NULL);
        break;
      case VT_PROD:
        var->val *= g_ascii_strtod(var->str, NULL);
        break;
      case VT_LAST:
        var->val = g_ascii_strtod(var->str, NULL);
        break;
      case VT_FIRST:
        if(!var->count)
          var->val = g_ascii_strtod(var->str, NULL);
        break;
    }
    var->count++;
  }
  else
    g_free(value);

  var->invalid = FALSE;
}

static gchar *scanner_var_parse_regex ( scan_var_t *var, GString *str )
{
  GMatchInfo *match;
  gchar *value;

  g_return_val_if_fail(var->definition, NULL);
  value = g_regex_match_full(var->definition, str->str, str->len, 0, 0, &match,
      NULL) ?  g_match_info_fetch(match, 1) : NULL;
  if(match)
    g_match_info_free(match);
  return value;
}

static gchar *scanner_var_parse_grab ( scan_var_t *var, GString *str )
{
  return g_strndup(str->str, str->len - (str->str[str->len - 1]=='\n' ? 1:0));
}

static void scanner_var_parse_default ( source_t *src, src_handler_t *handler,
  GString *str )
{
  GList *iter;
  gchar *val;

  for(iter=src->vars; iter; iter=g_list_next(iter))
    if(SCAN_VAR(iter->data)->parse)
      if( (val = SCAN_VAR(iter->data)->parse(iter->data, str)) )
        scanner_var_values_update(iter->data, val);
}

static src_handler_t src_handler_default = {
  .handle = scanner_var_parse_default,
};

static void scanner_handler_json_init ( source_t *src, src_handler_t *hnd )
{
  hnd->data = json_tokener_new();
}

void scanner_update_json1 ( scan_var_t *var, struct json_object *obj )
{
  struct json_object *ptr;
  gint i;

  ptr = jpath_parse(var->definition, obj);
  if(ptr && json_object_is_type(ptr, json_type_array))
    for(i=0; i<json_object_array_length(ptr); i++)
      scanner_var_values_update(var,
        g_strdup(json_object_get_string(json_object_array_get_idx(ptr, i))));
  if(ptr)
    json_object_put(ptr);
}

void scanner_update_json( json_object *obj, source_t *src )
{
  g_list_foreach(src->vars, (GFunc)scanner_update_json1, obj);
}

static void scanner_handler_json_handle ( source_t *src, src_handler_t *hnd,
  GString *str )
{
  json_object *obj;
  json_tokener *tok = hnd->data;
  enum json_tokener_error err;
  GString *remainder;

  obj = json_tokener_parse_ex(tok, str->str, str->len);
  err = json_tokener_get_error(tok);
  if(err == json_tokener_continue)
    return;
  if(!obj || err != json_tokener_success)
  {
    json_tokener_reset(tok);
    return;
  }
  g_list_foreach(src->vars, (GFunc)scanner_update_json1, obj);
  json_object_put(obj);
  if(tok->char_offset < str->len)
  {
    remainder = g_string_new(str->str+tok->char_offset);
    scanner_handler_json_handle(src, hnd, remainder);
    g_string_free(remainder, TRUE);
  }
}

static void scanner_handler_json_finish ( source_t *src, src_handler_t *hnd )
{
  json_tokener_free(hnd->data);
}

static src_handler_t src_handler_json = {
  .init = scanner_handler_json_init,
  .handle = scanner_handler_json_handle,
  .finish = scanner_handler_json_finish,
};

static gboolean scanner_expr_update ( source_t *source )
{
  scan_var_t *var = source->vars->data;

  if(EXPR_SOURCE(source)->updating)
    return FALSE;

  EXPR_SOURCE(source)->updating = TRUE;
  (void)vm_expr_eval(EXPR_SOURCE(source)->expr);
  var->vstate = EXPR_SOURCE(source)->expr->invalid;
  scanner_var_values_update(var, g_strdup(EXPR_SOURCE(source)->expr->cache));
  var->invalid = FALSE;
  var->src->invalid = TRUE;
  EXPR_SOURCE(source)->updating = FALSE;

  return TRUE;
}

/* update variables in a specific source from a channel */
GIOStatus scanner_source_update ( GIOChannel *in, source_t *src, gsize *size )
{
  GIOStatus status;
  GString *str;
  GList *iter;

  if(size)
    *size = 0;

  for(iter=src->handlers; iter; iter=g_list_next(iter))
    if(SRC_HANDLER(iter->data)->init)
      SRC_HANDLER(iter->data)->init(src, iter->data);

  str = g_string_new(NULL);
  while( (status = g_io_channel_read_line_string(in, str, NULL, NULL))
      ==G_IO_STATUS_NORMAL)
  {
    if(size)
      *size += str->len;
    for(iter=src->handlers; iter; iter=g_list_next(iter))
      if(SRC_HANDLER(iter->data)->handle)
        SRC_HANDLER(iter->data)->handle(src, iter->data, str);
  }
  g_string_free(str, TRUE);

  for(iter=src->handlers; iter; iter=g_list_next(iter))
    if(SRC_HANDLER(iter->data)->finish)
      SRC_HANDLER(iter->data)->finish(src, iter->data);

  for(iter=src->vars; iter; iter=g_list_next(iter))
    ((scan_var_t *)iter->data)->invalid = FALSE;

  g_debug("scanner: input status %d, (%s)", status, src->fname? src->fname :
      "(null)");

  return status;
}

static time_t scanner_file_mtime ( glob_t *gbuf )
{
  gint i;
  struct stat stattr;
  time_t res = 0;

  for(i=0; gbuf->gl_pathv[i]; i++)
    if(!stat(gbuf->gl_pathv[i], &stattr))
      res = MAX(stattr.st_mtime, res);

  return res;
}

static gboolean scanner_exec_update ( source_t *src )
{
  GIOChannel *chan;
  gint out;
  gchar **argv;

  if(!g_shell_parse_argv(src->fname, NULL, &argv, NULL))
    return FALSE;

  if(!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
        NULL, NULL, &out, NULL, NULL))
    return FALSE;

  if( (chan = g_io_channel_unix_new(out)) )
  {
    g_debug("scanner: exec: '%s'", src->fname);
    (void)scanner_source_update(chan, src, NULL);
    g_io_channel_unref(chan);
    src->invalid = FALSE;
  }
  close(out);

  return TRUE;
}

/* update all variables in a file (by glob) */
gboolean scanner_file_update ( source_t *src )
{
  GIOChannel *chan;
  glob_t gbuf;
  gchar *dnames[2];
  time_t mtime;
  gint i;

  g_return_val_if_fail(src, FALSE);
  g_return_val_if_fail(src->fname, FALSE);

  if(FILE_SOURCE(src)->flags & VF_NOGLOB)
  {
    dnames[0] = src->fname;
    dnames[1] = NULL;
    gbuf.gl_pathv = dnames;
  }
  else if(glob(src->fname, GLOB_NOSORT, NULL, &gbuf))
  {
    globfree(&gbuf);
    return FALSE;
  }

  if(FILE_SOURCE(src)->flags & VF_CHTIME)
    mtime = scanner_file_mtime(&gbuf);
  if(!(FILE_SOURCE(src)->flags & VF_CHTIME) || (FILE_SOURCE(src)->mtime < mtime))
    for(i=0; gbuf.gl_pathv[i]; i++)
      if( (chan = g_io_channel_new_file(gbuf.gl_pathv[i], "r", NULL)) )
      {
        src->invalid = FALSE;
        (void)scanner_source_update(chan, src, NULL);
        g_io_channel_unref(chan);
        if(FILE_SOURCE(src)->flags & VF_CHTIME)
          FILE_SOURCE(src)->mtime = mtime;
      }

  if(!(FILE_SOURCE(src)->flags & VF_NOGLOB))
    globfree(&gbuf);

  return TRUE;
}

source_t *scanner_source_new ( gchar *fname )
{
  static hash_table_t *src_list;
  source_t *src;

  if(!fname || !(src = hash_table_lookup(src_list, fname)) )
  {
    src = g_malloc0(sizeof(source_t));
    src->fname = g_strdup(fname);
    if(!src_list)
      src_list = hash_table_new(g_direct_hash, g_direct_equal);
    if(src->fname)
      hash_table_insert(src_list, src->fname, src);
  }

  return src;
}

source_t *scanner_file_new ( gchar *fname, gint flags )
{
  source_t *src = scanner_source_new(fname);

  if(!src->data)
    src->data = g_malloc0(sizeof(file_source_t));

  FILE_SOURCE(src)->flags = flags;

  if(!strchr(src->fname, '*') && !strchr(src->fname, '?'))
    FILE_SOURCE(src)->flags |= VF_NOGLOB;

  src->update = scanner_file_update;

  return src;
}

source_t *scanner_exec_new ( gchar *fname )
{
  source_t *src = scanner_source_new(fname);

  src->update = scanner_exec_update;

  return src;
}

static scan_var_t *scanner_var_new ( gchar *name, source_t *src )
{
  scan_var_t *var;
  GQuark quark;

  g_return_val_if_fail(name, NULL);

  quark = scanner_parse_identifier(name, NULL);

  if( (var = datalist_get(scan_list, quark)) && src && var->src != src)
  {
    g_debug("scanner: variable '%s' redeclared in a different source", name);
    return NULL;
  }
  if(!var)
  {
    var = g_malloc0(sizeof(scan_var_t));
    var->src = src;
    var->invalid = TRUE;
    var->id = quark;
    var->vstate = TRUE;
  }
  return var;
}

static scan_var_t *scanner_var_attach ( scan_var_t *var )
{
  if(datalist_get(scan_list, var->id))
    return NULL;

  if(var->src)
  {
    g_rec_mutex_lock(&var->src->mutex);
    var->src->vars = g_list_append(var->src->vars, var);
    g_rec_mutex_unlock(&var->src->mutex);
  }

  datalist_set(scan_list, var->id, var);
  expr_dep_trigger(var->id);

  return var;
}

scan_var_t *scanner_var_new_calc ( gchar *name, source_t *src, gpointer code,
   vm_store_t *store )
{
  scan_var_t *var;
  if( !(var = scanner_var_new(name, src)) )
    return NULL;

  if(!var->src)
  {
    var->src = scanner_source_new(NULL);
    var->src->update = scanner_expr_update;
    var->src->vars = g_list_append(NULL, var);
    var->src->data = g_malloc0(sizeof(expr_source_t));
  }
  var->src->invalid = TRUE;

  expr_cache_unref(EXPR_SOURCE(var->src)->expr);
  EXPR_SOURCE(var->src)->expr = expr_cache_new_with_code((GBytes *)code);
  EXPR_SOURCE(var->src)->expr->store = store;
  g_debug("scanner: new set: '%s' in %p", g_quark_to_string(var->id),
      EXPR_SOURCE(var->src)->expr->store);
  var->multi = VT_LAST;
  expr_dep_trigger(var->id);

  return scanner_var_attach(var);
}

scan_var_t *scanner_var_new_regex ( gchar *name, source_t *src, void *regex )
{
  scan_var_t *var;
  if( !(var = scanner_var_new(name, src)) )
    return NULL;

  if(var->definition)
    g_regex_unref(var->definition);
  var->definition = g_regex_new(regex, 0, 0, NULL);
  var->parse = scanner_var_parse_regex;
  scanner_source_handler_add(var->src, &src_handler_default);

  return scanner_var_attach(var);
}

scan_var_t *scanner_var_new_json ( gchar *name, source_t *src, void *path )
{
  scan_var_t *var;
  if( !(var = scanner_var_new(name, src)) )
    return NULL;

  str_assign((gchar **)&var->definition, g_strdup(path));
  scanner_source_handler_add(var->src, &src_handler_json);

  return scanner_var_attach(var);
}

scan_var_t *scanner_var_new_grab ( gchar *name, source_t *src, void *dummy )
{
  scan_var_t *var;
  if( !(var = scanner_var_new(name, src)) )
    return NULL;
  var->parse = scanner_var_parse_grab;
  scanner_source_handler_add(var->src, &src_handler_default);

  return scanner_var_attach(var);
}

void scanner_var_invalidate ( GQuark key, scan_var_t *var, void *data )
{
  var->invalid = TRUE;
  if(var->src)
    var->src->invalid = TRUE;
}

/* expire all variables in the tree */
void scanner_invalidate ( void )
{
  datalist_foreach(scan_list, (GDataForeachFunc)scanner_var_invalidate, NULL);
}

GQuark scanner_parse_identifier ( const gchar *id, guint8 *dtype )
{
  gchar *ptr, *lower, type;
  GQuark quark;

  if(!id)
    return (GQuark)0;

  type = (*id=='$')? SCANNER_TYPE_STR : SCANNER_TYPE_VAL;

  if(*id=='$')
    id = id + sizeof(gchar);

  if( (ptr = strchr(id, '.')) )
    lower = g_ascii_strdown(id, ptr-id);
  else
    lower = g_ascii_strdown(id, -1);

  quark = g_quark_from_string(lower);
  g_free(lower);

  if(dtype && ptr && (type != SCANNER_TYPE_STR))
  {
    if(!g_ascii_strcasecmp(ptr, ".val"))
      type = SCANNER_TYPE_VAL;
    else if(!g_ascii_strcasecmp(ptr, ".pval"))
      type = SCANNER_TYPE_PVAL;
    else if(!g_ascii_strcasecmp(ptr, ".count"))
      type = SCANNER_TYPE_COUNT;
    else if(!g_ascii_strcasecmp(ptr, ".time"))
      type = SCANNER_TYPE_TIME;
    else if(!g_ascii_strcasecmp(ptr, ".age"))
      type = SCANNER_TYPE_AGE;
  }

  if(dtype)
    *dtype = type;

  return quark;
}

/* get value of a variable by name */
value_t scanner_get_value ( GQuark id, gchar ftype, gboolean update,
    gboolean *vstate )
{
  scan_var_t *var;
  value_t result;

  if( !(var = datalist_get(scan_list, id)) )
    return value_na;

  if(update && var->src && var->src->invalid)
  {
    if(var->src->update && g_rec_mutex_trylock(&var->src->mutex))
      var->src->update(var->src);
    else
      g_rec_mutex_lock(&var->src->mutex);
    g_rec_mutex_unlock(&var->src->mutex);
  }

  if(vstate)
    *vstate = *vstate || var->vstate;

  if(ftype == SCANNER_TYPE_STR)
    result = value_new_string(g_strdup(var->str));
  else if(ftype == SCANNER_TYPE_VAL)
    result = value_new_numeric(var->val);
  else if(ftype == SCANNER_TYPE_PVAL)
    result = value_new_numeric(var->pval);
  else if(ftype == SCANNER_TYPE_COUNT)
    result = value_new_numeric(var->count);
  else if(ftype == SCANNER_TYPE_TIME)
    result = value_new_numeric(var->time);
  else if(ftype == SCANNER_TYPE_AGE)
    result = value_new_numeric(g_get_monotonic_time() - var->ptime);
  else
    result = value_na;

  if(result.type == EXPR_TYPE_NUMERIC)
    g_debug("scanner: %s = %f (vstate: %d)", g_quark_to_string(id),
        result.value.numeric, vstate? *vstate: 0);
  else if(result.type == EXPR_TYPE_STRING)
    g_debug("scanner: %s = %s (vstate: %d)", g_quark_to_string(id),
        result.value.string, vstate? *vstate: 0);
  return result;
}

gboolean scanner_is_variable ( gchar *identifier )
{
  return !!datalist_get(scan_list, scanner_parse_identifier(identifier, NULL));
}

void scanner_init ( void )
{
  scan_list = datalist_new();
}
