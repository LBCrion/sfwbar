#ifndef __DATALIST_H__
#define __DATALIST_H__

#include <glib.h>

typedef struct _datalist_t {
  GData *data;
  gint refcount;
} datalist_t;

datalist_t *datalist_new ( void );
datalist_t *datalist_ref ( datalist_t *dlist );
void datalist_unref ( datalist_t *dlist );

#endif
