/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
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

/* expire all variables in the tree */
void scanner_expire ( void )
{
  GList *node;

  for(node=context->scan_list;node!=NULL;node=g_list_next(node))
    SCAN_VAR(node->data)->status=0;
}

void update_var_value ( struct scan_var *var, gchar *value)
{
  g_free(var->str);
  var->str=value;
  if(*(var->name) != '$')
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

int update_json_file ( FILE *in, GList *var_list )
{
  GList *node;
  struct scan_var *var;
  gchar *fdata,*temp;
  struct ucl_parser *parser;
  const ucl_object_t *obj,*ptr;
  const gint buff_step = 8192;
  gint buff_len=0,i;

  fdata = g_malloc(buff_step);
  while((!feof(in))&&(!ferror(in)))
  {
    i=fread(fdata+buff_len,1,buff_step,in);
    buff_len+=i;
    temp = g_malloc(buff_len+buff_step);
    memcpy(temp,fdata,buff_len);
    g_free(fdata);
    fdata = temp;
  }

  parser = ucl_parser_new(0);
  ucl_parser_add_chunk( parser, (const guchar *) fdata, buff_len);
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
int scanner_update_file ( FILE *in, struct scan_file *file )
{
  struct scan_var *var;
  GList *node;
  GMatchInfo *match;

  while((!feof(in))&&(!ferror(in)))
    if(fgets(context->read_buff,context->buff_len,in)!=NULL)
    {
      if(file->flags & VF_CONCUR)
        for(node=file->vars;node!=NULL;node=g_list_next(node))
        {
          var=node->data;
          if(var->type == VP_REGEX)
          {
            g_regex_match (var->regex, context->read_buff, 0, &match);
            if(g_match_info_matches (match))
              update_var_value(var,g_match_info_fetch (match, 1));
            g_match_info_free (match);
          }
          if(var->type == VP_GRAB)
            update_var_value(var,g_strdup(context->read_buff));
        }
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
  gchar reset=0;

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
          reset=1;
          reset_var_list(file->vars);
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

