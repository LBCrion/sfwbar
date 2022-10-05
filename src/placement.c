/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include <gdk/gdk.h>
#include <json.h>

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

gboolean placer_check ( gboolean *cpid )
{
  *cpid = check_pid;
  return placer;
}

int comp_int ( const void *x1, const void *x2)
{
  return ( *(int*)x1 - *(int*)x2 );
}

void placer_calc ( gint nobs, GdkRectangle *obs, GdkRectangle output,
    GdkRectangle *win )
{
  gint xoff,yoff;
  gint *x,*y;
  gint i,j,c;
  gboolean success;

  xoff = (win->x*2+win->width)/2 - (output.x*2+output.width)/2;
  yoff = (win->y*2+win->height)/2 - (output.y*2+output.height)/2;
  if((xoff<-1)||(xoff>1)||(yoff<-1)||(yoff>1))
    return;

  x = g_malloc((nobs+1)*sizeof(int));
  y = g_malloc((nobs+1)*sizeof(int));
  for(c=0;c<nobs;c++)
  {
    x[c] = obs[c].x + obs[c].width;
    y[c] = obs[c].y + obs[c].height;
  }
  x[c]=output.x;
  y[c]=output.y;
  qsort(x,nobs+1,sizeof(int),comp_int);
  qsort(y,nobs+1,sizeof(int),comp_int);

  win->x = output.x+x_origin*output.width/100;
  win->y = output.y+y_origin*output.height/100;
  do
  {
    success=TRUE;
    for(c=0;c<nobs;c++)
      if((win->x==obs[c].x) && (win->y==obs[c].y))
        success=FALSE;
    if (success)
      break;
    win->x += output.width*x_step/100;
    win->y += output.height*y_step/100;;
  } while((win->x+win->width)<(output.x+output.width) &&
      (win->y+win->height)<(output.y+output.height));

  for(j=nobs;j>=0;j--)
    for(i=nobs;i>=0;i--)
    {
      success=TRUE;
      for(c=0;c<nobs;c++)
        if(!(((x[i]+win->width-1)<obs[c].x) ||
              (x[i]>(obs[c].x+obs[c].width-1)) ||
              ((y[j]+win->height-1)<obs[c].y) ||
              (y[j]>(obs[c].y+obs[c].height-1))))
          success=FALSE;
      if((x[i]<output.x)||(x[i]+win->width>output.x+output.width)||
          (y[j]<output.y)||(y[j]+win->height>output.y+output.height))
        success=FALSE;
      if(success)
      {
        win->x = x[i];
        win->y = y[j];
      }
    }
  g_free(x);
  g_free(y);
}
