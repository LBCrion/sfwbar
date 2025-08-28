/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2025- sfwbar maintainers
 */

#include "util/datalist.h"

datalist_t *datalist_new ( void )
{
  datalist_t *dlist;

  dlist = g_malloc0(sizeof(datalist_t));
  g_datalist_init(&dlist->data);
  dlist->refcount = 1;

  return dlist;
}

datalist_t *datalist_ref ( datalist_t *dlist )
{
  g_mutex_lock(&dlist->mutex);
  dlist->refcount++;
  g_mutex_unlock(&dlist->mutex);

  return dlist;
}

void datalist_unref ( datalist_t *dlist )
{
  g_mutex_lock(&dlist->mutex);
  dlist->refcount--;
  if(!(dlist->refcount))
  {
    g_datalist_clear(&dlist->data);
    g_free(dlist);
  }
  g_mutex_unlock(&dlist->mutex);
}

gpointer datalist_get ( datalist_t *dlist, GQuark id )
{
  gpointer result;

  g_mutex_lock(&dlist->mutex);
  result = g_datalist_id_get_data(&dlist->data, id);
  g_mutex_unlock(&dlist->mutex);

  return result;
}

void datalist_set ( datalist_t *dlist, GQuark id, gpointer d )
{
  g_mutex_lock(&dlist->mutex);
  g_datalist_id_set_data(&dlist->data, id, d);
  g_mutex_unlock(&dlist->mutex);
}

void datalist_foreach ( datalist_t *dlist, GDataForeachFunc func, void *d )
{
  g_mutex_lock(&dlist->mutex);
  g_datalist_foreach(&dlist->data, func, d);
  g_mutex_unlock(&dlist->mutex);
}
