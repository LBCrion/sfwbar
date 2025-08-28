#ifndef __DATALIST_H__
#define __DATALIST_H__

#include <glib.h>

typedef struct _datalist_t {
  GData *data;
  GMutex mutex;
  gint refcount;
} datalist_t;

datalist_t *datalist_new ( void );
datalist_t *datalist_ref ( datalist_t *dlist );
void datalist_unref ( datalist_t *dlist );
gpointer datalist_get ( datalist_t *dlist, GQuark id );
void datalist_set ( datalist_t *dlist, GQuark id, gpointer d );
void datalist_foreach ( datalist_t *dlist, GDataForeachFunc func, void *d );

#endif
