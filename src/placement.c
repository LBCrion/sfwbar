/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <glib.h>
#include <unistd.h>
#include <json.h>
#include "sfwbar.h"
#include <fcntl.h>

int comp_int ( const void *x1, const void *x2)
{
  return ( *(int*)x1 - *(int*)x2 );
}

struct rect parse_rect ( struct json_object *obj )
{
  struct json_object *rect;
  struct rect ret;
  json_object_object_get_ex(obj,"rect",&rect);

  ret.x = json_int_by_name(rect,"x",0);
  ret.y = json_int_by_name(rect,"y",0);
  ret.w = json_int_by_name(rect,"width",0);
  ret.h = json_int_by_name(rect,"height",0);

  return ret;
}

int placement_location ( struct json_object*obj, gint64 wid, struct rect *r )
{
  struct rect output, win, *obs;
  struct json_object *ptr,*item,*arr;
  gint c,i,j,nobs,success;
  gint *x, *y;
  gint xoff,yoff;
  output = parse_rect(obj);
  win = output;

  if(!json_object_object_get_ex(obj,"floating_nodes",&arr))
    return -1;
  if(!json_object_is_type(arr,json_type_array))
    return -1;
  nobs = json_object_array_length(arr)-1;
  obs = g_malloc(nobs*sizeof(struct rect));
  x = g_malloc((nobs+1)*sizeof(int));
  y = g_malloc((nobs+1)*sizeof(int));
  c=0;
  for(i=0;i<=nobs;i++)
  {
    item = json_object_array_get_idx(arr,i);
    json_object_object_get_ex(item,"id",&ptr);
    if(json_object_is_type(ptr,json_type_int))
    {
      if(json_object_get_int64(ptr) == wid)
        win = parse_rect(item);
      else
      {
        if(c<nobs)
        {
          obs[c] = parse_rect(item);
          x[c] = obs[c].x+obs[c].w;
          y[c] = obs[c].y+obs[c].h;
          c++;
        }
      }
    }
  }
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
          (y[j]<output.y)||(y[j]+win.h>output.y+output.h))
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

struct json_object *placement_find_wid ( struct json_object *obj, gint64 wid )
{
  json_object *ptr,*item,*arr, *ret;
  gint i;

  if(json_object_object_get_ex(obj,"floating_nodes",&arr))
    if(json_object_is_type(arr, json_type_array))
      for(i=0;i<json_object_array_length(arr);i++)
      {
        item = json_object_array_get_idx(arr,i);
        json_object_object_get_ex(item,"id",&ptr);
        if(json_object_is_type(ptr,json_type_int))
          if(json_object_get_int64(ptr) == wid)
            return obj;
      }
  ret = NULL;
  if(json_object_object_get_ex(obj,"nodes",&arr))
    if(json_object_is_type(arr, json_type_array))
      for(i=0;i<json_object_array_length(arr);i++)
        if(ret==NULL)
      {
          item = json_object_array_get_idx(arr,i);
          ret = placement_find_wid(item,wid);
      }
  return ret;
}

void place_window ( gint64 wid, gint64 pid )
{
  gint sock;
  gint32 etype;
  gchar *cmd;
  struct rect r;
  struct json_object *obj, *node;
  GList *iter;
  gchar *response;

  if(context->features & F_PL_CHKPID)
    for(iter=context->wt_list;iter!=NULL;iter=g_list_next(iter))
      if(AS_WINDOW(iter->data)->pid==pid)
        return;

  if(context->ipc == -1)
    return;
  sock = sway_ipc_open(3000);
  if(sock==-1)
    return;
  sway_ipc_send(sock,4,"");
  response = sway_ipc_poll(sock,&etype);
  close(sock);

  if(!response)
    return;
  obj = json_tokener_parse(response);
  if(obj)
  {
    node = placement_find_wid ( obj, wid );
    if(node!=NULL)
    {
      if(placement_location(node,wid,&r)!=-1)
      {
        cmd = g_strdup_printf("[con_id=%ld] move absolute position %d %d",
            wid,r.x,r.y);
        sway_ipc_send(context->ipc,0,cmd);
        g_free(cmd);
      }
      json_object_put(obj);
    }
  }
  g_free(response);
}
