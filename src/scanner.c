/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <glib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <glob.h>
#include "config.h"
#include "client.h"
#include "util/json.h"
#include "util/string.h"
#include "vm/expr.h"

static GList *file_list;
static GHashTable *scan_list;
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
    guint type, gint flag )
{
  ScanVar *var, *old;

  if(!name)
    return;

  old = scan_list? g_hash_table_lookup(scan_list, name): NULL;

  if(old && type != G_TOKEN_SET && old->file != file)
    return;

  var = old? old: g_malloc0(sizeof(ScanVar));

  var->file = file;
  var->type = type;
  var->multi = flag;
  var->invalid = TRUE;

  switch(var->type)
  {
    case G_TOKEN_SET:
      expr_cache_free(var->expr);
      var->expr = expr_cache_new();
      expr_cache_set(var->expr, g_strdup(pattern));
      var->expr->eval = TRUE;
      var->vstate = 1;
      expr_dep_trigger(name);
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

  if(!scan_list)
    scan_list = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, (GDestroyNotify)scanner_var_free);

  if(!old)
  {
    g_hash_table_insert(scan_list, g_strdup(name), var);
    expr_dep_trigger(name);
  }
}

void scanner_var_invalidate ( void *key, ScanVar *var, void *data )
{
  if( !var->file || var->file->source != SO_CLIENT )
    var->invalid = TRUE;
}

/* expire all variables in the tree */
void scanner_invalidate ( void )
{
  if(scan_list)
    g_hash_table_foreach(scan_list,(GHFunc)scanner_var_invalidate,NULL);
}

void scanner_var_values_update ( ScanVar *var, gchar *value)
{
  if(!value)
    return;

  if(var->multi!=VT_FIRST || !var->count)
  {
    g_free(var->str);
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
  else if(glob(file->fname,GLOB_NOSORT,NULL,&gbuf))
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

gchar *scanner_parse_identifier ( gchar *id, gchar **fname )
{
  gchar *ptr;

  if(!id)
  {
    if(fname)
      *fname = NULL;
    return NULL;
  }

  if(*id=='$')
    id = id + sizeof(gchar);

  ptr=strchr(id,'.');
  if(fname)
    *fname = g_strdup(ptr?ptr:".val");
  if(ptr)
    return g_strndup(id,ptr-id);
  else
    return g_strdup(id);
}

ScanVar *scanner_var_update ( gchar *name, gboolean update, expr_cache_t *expr )
{
  ScanVar *var;

  if(!scan_list)
    return NULL;

  var = g_hash_table_lookup(scan_list, name);
  if(!var)
    return NULL;

  if(!update || !var->invalid)
  {
    expr->vstate = expr->vstate || var->vstate;
    return var;
  }

  if(var->type == G_TOKEN_SET)
  {
    if(!var->inuse)
    {
      var->inuse = TRUE;
      var->expr->parent = expr;
      (void)expr_cache_eval(var->expr);
      var->expr->parent = NULL;
      var->inuse = FALSE;
      var->vstate = var->expr->vstate;
      expr->vstate = expr->vstate || var->expr->vstate;
      scanner_var_reset(var,NULL);
      scanner_var_values_update(var,g_strdup(var->expr->cache));
      var->invalid = FALSE;
    }
  }
  else
  {
    scanner_file_glob(var->file);
    expr->vstate = TRUE;
    var->vstate = TRUE;
  }

  return var;
}

/* get value of a variable by name */
value_t scanner_get_value ( gchar *ident, gboolean update, expr_cache_t *expr )
{
  value_t result;
  ScanVar *var;
  gchar *fname, *id;

  id = scanner_parse_identifier(ident,&fname);
  var = scanner_var_update(id, update, expr);
  g_free(id);

  result.type = EXPR_TYPE_NA;

  if(!var)
  {
    g_free(fname);
    expr_dep_add(ident, expr);
    return result;
  }
  if(var->type == G_TOKEN_SET)
    expr_dep_add(ident, expr);

  if(*ident == '$')
  {
    if(var->str)
    {
      result.type = EXPR_TYPE_STRING;
      result.value.string  = g_strdup(var->str);
    }
  }
  else
  {
    result.type = EXPR_TYPE_NUMERIC;
    if(!g_strcmp0(fname,".val"))
      result.value.numeric = var->val;
    else if(!g_strcmp0(fname,".pval"))
      result.value.numeric = var->pval;
    else if(!g_strcmp0(fname,".count"))
      result.value.numeric = var->count;
    else if(!g_strcmp0(fname,".time"))
      result.value.numeric = var->time;
    else if(!g_strcmp0(fname,".age"))
      result.value.numeric = (g_get_monotonic_time() - var->ptime);
    else
      result.type = EXPR_TYPE_NA;
  }

  g_free(fname);
  if(result.type == EXPR_TYPE_NUMERIC)
    g_debug("scanner: %s = %f (vstate: %d)", ident, result.value.numeric,
        expr->vstate);
  else if(result.type == EXPR_TYPE_STRING)
    g_debug("scanner: %s = %s (vstate: %d)", ident, result.value.string,
        expr->vstate);
  return result;
}

gboolean scanner_is_variable ( gchar *identifier )
{
  gchar *name;
  gboolean result;

  if(!scan_list)
    return FALSE;

  name = scanner_parse_identifier(identifier,NULL);
  result = (g_hash_table_lookup(scan_list, name)!=NULL);
  g_free(name);

  return result;
}
