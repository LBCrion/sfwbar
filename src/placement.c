/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include <glib.h>
#include <unistd.h>
#include <json.h>
#include "sfwbar.h"
#include "sway_ipc.h"
#include "wintree.h"

static gint x_step, y_step, x_origin, y_origin;
static gboolean check_pid;
static gboolean placer;

void placer_config ( gint xs, gint ys, gint xo, gint yo, gboolean pid )
{
  x_step = MAX(1,xs);
  y_step = MAX(1,ys);
  x_origin = xo;
  y_origin = yo;
  check_pid = !pid;
  placer = TRUE;
}

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

int placement_location ( struct json_object*obj, gint64 wid, GdkRectangle *r )
{
  GdkRectangle output, win, *obs;
  struct json_object *ptr,*item,*arr;
  gint c,i,j,nobs,success;
  gint *x, *y;
  gint xoff,yoff;
  output = sway_ipc_parse_rect(obj);
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
        win = sway_ipc_parse_rect(item);
      else
      {
        if(c<nobs)
        {
          obs[c] = sway_ipc_parse_rect(item);
          x[c] = obs[c].x+obs[c].width;
          y[c] = obs[c].y+obs[c].height;
          c++;
        }
      }
    }
  }
  xoff = (win.x*2+win.width)/2 - (output.x*2+output.width)/2;
  yoff = (win.y*2+win.height)/2 - (output.y*2+output.height)/2;
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
  win.x = output.x+x_origin*output.width/100;
  win.y = output.y+y_origin*output.height/100;
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
    win.x += output.width*x_step/100;
    win.y += output.height*y_step/100;;
  } while((win.x+win.width)<(output.x+output.width) &&
      (win.y+win.height)<(output.y+output.height));

  for(j=nobs;j>=0;j--)
    for(i=nobs;i>=0;i--)
    {
      success=1;
      for(c=0;c<nobs;c++)
        if(!(((x[i]+win.width-1)<obs[c].x)||(x[i]>(obs[c].x+obs[c].width-1))||
            ((y[j]+win.height-1)<obs[c].y)||(y[j]>(obs[c].y+obs[c].height-1))))
          success=0;
      if((x[i]<output.x)||(x[i]+win.width>output.x+output.width)||
          (y[j]<output.y)||(y[j]+win.height>output.y+output.height))
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
      {
        item = json_object_array_get_idx(arr,i);
        ret = placement_find_wid(item,wid);
        if(ret)
          break;
      }
  return ret;
}

void place_window ( gint64 wid, gint64 pid )
{
  gint sock;
  gint32 etype;
  GdkRectangle r;
  struct json_object *obj, *node;
  gchar *response;

  if(!placer)
    return;

  if(check_pid && wintree_from_pid(pid))
    return;

  if(!sway_ipc_active())
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
    if(node && placement_location(node,wid,&r)!=-1)
      sway_ipc_command("[con_id=%ld] move absolute position %d %d",
          wid,r.x,r.y);
    json_object_put(obj);
  }
  g_free(response);
}
