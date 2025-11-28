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

ScanFile *scanner_file_new ( gint source, gchar *fname, gint flags )
{
  static hash_table_t *file_list;
  ScanFile *file;

  if(!fname || !(file = hash_table_lookup(file_list, fname)) )
  {
    file = g_malloc0(sizeof(ScanFile));
    file->fname = g_strdup(fname);
    if(!file_list)
      file_list = hash_table_new(g_direct_hash, g_direct_equal);
    hash_table_insert(file_list, file->fname, file);
  }

  file->source = source;
  file->flags = flags;
  if( !strchr(file->fname,'*') && !strchr(file->fname, '?') )
    file->flags |= VF_NOGLOB;

  return file;
}

void scanner_var_new ( gchar *name, ScanFile *file, gchar *pattern,
    guint type, gint flag, vm_store_t *store )
{
  ScanVar *var, *old;
  GQuark quark;

  if(!name)
    return;

  quark = scanner_parse_identifier(name, NULL);

  old = datalist_get(scan_list, quark);
  if(old && (type != G_TOKEN_SET || old->type != G_TOKEN_SET) &&
      (old->file != file))
  {
    g_debug("scanner: variable '%s' redeclared in a different file", name);
    return;
  }

  var = old? old: g_malloc0(sizeof(ScanVar));

  var->file = file;
  var->type = type;
  var->multi = flag;
  var->invalid = TRUE;
  var->store = store;
  var->id = quark;

  switch(var->type)
  {
    case G_TOKEN_SET:
      expr_cache_unref(var->expr);
      var->expr = expr_cache_new_with_code((GBytes *)pattern);
      var->expr->store = store;
      g_debug("scanner: new set: '%s' in %p", g_quark_to_string(quark),
          var->expr->store);
      var->vstate = TRUE;
      var->expr->quark = quark;
      expr_dep_trigger(quark);
      break;
    case G_TOKEN_JSON:
      str_assign((gchar **)&var->definition, g_strdup(pattern));
      break;
    case G_TOKEN_REGEX:
      if(var->definition)
        g_regex_unref(var->definition);
      var->definition = g_regex_new(pattern, 0, 0, NULL);
      break;
  }

  if(file && !old)
  {
    g_mutex_lock(&file->mutex);
    file->vars = g_list_append(file->vars, var);
    g_mutex_unlock(&file->mutex);
  }

  if(!old)
  {
    datalist_set(scan_list, quark, var);
    expr_dep_trigger(quark);
  }
}

void scanner_var_invalidate ( GQuark key, ScanVar *var, void *data )
{
  if( var->file && var->file->source == SO_CLIENT )
    return;
  var->invalid = TRUE;
  if(var->file)
    var->file->invalid = TRUE;
}

/* expire all variables in the tree */
void scanner_invalidate ( void )
{
  datalist_foreach(scan_list, (GDataForeachFunc)scanner_var_invalidate, NULL);
}

static void scanner_var_values_update ( ScanVar *var, gchar *value)
{
  if(!value)
    return;

  if(var->multi!=VT_FIRST || !var->count || var->type==G_TOKEN_SET)
  {
    str_assign(&var->str, value);
    switch(var->multi)
    {
      case VT_SUM:
        var->val += g_ascii_strtod(var->str, NULL);
        break;
      case VT_PROD:
        var->val *= g_ascii_strtod(var->str,NULL);
        break;
      case VT_LAST:
        var->val = g_ascii_strtod(var->str,NULL);
        break;
      case VT_FIRST:
        if(!var->count)
          var->val = g_ascii_strtod(var->str,NULL);
        break;
    }
    var->count++;
  }
  else
    g_free(value);

  var->invalid = FALSE;
}

void scanner_update_json ( struct json_object *obj, ScanFile *file )
{
  GList *node;
  struct json_object *ptr;
  gint i;

  g_mutex_lock(&file->mutex);
  for(node=file->vars; node!=NULL; node=g_list_next(node))
  {
    ptr = jpath_parse(((ScanVar *)node->data)->definition, obj);
    if(ptr && json_object_is_type(ptr, json_type_array))
      for(i=0; i<json_object_array_length(ptr); i++)
      {
        scanner_var_values_update(((ScanVar *)node->data),
          g_strdup(json_object_get_string(json_object_array_get_idx(ptr, i))));
      }
    if(ptr)
      json_object_put(ptr);
  }
  g_mutex_unlock(&file->mutex);
}

/* update variables in a specific file (or pipe) */
GIOStatus scanner_file_update ( GIOChannel *in, ScanFile *file, gsize *size )
{
  ScanVar *var;
  GList *node;
  GMatchInfo *match = NULL;
  struct json_tokener *json = NULL;
  struct json_object *obj;
  gchar *read_buff;
  GIOStatus status;
  gsize lsize;

  if(size)
    *size = 0;

  while( (status = g_io_channel_read_line(in, &read_buff, &lsize, NULL, NULL))
      ==G_IO_STATUS_NORMAL)
  {
    if(size)
      *size += lsize;
    for(node=file->vars; node!=NULL; node=g_list_next(node))
    {
      var=node->data;
      switch(var->type)
      {
        case G_TOKEN_REGEX:
          if(var->definition &&
              g_regex_match(var->definition, read_buff, 0, &match))
            scanner_var_values_update(var, g_match_info_fetch(match, 1));
          if(match)
            g_match_info_free(match);
          break;
        case G_TOKEN_GRAB:
          if(lsize>0 && *(read_buff+lsize-1)=='\n')
            *(read_buff+lsize-1)='\0';
          scanner_var_values_update(var, g_strdup(read_buff));
          break;
        case G_TOKEN_JSON:
          if(!json)
            json = json_tokener_new();
          break;
      }
    }
    if(json)
      obj = json_tokener_parse_ex(json, read_buff, strlen(read_buff));
    g_free(read_buff);
  }
  g_free(read_buff);

  if(json)
  {
    scanner_update_json(obj, file);
    json_object_put(obj);
    json_tokener_free(json);
  }

  for(node=file->vars; node!=NULL; node=g_list_next(node))
  {
    ((ScanVar *)node->data)->invalid = FALSE;
    ((ScanVar *)node->data)->vstate = TRUE;
  }

  g_debug("channel status %d, (%s)",status, file->fname? file->fname : "(null)");

  return status;
}

void scanner_var_reset ( ScanVar *var, gpointer dummy )
{
  gint64 t = g_get_monotonic_time();

  var->pval = var->val;
  var->count = 0;
  var->val = 0;
  var->time = t-var->ptime;
  var->ptime = t;
}

time_t scanner_file_mtime ( glob_t *gbuf )
{
  gint i;
  struct stat stattr;
  time_t res = 0;

  for(i=0;gbuf->gl_pathv[i]!=NULL;i++)
    if(!stat(gbuf->gl_pathv[i],&stattr))
      res = MAX(stattr.st_mtime, res);

  return res;
}

gboolean scanner_file_exec ( ScanFile *file )
{
  GIOChannel *chan;
  gint out;
  gchar **argv;

  if(!g_shell_parse_argv(file->fname, NULL, &argv, NULL))
    return FALSE;

  if(!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
        NULL, NULL, &out, NULL, NULL))
    return FALSE;

  if( (chan = g_io_channel_unix_new(out)) )
  {
    g_debug("scanner: exec '%s'", file->fname);
    g_list_foreach(file->vars, (GFunc)scanner_var_reset, NULL);
    (void)scanner_file_update(chan, file, NULL);
    g_io_channel_unref(chan);
  }
  close(out);

  return TRUE;
}

/* update all variables in a file (by glob) */
gboolean scanner_file_glob ( ScanFile *file )
{
  glob_t gbuf;
  gchar *dnames[2];
  struct stat stattr;
  gint i;
  gint in;
  GIOChannel *chan;
  gboolean reset=FALSE;

  g_return_val_if_fail(file, FALSE);
  g_return_val_if_fail(file->fname, FALSE);

  if(file->source == SO_CLIENT)
    return FALSE;
  if(file->source == SO_EXEC)
    return scanner_file_exec(file);
  if(!file->invalid)
    return FALSE;
  if((file->flags & VF_NOGLOB)||(file->source != SO_FILE))
  {
    dnames[0] = file->fname;
    dnames[1] = NULL;
    gbuf.gl_pathv = dnames;
  }
  else if(glob(file->fname, GLOB_NOSORT, NULL, &gbuf))
  {
    globfree(&gbuf);
    return FALSE;
  }

  if( !(file->flags & VF_CHTIME) || (file->mtime < scanner_file_mtime(&gbuf)) )
    for(i=0;gbuf.gl_pathv[i];i++)
    {
      in = open(gbuf.gl_pathv[i],O_RDONLY);
      if(in != -1)
      {
        if(!reset)
        {
          reset=TRUE;
          g_list_foreach(file->vars,(GFunc)scanner_var_reset,NULL);
        }

        chan = g_io_channel_unix_new(in);
        (void)scanner_file_update(chan, file, NULL);
        g_io_channel_unref(chan);

        close(in);
        if(!stat(gbuf.gl_pathv[i],&stattr))
          file->mtime = stattr.st_mtime;
      }
    }

  if(!(file->flags & VF_NOGLOB)&&(file->source == SO_FILE))
    globfree(&gbuf);

  file->invalid = FALSE;
  return TRUE;
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

static ScanVar *scanner_var_update ( ScanVar *var )
{
  g_rec_mutex_lock(&var->mutex);
  if(var->type == G_TOKEN_SET && !var->updating)
  {
    var->updating = TRUE;
    (void)vm_expr_eval(var->expr);
    var->vstate = var->expr->invalid;
    if(var->invalid)
      scanner_var_reset(var, NULL);
    scanner_var_values_update(var, g_strdup(var->expr->cache));
    var->invalid = FALSE;
    var->updating = FALSE;
  }
  else if(var->file)
  {
    if(g_mutex_trylock(&var->file->mutex))
      scanner_file_glob(var->file);
    else
      g_mutex_lock(&var->file->mutex);
    g_mutex_unlock(&var->file->mutex);
    var->vstate = TRUE;
  }
  g_rec_mutex_unlock(&var->mutex);

  return var;
}

/* get value of a variable by name */
value_t scanner_get_value ( GQuark id, gchar ftype, gboolean update,
    gboolean *vstate )
{
  value_t result;
  ScanVar *var;

  if( !(var = datalist_get(scan_list, id)) )
    return value_na;

  if(update && (var->invalid || var->type == G_TOKEN_SET))
    scanner_var_update(var);

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
