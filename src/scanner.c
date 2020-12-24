/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <glob.h>
#include "sfwbar.h"

void scanner_init ( struct context *context, json_object *obj )
{
  json_object *ptr,*iter,*arr;
  int i,j;
  struct scan_file *file;
  const char *flags[] = {"NoGlob","Exec","CheckTime"};
  gchar *flist;
  json_object_object_get_ex(obj,"scanner",&arr);
  if( json_object_is_type(arr, json_type_array))
    for(i=0;i<json_object_array_length(arr);i++)
    {
      iter = json_object_array_get_idx(arr,i);
      json_object_object_get_ex(iter,"name",&ptr);
      if(ptr!=NULL)
      {
        file = g_malloc(sizeof(struct scan_file));  
        if(file!=NULL)
        {
          file->fname = strdup(json_object_get_string(ptr));
          file->mod_time = 0;
          file->flags = 0;
          memset(file->md5,0,16);
          flist = json_string_by_name(iter,"flags");
          if(flist!=NULL)
            for(j=0;j<3;j++)
              if(g_strrstr(flist,flags[j])!=NULL)
                file->flags |= (1<<j);
          g_free(flist);
          json_object_object_get_ex(iter,"variables",&ptr);
          file->vars = scanner_add_vars(context, ptr, file);
          context->file_list = g_list_append(context->file_list,file);
        }
      }
    }
}

GList *scanner_add_vars( struct context *context, json_object *obj, struct scan_file *file )
{
  struct scan_var *var;
  json_object *iter,*ptr;
  char *name, *match;
  GRegex *regex;
  GList *vlist;
  int i,j,v;
  const char *flags[] = {"Add","Product","Replace","First"};
  gchar *flist;
  if(!json_object_is_type(obj, json_type_array))
    return NULL;

  vlist=NULL;

  for(i=0;i<json_object_array_length(obj);i++)
  {
    iter = json_object_array_get_idx(obj,i);

    v=1;
    if(json_object_object_get_ex(iter,"number",&ptr)==FALSE)
      json_object_object_get_ex(iter,"string",&ptr);
    else
      v=0;
    name = g_strdup(json_object_get_string(ptr));
    json_object_object_get_ex(iter,"value",&ptr);
    match = g_strdup(json_object_get_string(ptr));
    var = g_malloc(sizeof(struct scan_var)); 
    if(match!=NULL)
      regex = g_regex_new(match,0,0,NULL);

    if((name!=NULL)&&(match!=NULL)&&(var!=NULL)&&(regex!=NULL))
    {
      var->name = name;
      var->file = file;
      var->regex = regex;
      var->val = 0;
      var->pval = 0;
      var->time = 0;
      var->ptime = 0;
      var->status = 0;
      var->str = NULL;
      var->var_type = v;
      var->multi = SV_REPLACE;
      json_object_object_get_ex(iter,"flag",&ptr);
      flist=(gchar *)json_object_get_string(ptr);
        if(flist!=NULL)
          for(j=0;j<4;j++)
            if(g_strrstr(flist,flags[j])!=NULL)
              var->multi = j+1;
      vlist = g_list_append(vlist,var);
      context->scan_list = g_list_append(context->scan_list,var);

    }
    else
    {
      g_free(name);
      g_free(var);
      g_regex_unref (regex);
    }
  g_free(match);
  }
  return vlist;
}

/* expire all variables in the tree */
int scanner_expire ( GList *start )
  {
  GList *node;

  for(node=start;node!=NULL;node=g_list_next(node))
    SCAN_VAR(node->data)->status=0;

  return 0;
  }

/* update variables in a specific file (or pipe) */
int update_var_file ( struct context *context, FILE *in, GList *var_list )
  {
  struct scan_var *var;
  GList *node;
  GMatchInfo *match;
  while((!feof(in))&&(!ferror(in)))
    if(fgets(context->read_buff,context->buff_len,in)!=NULL)
      for(node=var_list;node!=NULL;node=g_list_next(node))
        {
        var=node->data;
        g_regex_match (var->regex, context->read_buff, 0, &match);
          if(g_match_info_matches (match))
          {
          g_free(var->str);
          var->str=g_match_info_fetch (match, 1);
          if(!var->var_type)
            switch(var->multi)
              {
              case SV_ADD: var->val+=g_ascii_strtod((char *)var->str,NULL); break;
              case SV_PRODUCT: var->val*=g_ascii_strtod((char *)var->str,NULL); break;
              case SV_REPLACE: var->val=g_ascii_strtod((char *)var->str,NULL); break;
              case SV_FIRST: if(var->count==0) var->val=g_ascii_strtod((char *)var->str,NULL); break;
              }
          var->count++;
          var->status=1;
          }
        g_match_info_free (match);
        }
  return 0;
  }


/* reset variables in a list */
int reset_var_list ( GList *var_list )
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

/* update all variables in a file (by glob) */
int update_var_files ( struct context *context, struct scan_file *file )
{
  struct stat stattr;
  int i;
  char reset=0;
  FILE *in;
  glob_t gbuf;
  char *dnames[2];

  if(file==NULL)
    return -1;
  if(file->fname==NULL)
    return -1;
  if(file->flags & (VF_EXEC | VF_NOGLOB))
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

  for(i=0;gbuf.gl_pathv[i]!=NULL;i++)
    {
    stat(gbuf.gl_pathv[i],&stattr);
    if((!(file->flags & VF_CHTIME))||(stattr.st_mtime>file->mod_time)||(file->flags & VF_EXEC))
    {
      file->mod_time=stattr.st_mtime;
      if(file->flags & VF_EXEC)
        in = popen(file->fname,"r");
      else
        in = fopen(gbuf.gl_pathv[i],"rt");
      if(in !=NULL )
      {
        if(!reset)
	{
	  reset=1;
	  reset_var_list(file->vars);
        }
        update_var_file(context,in,file->vars);
	fclose(in);
        stat(gbuf.gl_pathv[i],&stattr);
        file->mod_time=stattr.st_mtime;
      }
    }
  }
  if(!(file->flags & (VF_EXEC | VF_NOGLOB)))
    globfree(&gbuf);

  return 0;
}

/* get string value of a variable by name */
char *string_from_name ( struct context *context, char *name )
  {
  struct scan_var *scan;
  char *fname,*id,*res=NULL;

  id = parse_identifier(name,&fname);
  g_free(fname);
  if((scan=list_by_name(context->scan_list,id))!=NULL)
    {
    if(!scan->status)
      update_var_files(context,scan->file);
    res = g_strdup(scan->str);
    }
  if(res==NULL)
    res = g_strdup("");
  g_free(id);
  return res;
  }

/* get numeric value of a variable by name */
double numeric_from_name ( struct context *context, char *name )
  {
  struct scan_var *scan;
  double retval=0;
  char *fname,*id;

  id = parse_identifier(name,&fname);

  if((scan=list_by_name(context->scan_list,id))!=NULL)
    {
    if(!scan->status)
      update_var_files(context,scan->file);
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
  return retval;
  }

char *parse_identifier ( char *id, char **fname )
  {
  char *temp;
  char *ptr;
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
