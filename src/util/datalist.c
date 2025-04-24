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
  dlist->refcount++;
  return dlist;
}

void datalist_unref ( datalist_t *dlist )
{
  dlist->refcount--;
  if(!(dlist->refcount))
  {
    g_datalist_clear(&dlist->data);
    g_free(dlist);
  }
}
