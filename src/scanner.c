/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <glob.h>
#include "sfwbar.h"
#include "expr.h"
#include "config.h"

static GList *file_list;
static GHashTable *scan_list;
static GHashTable *trigger_list;

void scanner_file_attach ( gchar *trigger, ScanFile *file )
{
  if(!trigger_list)
    trigger_list = g_hash_table_new((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal);

  g_hash_table_insert(trigger_list,trigger,file);
}

ScanFile *scanner_file_get ( gchar *trigger )
{
  if(!trigger_list)
    return NULL;

  return g_hash_table_lookup(trigger_list,trigger);
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

  if(g_strcmp0(file->trigger,trigger))
  {
    if(file->trigger)
      g_hash_table_remove(trigger_list,file->trigger);
    g_free(file->trigger);
    file->trigger = trigger;
    if(file->trigger)
      scanner_file_attach(file->trigger,file);
  }
  else
    g_free(trigger);

  return file;
}

void scanner_var_free ( ScanVar *var )
{
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
  ScanVar *var = g_malloc0(sizeof(ScanVar));

  var->file = file;
  var->type = type;
  var->multi = flag;
  var->invalid = TRUE;

  switch(var->type)
  {
    case G_TOKEN_SET:
      var->expr = expr_cache_new();
      var->expr->definition = pattern;
      var->expr->eval = TRUE;
      break;
    case G_TOKEN_JSON:
      var->definition = pattern;
      break;
    case G_TOKEN_REGEX:
      var->definition = g_regex_new(pattern,0,0,NULL);
      g_free(pattern);
      break;
    default:
      g_free(pattern);
      break;
  }

  if(file)
    file->vars = g_list_append(file->vars,var);

  if(!scan_list)
    scan_list = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal,g_free,(GDestroyNotify)scanner_var_free);

  g_hash_table_insert(scan_list,name,var);
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

  if(var->multi!=G_TOKEN_FIRST || !var->count)
  {
    g_free(var->str);
    var->str = value;
    switch(var->multi)
    {
      case G_TOKEN_SUM:
        var->val += g_ascii_strtod(var->str,NULL);
        break;
      case G_TOKEN_PRODUCT:
        var->val *= g_ascii_strtod(var->str,NULL);
        break;
      case G_TOKEN_LASTW:
        var->val = g_ascii_strtod(var->str,NULL);
        break;
      case G_TOKEN_FIRST:
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
void scanner_file_update ( GIOChannel *in, ScanFile *file )
{
  ScanVar *var;
  GList *node;
  GMatchInfo *match;
  struct json_tokener *json = NULL;
  struct json_object *obj;
  gchar *read_buff;
  GIOStatus status;

  while((status = g_io_channel_read_line(in,&read_buff,NULL,NULL,NULL))
      ==G_IO_STATUS_NORMAL)
  {
    for(node=file->vars;node!=NULL;node=g_list_next(node))
    {
      var=node->data;
      switch(var->type)
      {
        case G_TOKEN_REGEX:
          g_regex_match (var->definition, read_buff, 0, &match);
          if(g_match_info_matches (match))
            scanner_var_values_update(var,g_match_info_fetch (match, 1));
          g_match_info_free (match);
          break;
        case G_TOKEN_GRAB:
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
    ((ScanVar *)node->data)->invalid = FALSE;

  g_debug("channel status %d",status);
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

/* update all variables in a file (by glob) */
gboolean scanner_file_glob ( ScanFile *file )
{
  glob_t gbuf;
  gchar *dnames[2];
  struct stat stattr;
  gint i;
  FILE *in;
  GIOChannel *chan;
  gboolean reset=FALSE;

  if(!file)
    return FALSE;
  if(file->source == SO_CLIENT || !file->fname)
    return FALSE;
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
      if(file->source == SO_EXEC)
        in = popen(gbuf.gl_pathv[i],"r");
      else
        in = fopen(gbuf.gl_pathv[i],"rt");
      if(in)
      {
        if(!reset)
        {
          reset=TRUE;
          g_list_foreach(file->vars,(GFunc)scanner_var_reset,NULL);
        }

        chan = g_io_channel_unix_new(fileno(in));
        scanner_file_update(chan,file);
        g_io_channel_unref(chan);

        if(file->source == SO_EXEC)
          pclose(in);
        else
        {
          fclose(in);
          if(!stat(gbuf.gl_pathv[i],&stattr))
            file->mtime = stattr.st_mtime;
        }
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

ScanVar *scanner_var_update ( gchar *name, gboolean update, guint *vcount )
{
  ScanVar *var;

  if(!scan_list)
    return NULL;

  var = g_hash_table_lookup(scan_list, name);
  if(!var)
    return NULL;

  if(!update || !var->invalid)
  {
    *vcount += var->vcount;
    return var;
  }

  if(var->type == G_TOKEN_SET)
  {
    if(!var->inuse)
    {
      var->inuse = TRUE;
      (void)expr_cache_eval(var->expr);
      var->inuse = FALSE;
      var->vcount = var->expr->vcount;
      *vcount += var->expr->vcount;
      scanner_var_reset(var,NULL);
      scanner_var_values_update(var,g_strdup(var->expr->cache));
      var->invalid = FALSE;
    }
  }
  else
  {
    scanner_file_glob(var->file);
    var->vcount = ++(*vcount);
  }

  return var;
}

/* get string value of a variable by name */
gchar *scanner_get_string ( gchar *name, gboolean update, guint *vcount )
{
  ScanVar *var;
  gchar *id,*res;

  id = scanner_parse_identifier(name,NULL);
  var = scanner_var_update(id, update,vcount);
  g_free(id);

  if(var)
    res = g_strdup(var->str);
  else
    res = g_strdup("");

  g_debug("scanner: %s = \"%s\"",name,res);
  g_debug("count = %d",*vcount);
  return res;
}

/* get numeric value of a variable by name */
double scanner_get_numeric ( gchar *name, gboolean update, guint *vcount )
{
  ScanVar *var;
  double retval;
  gchar *fname,*id;

  id = scanner_parse_identifier(name,&fname);
  var = scanner_var_update(id, update, vcount);
  g_free(id);

  if(var)
  {
    if(!g_strcmp0(fname,".val"))
      retval = var->val;
    else if(!g_strcmp0(fname,".pval"))
      retval = var->pval;
    else if(!g_strcmp0(fname,".count"))
      retval = var->count;
    else if(!g_strcmp0(fname,".time"))
      retval = var->time;
    else if(!g_strcmp0(fname,".age"))
      retval = (g_get_monotonic_time() - var->ptime);
    else
      retval = 0;
  }
  else
    retval = 0;
  g_free(fname);
  g_debug("scanner: %s = %f",name,retval);
  return retval;
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
