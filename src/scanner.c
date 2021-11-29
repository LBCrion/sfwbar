/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <glob.h>
#include "sfwbar.h"

/* expire all variables in the tree */
void scanner_expire ( void )
{
  GList *node;

  for(node=context->scan_list;node!=NULL;node=g_list_next(node))
    SCAN_VAR(node->data)->status=0;
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

gchar *scanner_extract_json ( struct json_object *obj, gchar *expr )
{
  struct json_object *iter;
  gchar **evect;
  gchar *result;
  gchar *sep;
  gint i,n;

  if(!expr || !obj)
    return NULL;

  if(strlen(expr)<2)
    return NULL;

  sep = g_strndup(expr,1);
  evect = g_strsplit(expr+1,sep,64);
  g_free(sep);
  iter = obj;

  for(i=0;evect[i];i++)
    if(iter)
    {
      n = g_ascii_strtoull(evect[i],&sep,10);
      if(*sep)
        json_object_object_get_ex(iter,evect[i],&iter);
      else
        if(json_object_is_type(iter,json_type_array))
          iter = json_object_array_get_idx(iter,n);
        else
          iter = NULL;
    }

  result = g_strdup(json_object_get_string(iter));

  g_strfreev(evect);

  return result;
}

#define SCANNER_BUFF_LEN 1024

/* update variables in a specific file (or pipe) */
int scanner_update_file ( FILE *in, struct scan_file *file )
{
  struct scan_var *var;
  GList *node;
  GMatchInfo *match;
  struct json_tokener *json = NULL;
  struct json_object *obj;
  static gchar read_buff[SCANNER_BUFF_LEN];

  while((!feof(in))&&(!ferror(in)))
    if(fgets(read_buff,SCANNER_BUFF_LEN,in)!=NULL)
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
    }
  if(json)
  {
    for(node=file->vars;node!=NULL;node=g_list_next(node))
      scanner_update_var(SCAN_VAR(node->data),
          scanner_extract_json(obj,SCAN_VAR(node->data)->json));
    json_object_put(obj);
    json_tokener_free(json);
  }
  for(node=file->vars;node!=NULL;node=g_list_next(node))
    SCAN_VAR(node->data)->status=1;
  return 0;
}


/* reset variables in a list */
int scanner_reset_vars ( GList *var_list )
{
  GList *node;
  gint tv = g_get_real_time();
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
  gboolean reset=FALSE;

  if(file==NULL)
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
        scanner_update_file(in,file);

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

/* get string value of a variable by name */
char *string_from_name ( gchar *name )
{
  struct scan_var *scan;
  gchar *fname,*id,*res=NULL;

  id = parse_identifier(name,&fname);
  g_free(fname);
  if((scan=list_by_name(context->scan_list,id))!=NULL)
    {
    if(!scan->status)
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
double numeric_from_name ( gchar *name )
{
  struct scan_var *scan;
  double retval=0;
  gchar *fname,*id;

  id = parse_identifier(name,&fname);

  if((scan=list_by_name(context->scan_list,id))!=NULL)
    {
    if(!scan->status)
      scanner_update_file_glob(scan->file);
    if(!strcmp(fname,".val"))
      retval=scan->val;
    if(!strcmp(fname,".pval"))
      retval=scan->pval;
    if(!strcmp(fname,".count"))
      retval=scan->count;
    if(!strcmp(fname,".time"))
      retval=scan->time;
    }
  g_free(id);
  g_free(fname);
  g_debug("scanner: %s = %f",name,retval);
  return retval;
}

char *parse_identifier ( gchar *id, gchar **fname )
{
  gchar *temp;
  gchar *ptr;
  if(id==NULL)
    {
    *fname=NULL;
    return NULL;
    }
  if(*id=='$')
    temp=g_strdup(id+sizeof(char));
  else
    temp=g_strdup(id);
  if((ptr=strchr(temp,'.'))==NULL)
    {
    *fname=g_strdup(".val");
    return temp;
    }
  *fname=g_strdup(ptr);
  *ptr='\0';
  ptr=g_strdup(temp);
  g_free(temp);
  return ptr;
}

/* get node by name from the list root */
void *list_by_name ( GList *prev, gchar *name )
{
  GList *node;
  for(node=prev;node!=NULL;node=g_list_next(node))
      if(!g_strcmp0(SCAN_VAR(node->data)->name,name))
        return node->data;
  return NULL;
}

