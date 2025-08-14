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
#include "util/json.h"
#include "util/string.h"
#include "vm/expr.h"

static GList *file_list;
static GData *scan_list;
static GHashTable *trigger_list;

void scanner_file_attach ( const gchar *trigger, ScanFile *file )
{
  if(!trigger_list)
    trigger_list = g_hash_table_new(g_direct_hash, g_direct_equal);

  g_hash_table_insert(trigger_list, (void *)g_intern_string(trigger), file);
}

ScanFile *scanner_file_get ( gchar *trigger )
{
  if(!trigger_list)
    return NULL;

  return g_hash_table_lookup(trigger_list, (void *)g_intern_string(trigger));
}

void scanner_file_merge ( ScanFile *keep, ScanFile *temp )
{
  GList *iter;

  file_list = g_list_remove(file_list, temp);

  for(iter=temp->vars; iter; iter=g_list_next(iter))
    ((ScanVar *)(iter->data))->file = keep;
  keep->vars = g_list_concat(keep->vars, temp->vars);

  g_free(temp->fname);
  g_free(temp);
}

ScanFile *scanner_file_new ( gint source, gchar *fname,
    gchar *trigger, gint flags )
{
  ScanFile *file;
  GList *iter;

  if(source == SO_CLIENT)
    iter = NULL;
  else
    for(iter=file_list;iter;iter=g_list_next(iter))
      if(!g_strcmp0(fname,((ScanFile *)(iter->data))->fname))
        break;

  if(iter)
  {
    file = iter->data;
    g_free(fname);
  }
  else
  {
    file = g_malloc0(sizeof(ScanFile));
    file_list = g_list_append(file_list,file);
    file->fname = fname;
  }

  file->source = source;
  file->flags = flags;
  if( !strchr(file->fname,'*') && !strchr(file->fname,'?') )
    file->flags |= VF_NOGLOB;

  if(file->trigger != g_intern_string(trigger))
  {
    if(file->trigger)
      g_hash_table_remove(trigger_list, file->trigger);
    file->trigger = g_intern_string(trigger);
    if(file->trigger)
      scanner_file_attach(file->trigger, file);
  }
  g_free(trigger);

  return file;
}

void scanner_var_free ( ScanVar *var )
{
  if(var->file)
    var->file->vars = g_list_remove(var->file->vars,var);
  if(var->type != G_TOKEN_REGEX)
    g_free(var->definition);
  else
    if(var->definition)
      g_regex_unref(var->definition);
  expr_cache_free(var->expr);
  g_free(var->str);
  g_free(var);
}

void scanner_var_new ( gchar *name, ScanFile *file, gchar *pattern,
    guint type, gint flag, vm_store_t *store )
{
  ScanVar *var, *old;
  GQuark quark;

  if(!name)
    return;

  quark = scanner_parse_identifier(name, NULL);

  old = g_datalist_id_get_data(&scan_list, quark);
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

  switch(var->type)
  {
    case G_TOKEN_SET:
      expr_cache_free(var->expr);
      var->expr = expr_cache_new_with_code((GBytes *)pattern);
      if(var->expr)
        var->expr->store = store;
      g_debug("scanner: new set: '%s' in %p", g_quark_to_string(quark),
          var->expr? var->expr->store : NULL);
      var->vstate = 1;
      expr_dep_trigger(quark);
      break;
    case G_TOKEN_JSON:
      g_free(var->definition);
      var->definition = g_strdup(pattern);
      break;
    case G_TOKEN_REGEX:
      if(var->definition)
        g_regex_unref(var->definition);
      var->definition = g_regex_new(pattern, 0, 0, NULL);
      break;
  }

  if(file && !old)
    file->vars = g_list_append(file->vars, var);

  if(!old)
  {
    g_datalist_id_set_data(&scan_list, quark, var);
    expr_dep_trigger(quark);
  }
}

void scanner_var_invalidate ( GQuark key, ScanVar *var, void *data )
{
  if( !var->file || var->file->source != SO_CLIENT )
    var->invalid = TRUE;
}

/* expire all variables in the tree */
void scanner_invalidate ( void )
{
  g_datalist_foreach(&scan_list, (GDataForeachFunc)scanner_var_invalidate,
      NULL);
}

static void scanner_var_values_update ( ScanVar *var, gchar *value)
{
  if(!value)
    return;

  if(var->multi!=VT_FIRST || !var->count || var->type==G_TOKEN_SET)
  {
    g_clear_pointer(&var->str, g_free);
    var->str = value;
    switch(var->multi)
    {
      case VT_SUM:
        var->val += g_ascii_strtod(var->str,NULL);
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

  for(node=file->vars;node!=NULL;node=g_list_next(node))
  {
    ptr = jpath_parse(((ScanVar *)node->data)->definition,obj);
    if(ptr && json_object_is_type(ptr, json_type_array))
      for(i=0;i<json_object_array_length(ptr);i++)
      {
        scanner_var_values_update(((ScanVar *)node->data),
          g_strdup(json_object_get_string(json_object_array_get_idx(ptr,i))));
      }
    if(ptr)
      json_object_put(ptr);
  }
}

/* update variables in a specific file (or pipe) */
GIOStatus scanner_file_update ( GIOChannel *in, ScanFile *file, gsize *size )
{
  ScanVar *var;
  GList *node;
  GMatchInfo *match;
  struct json_tokener *json = NULL;
  struct json_object *obj;
  gchar *read_buff;
  GIOStatus status;
  gsize lsize;

  if(size)
    *size = 0;

  while((status = g_io_channel_read_line(in,&read_buff,&lsize,NULL,NULL))
      ==G_IO_STATUS_NORMAL)
  {
    if(size)
      *size += lsize;
    for(node=file->vars;node!=NULL;node=g_list_next(node))
    {
      var=node->data;
      switch(var->type)
      {
        case G_TOKEN_REGEX:
          if(var->definition &&
              g_regex_match (var->definition, read_buff, 0, &match))
            scanner_var_values_update(var,g_match_info_fetch (match, 1));
          if(match)
            g_match_info_free (match);
          break;
        case G_TOKEN_GRAB:
          if(lsize>0 && *(read_buff+lsize-1)=='\n')
            *(read_buff+lsize-1)='\0';
          scanner_var_values_update(var,g_strdup(read_buff));
          break;
        case G_TOKEN_JSON:
          if(!json)
            json = json_tokener_new();
          break;
      }
    }
    if(json)
      obj = json_tokener_parse_ex(json,read_buff,
          strlen(read_buff));
    g_free(read_buff);
  }
  g_free(read_buff);

  if(json)
  {
    scanner_update_json(obj,file);
    json_object_put(obj);
    json_tokener_free(json);
  }

  for(node=file->vars;node!=NULL;node=g_list_next(node))
  {
    ((ScanVar *)node->data)->invalid = FALSE;
    ((ScanVar *)node->data)->vstate = TRUE;
  }

  g_debug("channel status %d, (%s)",status,file->fname?file->fname:"(null)");

  return status;
}

void scanner_var_reset ( ScanVar *var, gpointer dummy )
{
  gint64 tv = g_get_monotonic_time();

  var->pval = var->val;
  var->count = 0;
  var->val = 0;
  var->time = tv-var->ptime;
  var->ptime = tv;
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

  if(!g_spawn_async_with_pipes(NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,
        NULL, NULL, &out, NULL, NULL))
    return FALSE;

  chan = g_io_channel_unix_new(out);
  if(chan)
  {
    g_debug("scanner: exec '%s'",file->fname);
    g_list_foreach(file->vars,(GFunc)scanner_var_reset,NULL);
    (void)scanner_file_update(chan,file,NULL);
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

  if(!file)
    return FALSE;
  if(file->source == SO_CLIENT || !file->fname)
    return FALSE;
  if(file->source == SO_EXEC)
    return scanner_file_exec(file);
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
        (void)scanner_file_update(chan,file,NULL);
        g_io_channel_unref(chan);

        close(in);
        if(!stat(gbuf.gl_pathv[i],&stattr))
          file->mtime = stattr.st_mtime;
      }
    }

  if(!(file->flags & VF_NOGLOB)&&(file->source == SO_FILE))
    globfree(&gbuf);

  return TRUE;
}

GQuark scanner_parse_identifier ( const gchar *id, guint8 *dtype )
{
  gchar *ptr, *lower, type;
  GQuark quark;

  if(!id)
    return (GQuark)0;

  type = (*id=='$')? SCANNER_TYPE_STR : SCANNER_TYPE_NONE;

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

static ScanVar *scanner_var_update ( GQuark id, gboolean update,
    expr_cache_t *expr )
{
  ScanVar *var;

  if(!(var = g_datalist_id_get_data(&scan_list, id)) )
    return NULL;

  if(!update || (!var->invalid && var->type != G_TOKEN_SET))
  {
    if(expr)
      expr->vstate = expr->vstate || var->vstate;
    return var;
  }

  if(var->type == G_TOKEN_SET)
  {
    if(!g_rec_mutex_trylock(&var->mutex))
      g_rec_mutex_lock(&var->mutex);
    else
    {
      if(!var->updating)
      {
        var->updating = TRUE;
        var->expr->parent = expr;
        (void)vm_expr_eval(var->expr);
        var->expr->parent = NULL;
        var->vstate = var->expr->vstate;
        if(expr)
          expr->vstate = expr->vstate || var->expr->vstate;
        if(var->invalid)
          scanner_var_reset(var, NULL);
        scanner_var_values_update(var, g_strdup(var->expr->cache));
        var->invalid = FALSE;
        var->updating = FALSE;
      }
    }
    if(expr)
      expr->vstate = expr->vstate || var->expr->vstate;
    g_rec_mutex_unlock(&var->mutex);
  }
  else
  {
    scanner_file_glob(var->file);
    if(expr)
      expr->vstate = TRUE;
    var->vstate = TRUE;
  }

  return var;
}

/* get value of a variable by name */
value_t scanner_get_value ( GQuark id, gchar ftype, gboolean update,
    expr_cache_t *expr )
{
  value_t result;
  ScanVar *var;

  if( !(var = scanner_var_update(id, update, expr)) )
  {
    expr_dep_add(id, expr);
    return value_na;
  }
  if(var->type == G_TOKEN_SET)
    expr_dep_add(id, expr);

  if(ftype == SCANNER_TYPE_STR)
    result = value_new_string(g_strdup(var->str));
  else
  {
    if(ftype == SCANNER_TYPE_VAL || ftype == SCANNER_TYPE_NONE)
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
  }

  if(result.type == EXPR_TYPE_NUMERIC)
    g_debug("scanner: %s = %f (vstate: %d)", g_quark_to_string(id),
        result.value.numeric, (expr?  expr->vstate: 0));
  else if(result.type == EXPR_TYPE_STRING)
    g_debug("scanner: %s = %s (vstate: %d)", g_quark_to_string(id),
        result.value.string, (expr?  expr->vstate: 0));
  return result;
}

gboolean scanner_is_variable ( gchar *identifier )
{
  return !!g_datalist_id_get_data(&scan_list,
      scanner_parse_identifier(identifier, NULL));
}
