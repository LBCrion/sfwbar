/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <glib.h>
#include <unistd.h>
#include "sfwbar.h"

void placement_init ( struct context *context, const ucl_object_t *obj )
{
  const ucl_object_t *ptr;
  if((ptr=ucl_object_lookup(obj,"placement"))==NULL)
    return;
  context->features |= F_PLACEMENT;
  context->wp_x= ucl_int_by_name(ptr,"xcascade",10);
  context->wp_y= ucl_int_by_name(ptr,"ycascade",10);
  context->wo_x= ucl_int_by_name(ptr,"xorigin",0);
  context->wo_y= ucl_int_by_name(ptr,"yorigin",0);
  if(ucl_bool_by_name(ptr,"children",FALSE)==FALSE)
    context->features |= F_PL_CHKPID;
  if(context->wp_x<1)
    context->wp_x=1;
  if(context->wp_y<1)
    context->wp_y=1;
}

int comp_int ( const void *x1, const void *x2)
{
  return ( *(int*)x1 - *(int*)x2 );
}

struct rect parse_rect ( const ucl_object_t *obj )
{
  const ucl_object_t *rect;
  struct rect ret;
  rect = ucl_object_lookup(obj,"rect");
  ret.x = ucl_int_by_name(rect,"x",0);
  ret.y = ucl_int_by_name(rect,"y",0);
  ret.w = ucl_int_by_name(rect,"width",0);
  ret.h = ucl_int_by_name(rect,"height",0);

  return ret;
}

int placement_location ( struct context *context, const ucl_object_t *obj, gint64 wid, struct rect *r )
{
  struct rect output, win, *obs;
  const ucl_object_t *ptr,*iter,*arr;
  ucl_object_iter_t *itp;
  gint c,i,j,nobs,success;
  gint *x, *y;
  gint xoff,yoff;
  output = parse_rect(obj);

  arr = ucl_object_lookup(obj,"floating_nodes");
  if( arr == NULL )
    return -1;
  nobs = ucl_array_size(arr)-1;
  obs = g_malloc(nobs*sizeof(struct rect));
  x = g_malloc((nobs+1)*sizeof(int));
  y = g_malloc((nobs+1)*sizeof(int));
  c=0;
  itp = ucl_object_iterate_new(arr);
  while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
  {
    ptr = ucl_object_lookup(iter,"id");
    if(ucl_object_type(ptr) == UCL_INT)
    {
      if(ucl_object_toint(ptr) == wid)
        win = parse_rect(iter);
      else
      {
        if(c<nobs)
        {
          obs[c] = parse_rect(iter);
          x[c] = obs[c].x+obs[c].w;
          y[c] = obs[c].y+obs[c].h;
          c++;
        }
      }
    }
  }

  ucl_object_iterate_free(itp);

  xoff = (win.x*2+win.w)/2 - (output.x*2+output.w)/2;
  yoff = (win.y*2+win.h)/2 - (output.y*2+output.h)/2;
  if((xoff<-1)||(xoff>1)||(yoff<-1)||(yoff>1)||(c!=nobs))
  {
    g_free(obs);
    g_free(x);
    g_free(y);
    return -1;
  }
  x[c]=output.x;
  y[c]=output.y;
  qsort(x,nobs+1,sizeof(int),comp_int);
  qsort(y,nobs+1,sizeof(int),comp_int);

  *r=win;
  win.x = output.x+context->wo_x*output.w/100;
  win.y = output.y+context->wo_y*output.h/100;
  do
  {
    success=1;
    for(c=0;c<nobs;c++)
    {
      if((win.x==obs[c].x) && (win.y==obs[c].y))
        success=0;
    }
    if (success==1)
    {
      *r=win;
      break;
    }
    win.x += output.w*context->wp_x/100;
    win.y += output.h*context->wp_y/100;;
  } while((win.x+win.w)<(output.x+output.w) && (win.y+win.h)<(output.y+output.h));

  for(j=nobs;j>=0;j--)
    for(i=nobs;i>=0;i--)
    {
      success=1;
      for(c=0;c<nobs;c++)
        if(!(((x[i]+win.w-1)<obs[c].x)||(x[i]>(obs[c].x+obs[c].w-1))||
            ((y[j]+win.h-1)<obs[c].y)||(y[j]>(obs[c].y+obs[c].h-1))))
          success=0;
      if((x[i]<output.x)||(x[i]+win.w>output.x+output.w)||
          (y[j]<output.y)||(y[j]+win.h>output.x+output.h))
        success=0;
      if(success==1)
      {
        win.x = x[i];
        win.y = y[j];
        *r = win;
      }
    }
  g_free(x);
  g_free(y);
  g_free(obs);
  return 1;
}

const ucl_object_t *placement_find_wid ( const ucl_object_t *obj, gint64 wid )
{
  const ucl_object_t *ptr,*iter,*arr, *ret;
  ucl_object_iter_t *itp;
  arr= ucl_object_lookup(obj,"floating_nodes");
  if( arr )
  {
    itp = ucl_object_iterate_new(arr);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
    {
      ptr = ucl_object_lookup(iter,"id");
      if(ucl_object_type(ptr)==UCL_INT)
        if(ucl_object_toint(ptr) == wid)
          return obj;
    }
    ucl_object_iterate_free(itp);
  }
  ret = NULL;
  arr = ucl_object_lookup(obj,"nodes");
  if( arr )
  {
    itp = ucl_object_iterate_new(arr);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
    {
      if(ret==NULL)
        ret = placement_find_wid(iter,wid);
    }
    ucl_object_iterate_free(itp);
  }
  return ret;
}

void place_window ( gint64 wid, gint64 pid, struct context *context )
{
  gint sock;
  gint32 etype;
  gchar buff[256];
  struct rect r;
  const ucl_object_t *obj, *node;
  struct ucl_parser *parse;
  GList *iter;
  gchar *response;

  if(context->features & F_PL_CHKPID)
    for(iter=context->wt_list;iter!=NULL;iter=g_list_next(iter))
      if(AS_WINDOW(iter->data)->pid==pid)
        return;

  sock = sway_ipc_open(3000);
  if(sock==-1)
    return;
  sway_ipc_send(sock,4,"");
  response = sway_ipc_poll(sock,&etype);
  close(sock);
  if(response==NULL)
    return;
  parse = ucl_parser_new(0);
  ucl_parser_add_string(parse,response,strlen(response));
  obj = ucl_parser_get_object(parse);
  node = placement_find_wid ( obj, wid );
  placement_location(context,node,wid,&r);
  snprintf(buff,255,"[con_id=%ld] move absolute position %d %d",wid,r.x,r.y);
  if(context->ipc != -1)
    sway_ipc_send(context->ipc,0,buff);
  ucl_object_unref((ucl_object_t *)obj);
  ucl_parser_free(parse);
  g_free(response);
}
