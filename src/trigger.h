#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include <glib.h>
#include "vm/vm.h"

typedef void(*trigger_func_t)(void *, vm_store_t *);

const gchar *trigger_add ( gchar *name, trigger_func_t func, void *data );
void trigger_remove ( gchar *name, trigger_func_t func, void *data );
void trigger_action_cb ( vm_closure_t *closure, vm_store_t *store );
const gchar *trigger_name_intern  ( gchar *name );
void trigger_emit_with_data ( gchar *name, vm_store_t *store );
void trigger_emit ( gchar *name );

#endif
