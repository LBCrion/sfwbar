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

gint scanner_file_comp ( struct scan_file *f, gchar *n )
{
  return g_strcmp0(f->fname,n);
}

void scanner_init ( struct context *context, const ucl_object_t *obj )
{
  const ucl_object_t *iter,*arr;
  ucl_object_iter_t *itp;
  int j;
  struct scan_file *file;
  GList *find;
  const char *flags[] = {"NoGlob","Exec","CheckTime","Json"};
  gchar *flist;
  arr = ucl_object_lookup(obj,"scanner");
  if( arr )
  {
    itp = ucl_object_iterate_new(arr);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
      if(ucl_object_key(iter)!=NULL)
      {
        find = g_list_find_custom(context->file_list,ucl_object_key(iter),
            (GCompareFunc)scanner_file_comp);
        if(find!=NULL)
          file = find->data;
        else
          file = g_malloc(sizeof(struct scan_file));  
        if(file!=NULL)
        {
          file->fname = g_strdup(ucl_object_key(iter));
          file->mod_time = 0;
          file->flags = 0;
          flist = ucl_string_by_name(iter,"flags");
          if(flist!=NULL)
            for(j=0;j<4;j++)
              if(g_strrstr(flist,flags[j])!=NULL)
                file->flags |= (1<<j);
          if(file->flags & VF_EXEC )
          {
            file->flags |= VF_NOGLOB;
            file->flags &= ~VF_CHTIME;
          }
          file->vars = scanner_add_vars(context, iter, file);
          context->file_list = g_list_append(context->file_list,file);
        }
    }
    ucl_object_iterate_free(itp);
  }
}

GList *scanner_add_vars( struct context *context, const ucl_object_t *obj, struct scan_file *file )
{
  struct scan_var *var;
  const ucl_object_t *iter;
  ucl_object_iter_t *itp;
  char *name, *match, *p;
  GRegex *regex=NULL;
  GList *vlist;
  int j;
  const char *flags[] = {"Add","Product","Replace","First"};
  gchar *flist;
  if(!obj)
    return NULL;

  vlist=NULL;
  itp = ucl_object_iterate_new(obj);

  while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
  {
    name = g_strdup(ucl_object_key(iter)); 
    match = (char *)ucl_object_tostring(iter);
    var = g_malloc(sizeof(struct scan_var));

    if((!(file->flags & VF_JSON))&&(match!=NULL))
      regex = g_regex_new(match,0,0,NULL);

    if((name!=NULL)&&(match!=NULL)&&(g_ascii_strcasecmp(name,"flags")!=0)&&
      (var!=NULL)&&((file->flags & VF_JSON)||(regex!=NULL)))
    {
      p = strchr(name,'.');
      if(p==NULL)
        var->var_type = 1;
      else
      {
        *p='\0';
        var->var_type = 0;
        var->multi = SV_REPLACE;
        flist = p+sizeof(char);
        for(j=0;j<4;j++)
          if(g_ascii_strcasecmp(flist,flags[j])==0)
            var->multi = j+1;
      }
      var->name = g_strdup(name);
      var->file = file;
      if(file->flags & VF_JSON)
        var->json = g_strdup(match);
      else
        var->regex = regex;
      var->val = 0;
      var->pval = 0;
      var->time = 0;
      var->ptime = 0;
      var->status = 0;
      var->str = NULL;
      vlist = g_list_append(vlist,var);
      context->scan_list = g_list_append(context->scan_list,var);
    }
    else
    {
      g_free(var);
      if(regex!=NULL)
        g_regex_unref (regex);
    }
  g_free(name);
  }
  ucl_object_iterate_free(itp);
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

void update_var_value ( struct scan_var *var, gchar *value)
{
  g_free(var->str);
  var->str=value;
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

int update_json_file ( struct context *context, FILE *in, GList *var_list )
{
  GList *node;
  struct scan_var *var;
  gchar *fdata,*temp;
  struct ucl_parser *parser;
  const ucl_object_t *obj,*ptr;

  fdata = g_strdup("");
  while((!feof(in))&&(!ferror(in)))
    if(fgets(context->read_buff,context->buff_len,in)!=NULL)
    {
      temp = g_strconcat(fdata,context->read_buff,NULL);
      g_free(fdata);
      fdata = temp;
    }

  parser = ucl_parser_new(0);
  ucl_parser_add_chunk( parser, (const unsigned char *) fdata, strlen(fdata));
  obj = ucl_parser_get_object(parser);
  temp = (gchar *)ucl_parser_get_error(parser);
  if(temp!=NULL)
    fprintf(stderr,"%s\n",temp);

  for (node=var_list; node!=NULL; node=g_list_next(node))
  {
    var = node->data;
    ptr = ucl_object_lookup_path_char(obj, var->json+sizeof(gchar),*(var->json));
    if(ptr!=NULL)
    {
      temp = (gchar *)ucl_object_tostring_forced(ptr);
      if(temp!=NULL)
        update_var_value(var,g_strdup(temp));
    }
  }

  ucl_object_unref((ucl_object_t *)obj);
  ucl_parser_free(parser);
  free(fdata);
  return 0;
}

/* update variables in a specific file (or pipe) */
int update_regex_file ( struct context *context, FILE *in, GList *var_list )
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
          update_var_value(var,g_match_info_fetch (match, 1));
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
  char update=0;
  FILE *in;
  glob_t gbuf;
  char *dnames[2];

  if(file==NULL)
    return -1;
  if(file->fname==NULL)
    return -1;
  if(file->flags & VF_NOGLOB)
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

  if(file->flags & VF_CHTIME)
  {
    for(i=0;gbuf.gl_pathv[i]!=NULL;i++)
    {
      if(stat(gbuf.gl_pathv[i],&stattr)!=0)
        stattr.st_mtime = 0;
      if(stattr.st_mtime>file->mod_time)
      {
        update=1;
        file->mod_time=stattr.st_mtime;
      }
    }
  }
  else
    update=1;

  for(i=0;gbuf.gl_pathv[i]!=NULL;i++)
    if(update)
    {
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
        if(file->flags & VF_JSON)
          update_json_file(context,in,file->vars);
        else
          update_regex_file(context,in,file->vars);
        if(file->flags & VF_EXEC)
          pclose(in);
        else
          fclose(in);
        if(stat(gbuf.gl_pathv[i],&stattr)==0)
          file->mod_time=stattr.st_mtime;
      }
    }
  if(!(file->flags & VF_NOGLOB))
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
