/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <glob.h>
#include "sfwbar.h"
#include "config.h"

static GList *file_list;
static GHashTable *scan_list;
static GHashTable *trigger_list;

void scanner_file_attach ( gchar *trigger, scan_file_t *file )
{
  if(!trigger_list)
    trigger_list = g_hash_table_new((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal);

  g_hash_table_insert(trigger_list,trigger,file);
}

scan_file_t *scanner_file_get ( gchar *trigger )
{
  if(!trigger_list)
    return NULL;

  return g_hash_table_lookup(trigger_list,trigger);
}

scan_file_t *scanner_file_new ( gint source, gchar *fname,
    gchar *trigger, gint flags )
{
  scan_file_t *file;
  GList *iter;

  if(source == SO_CLIENT)
    iter = NULL;
  else
    for(iter=file_list;iter;iter=g_list_next(iter))
      if(!g_strcmp0(fname,((scan_file_t *)(iter->data))->fname))
        break;

  if(iter)
    file = iter->data;
  else
    file = g_malloc(sizeof(scan_file_t));

  file->fname = fname;
  file->trigger = trigger;
  file->source = source;
  file->mtime = 0;
  file->flags = flags;
  file->vars = NULL;
  if( !strchr(fname,'*') && !strchr(fname,'?') )
    file->flags |= VF_NOGLOB;
  file_list = g_list_append(file_list,file);
  if(file->trigger)
    scanner_file_attach(trigger,file);

  return file;
}

void scanner_var_attach ( gchar *name, scan_file_t *file, gchar *pattern,
    guint type, gint flag )
{
  scan_var_t *var = g_malloc0(sizeof(scan_var_t));

  var->file = file;
  var->type = type;
  var->multi = flag;

  switch(var->type)
  {
    case G_TOKEN_JSON:
      var->json = pattern;
      break;
    case G_TOKEN_REGEX:
      var->regex = g_regex_new(pattern,0,0,NULL);
      g_free(pattern);
      break;
    default:
      g_free(pattern);
      break;
  }

  file->vars = g_list_append(file->vars,var);

  if(!scan_list)
    scan_list = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

  g_hash_table_insert(scan_list,name,var);
}

void scanner_expire_var ( void *key, scan_var_t *var, void *data )
{
  if( var->file->source != SO_CLIENT )
    var->status=0;
}

/* expire all variables in the tree */
void scanner_expire ( void )
{
  if(scan_list)
    g_hash_table_foreach(scan_list,(GHFunc)scanner_expire_var,NULL);
}

void scanner_update_var ( scan_var_t *var, gchar *value)
{
  if(!value)
    return;
  if((var->multi!=G_TOKEN_FIRST)||(!var->count))
  {
    g_free(var->str);
    var->str=value;
  }
  else
    g_free(value);
  switch(var->multi)
  {
    case G_TOKEN_SUM:
      var->val+=g_ascii_strtod(value,NULL);
      break;
    case G_TOKEN_PRODUCT:
      var->val*=g_ascii_strtod(value,NULL);
      break;
    case G_TOKEN_LASTW:
      var->val=g_ascii_strtod(value,NULL);
      break;
    case G_TOKEN_FIRST:
      if(!var->count)
        var->val=g_ascii_strtod(value,NULL);
      break;
  }
  var->count++;
  var->status=1;
}

void scanner_update_json ( struct json_object *obj, scan_file_t *file )
{
  GList *node;
  struct json_object *ptr;
  gint i;

  for(node=file->vars;node!=NULL;node=g_list_next(node))
  {
    ptr = jpath_parse(((scan_var_t *)node->data)->json,obj);
    for(i=0;i<json_object_array_length(ptr);i++)
    {
      scanner_update_var(((scan_var_t *)node->data),
        g_strdup(json_object_get_string(json_object_array_get_idx(ptr,i))));
    }
    json_object_put(ptr);
  }
}

/* update variables in a specific file (or pipe) */
int scanner_update_file ( GIOChannel *in, scan_file_t *file )
{
  scan_var_t *var;
  GList *node;
  GMatchInfo *match;
  struct json_tokener *json = NULL;
  struct json_object *obj;
  gchar *read_buff;

  while(g_io_channel_read_line(in,&read_buff,NULL,NULL,NULL)==G_IO_STATUS_NORMAL)
  {
    for(node=file->vars;node!=NULL;node=g_list_next(node))
    {
      var=node->data;
      switch(var->type)
      {
        case G_TOKEN_REGEX:
          g_regex_match (var->regex, read_buff, 0, &match);
          if(g_match_info_matches (match))
            scanner_update_var(var,g_match_info_fetch (match, 1));
          g_match_info_free (match);
          break;
        case G_TOKEN_GRAB:
          scanner_update_var(var,g_strdup(read_buff));
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
    ((scan_var_t *)node->data)->status=1;

  return 0;
}

/* reset variables in a list */
int scanner_reset_vars ( GList *var_list )
{
  GList *node;
  gint64 tv = g_get_monotonic_time();
  for(node=var_list;node!=NULL;node=g_list_next(node))
    {
    ((scan_var_t *)node->data)->pval = ((scan_var_t *)node->data)->val;
    ((scan_var_t *)node->data)->count = 0;
    ((scan_var_t *)node->data)->val = 0;
    ((scan_var_t *)node->data)->time=tv-((scan_var_t *)node->data)->ptime;
    ((scan_var_t *)node->data)->ptime=tv;
    }
  return 0;
}

time_t scanner_file_mtime ( glob_t *gbuf )
{
  gint i;
  struct stat stattr;
  time_t res = 0;

  for(i=0;gbuf->gl_pathv[i]!=NULL;i++)
    if(stat(gbuf->gl_pathv[i],&stattr)==0)
      if(stattr.st_mtime>res)
        res = stattr.st_mtime;

  return res;
}

/* update all variables in a file (by glob) */
int scanner_update_file_glob ( scan_file_t *file )
{
  glob_t gbuf;
  gchar *dnames[2];
  struct stat stattr;
  gint i;
  FILE *in;
  GIOChannel *chan;
  gboolean reset=FALSE;

  if(file==NULL)
    return -1;
  if(file->source == SO_CLIENT)
    return -1;
  if(file->fname==NULL)
    return -1;
  if((file->flags & VF_NOGLOB)||(file->source != SO_FILE))
  {
    dnames[0] = file->fname;
    dnames[1] = NULL;
    gbuf.gl_pathv = dnames;
  }
  else
    if(glob(file->fname,GLOB_NOSORT,NULL,&gbuf))
    {
      globfree(&gbuf);
      return -1;
    }

  if( !(file->flags & VF_CHTIME) || (file->mtime < scanner_file_mtime(&gbuf)) )
    for(i=0;gbuf.gl_pathv[i]!=NULL;i++)
    {
      if(file->source == SO_EXEC)
        in = popen(gbuf.gl_pathv[i],"r");
      else
        in = fopen(gbuf.gl_pathv[i],"rt");
      if(in !=NULL )
      {
        if(!reset)
        {
          reset=TRUE;
          scanner_reset_vars(file->vars);
        }

        chan = g_io_channel_unix_new(fileno(in));
        scanner_update_file(chan,file);
        g_io_channel_unref(chan);

        if(file->source == SO_EXEC)
          pclose(in);
        else
        {
          fclose(in);
          if(stat(gbuf.gl_pathv[i],&stattr)==0)
            file->mtime=stattr.st_mtime;
        }
      }
    }

  if(!(file->flags & VF_NOGLOB)&&(file->source == SO_FILE))
    globfree(&gbuf);

  return 0;
}

char *scanner_parse_identifier ( gchar *id, gchar **fname )
{
  gchar *temp;
  gchar *ptr;
  if(id==NULL)
    {
    *fname=NULL;
    return NULL;
    }
  if(*id=='$')
    temp=g_strdup(id+sizeof(gchar));
  else
    temp=g_strdup(id);
  if((ptr=strchr(temp,'.'))==NULL)
    {
    *fname=g_strdup(".val");
    return temp;
    }
  *fname=g_strdup(ptr);
  *ptr='\0';
  return temp;
}

/* get string value of a variable by name */
char *scanner_get_string ( gchar *name, gboolean update )
{
  scan_var_t *scan;
  gchar *fname,*id,*res=NULL;

  id = scanner_parse_identifier(name,&fname);
  g_free(fname);
  if((scan=g_hash_table_lookup(scan_list,id))!=NULL)
    {
    if(!scan->status && update)
      scanner_update_file_glob(scan->file);
    res = g_strdup(scan->str);
    }
  if(res==NULL)
    res = g_strdup("");
  g_free(id);
  g_debug("scanner: %s = \"%s\"",name,res);
  return res;
}

/* get numeric value of a variable by name */
double scanner_get_numeric ( gchar *name, gboolean update )
{
  scan_var_t *scan;
  double retval=0;
  gchar *fname,*id;

  id = scanner_parse_identifier(name,&fname);

  if((scan=g_hash_table_lookup(scan_list,id))!=NULL)
    {
    if(!scan->status && update)
      scanner_update_file_glob(scan->file);
    if(!g_strcmp0(fname,".val"))
      retval=scan->val;
    else if(!g_strcmp0(fname,".pval"))
      retval=scan->pval;
    else if(!g_strcmp0(fname,".count"))
      retval=scan->count;
    else if(!g_strcmp0(fname,".time"))
      retval=scan->time;
    else if(!g_strcmp0(fname,".age"))
      retval=(g_get_monotonic_time() - scan->ptime);
    }
  g_free(id);
  g_free(fname);
  g_debug("scanner: %s = %f",name,retval);
  return retval;
}
