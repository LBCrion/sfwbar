/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <glob.h>
#include "sfwbar.h"

static GHashTable *scan_list;

void scanner_var_attach ( gchar *name, struct scan_var *var )
{
  if(!scan_list)
    scan_list = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

  g_hash_table_insert(scan_list,name,var);
}

void scanner_expire_var ( void *key, struct scan_var *var, void *data )
{
  if( var->file->source != SO_CLIENT )
    var->status=0;
}

/* expire all variables in the tree */
void scanner_expire ( void )
{
  g_hash_table_foreach(scan_list,(GHFunc)scanner_expire_var,NULL);
}

void scanner_update_var ( struct scan_var *var, gchar *value)
{
  if(!value)
    return;
  if((var->multi!=SV_FIRST)||(!var->count))
  {
    g_free(var->str);
    var->str=value;
  }
  switch(var->multi)
  {
    case SV_ADD:
      var->val+=g_ascii_strtod(value,NULL);
      break;
    case SV_PRODUCT:
      var->val*=g_ascii_strtod(value,NULL);
      break;
    case SV_REPLACE:
      var->val=g_ascii_strtod(value,NULL);
      break;
    case SV_FIRST:
      if(!var->count)
        var->val=g_ascii_strtod(value,NULL);
      break;
  }
  var->count++;
  var->status=1;
}

void scanner_update_json ( struct json_object *obj, struct scan_file *file )
{
  GList *node;
  struct json_object *ptr;
  gint i;

  for(node=file->vars;node!=NULL;node=g_list_next(node))
  {
    ptr = jpath_parse(((struct scan_var *)node->data)->json,obj);
    for(i=0;i<json_object_array_length(ptr);i++)
    {
      scanner_update_var(((struct scan_var *)node->data),
        g_strdup(json_object_get_string(json_object_array_get_idx(ptr,i))));
    }
    json_object_put(ptr);
  }
}

/* update variables in a specific file (or pipe) */
int scanner_update_file ( GIOChannel *in, struct scan_file *file )
{
  struct scan_var *var;
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
        case VP_REGEX:
          g_regex_match (var->regex, read_buff, 0, &match);
          if(g_match_info_matches (match))
            scanner_update_var(var,g_match_info_fetch (match, 1));
          g_match_info_free (match);
          break;
        case VP_GRAB:
          scanner_update_var(var,g_strdup(read_buff));
          break;
        case VP_JSON:
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
    ((struct scan_var *)node->data)->status=1;

  return 0;
}

/* reset variables in a list */
int scanner_reset_vars ( GList *var_list )
{
  GList *node;
  gint64 tv = g_get_monotonic_time();
  for(node=var_list;node!=NULL;node=g_list_next(node))
    {
    ((struct scan_var *)node->data)->pval = ((struct scan_var *)node->data)->val;
    ((struct scan_var *)node->data)->count = 0;
    ((struct scan_var *)node->data)->val = 0;
    ((struct scan_var *)node->data)->time=tv-((struct scan_var *)node->data)->ptime;
    ((struct scan_var *)node->data)->ptime=tv;
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
int scanner_update_file_glob ( struct scan_file *file )
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
  struct scan_var *scan;
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
  struct scan_var *scan;
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
