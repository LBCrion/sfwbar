#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include <glib.h>
#include "vm/vm.h"

const gchar *trigger_add ( gchar *name, GSourceFunc func, void *data );
void trigger_remove ( gchar *name, GSourceFunc func, void *data );
const gchar *trigger_name_intern  ( gchar *name );
void trigger_emit ( gchar *name );

#endif
