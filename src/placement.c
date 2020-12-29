/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <glib.h>
#include <json.h>
#include "sfwbar.h"

int comp_int ( const void *x1, const void *x2)
{
  return ( *(int*)x1 - *(int*)x2 );
}

struct rect parse_rect ( json_object *obj )
{
  json_object *rect;
  struct rect ret;
  json_object_object_get_ex(obj,"rect",&rect);
  ret.x = json_int_by_name(rect,"x",0);
  ret.y = json_int_by_name(rect,"y",0);
  ret.w = json_int_by_name(rect,"width",0);
  ret.h = json_int_by_name(rect,"height",0);

  return ret;
}

int placement_location ( struct context *context, json_object *obj, gint64 pid, struct rect *r )
{
  struct rect output, win, *obs;
  json_object *ptr,*iter,*arr;
  int c,i,j,nobs,success;
  int *x, *y;
  output = parse_rect(obj);

  json_object_object_get_ex(obj,"floating_nodes",&arr);
  if( !json_object_is_type(arr, json_type_array))
    return -1;
  nobs = json_object_array_length(arr)-1;
  obs = g_malloc(nobs*sizeof(struct rect));
  x = g_malloc((nobs+1)*sizeof(int));
  y = g_malloc((nobs+1)*sizeof(int));
  c=0;
  for(i=0;i<json_object_array_length(arr);i++)
  {
    iter = json_object_array_get_idx(arr,i);
    json_object_object_get_ex(iter,"pid",&ptr);
    if(json_object_is_type(ptr,json_type_int))
    {
      if(json_object_get_int64(ptr) == pid)
        win = parse_rect(iter);
      else
      {
        if(c>=nobs)
          return -1;
        obs[c] = parse_rect(iter);
        x[c] = obs[c].x+obs[c].w;
        y[c] = obs[c].y+obs[c].h;
        c++;
      }
    }
  }
  if(c!=nobs)
    return -1;
  x[c]=output.x;
  y[c]=output.y;
  qsort(x,nobs+1,sizeof(int),comp_int);
  qsort(y,nobs+1,sizeof(int),comp_int);

  *r=win;
  win.x = output.x;
  win.y = output.y;
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

json_object *placement_find_pid ( json_object *obj, gint64 pid )
{
  json_object *ptr,*iter,*arr, *ret;
  int i;
  json_object_object_get_ex(obj,"floating_nodes",&arr);
  if( json_object_is_type(arr, json_type_array))
    for(i=0;i<json_object_array_length(arr);i++)
    {
      iter = json_object_array_get_idx(arr,i);
      json_object_object_get_ex(iter,"pid",&ptr);
      if(json_object_is_type(ptr,json_type_int))
        if(json_object_get_int(ptr) == pid)
          return obj;
    }
  json_object_object_get_ex(obj,"nodes",&arr);
  if( json_object_is_type(arr, json_type_array))
    for(i=0;i<json_object_array_length(arr);i++)
    {
      ret = placement_find_pid(json_object_array_get_idx(arr,i),pid);
      if (ret != NULL)
        return ret;
    }
  return NULL;
}

void place_window ( gint64 pid, struct context *context )
{
  int sock;
  gint32 etype;
  char buff[256];
  struct rect r;
  json_object *obj, *node;
  sock = ipc_open(3000);
  ipc_send(sock,4,"");
  obj = ipc_poll(sock,&etype);
  node = placement_find_pid ( obj, pid );
  placement_location(context,node,pid,&r);
  json_object_put(obj);
  snprintf(buff,255,"[pid=%ld] move absolute position %d %d",pid,r.x,r.y);
  ipc_send(context->ipc,0,buff);
  close(sock);
}
